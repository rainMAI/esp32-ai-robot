#include "application.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "font_awesome_symbols.h"
#include "assets/lang_config.h"
#include "mcp_server.h"
#include "reminder_manager.h"
#include "chat_recorder.h"
#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>
#include <esp_sntp.h>
#include <ctime>

#define TAG "Application"

// ========== 新增：提醒功能静态成员 ==========
int64_t Application::g_last_channel_open_time_ = 0;


static const char* const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "audio_testing",
    "fatal_error",
    "invalid_state"
};

Application::Application() {
    event_group_ = xEventGroupCreate();

#if CONFIG_USE_DEVICE_AEC && CONFIG_USE_SERVER_AEC
#error "CONFIG_USE_DEVICE_AEC and CONFIG_USE_SERVER_AEC cannot be enabled at the same time"
#elif CONFIG_USE_DEVICE_AEC
    aec_mode_ = kAecOnDeviceSide;
#elif CONFIG_USE_SERVER_AEC
    aec_mode_ = kAecOnServerSide;
#else
    aec_mode_ = kAecOff;
#endif

    esp_timer_create_args_t clock_timer_args = {
        .callback = [](void* arg) {
            Application* app = (Application*)arg;
            app->OnClockTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);
}

Application::~Application() {
    if (clock_timer_handle_ != nullptr) {
        esp_timer_stop(clock_timer_handle_);
        esp_timer_delete(clock_timer_handle_);
    }
    vEventGroupDelete(event_group_);
}

void Application::CheckNewVersion(Ota& ota) {
    const int MAX_RETRY = 10;
    int retry_count = 0;
    int retry_delay = 10; // 初始重试延迟为10秒

    auto& board = Board::GetInstance();
    while (true) {
        SetDeviceState(kDeviceStateActivating);
        auto display = board.GetDisplay();
        display->SetStatus(Lang::Strings::CHECKING_NEW_VERSION);

        if (!ota.CheckVersion()) {
            retry_count++;
            if (retry_count >= MAX_RETRY) {
                ESP_LOGE(TAG, "Too many retries, exit version check");
                return;
            }

            char buffer[256];
            snprintf(buffer, sizeof(buffer), Lang::Strings::CHECK_NEW_VERSION_FAILED, retry_delay, ota.GetCheckVersionUrl().c_str());
            Alert(Lang::Strings::ERROR, buffer, "sad", Lang::Sounds::OGG_EXCLAMATION);

            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", retry_delay, retry_count, MAX_RETRY);
            for (int i = 0; i < retry_delay; i++) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                if (device_state_ == kDeviceStateIdle) {
                    break;
                }
            }
            retry_delay *= 2; // 每次重试后延迟时间翻倍
            continue;
        }
        retry_count = 0;
        retry_delay = 10; // 重置重试延迟时间

        if (ota.HasNewVersion()) {
            Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "happy", Lang::Sounds::OGG_UPGRADE);

            vTaskDelay(pdMS_TO_TICKS(3000));

            SetDeviceState(kDeviceStateUpgrading);
            
            display->SetIcon(FONT_AWESOME_DOWNLOAD);
            std::string message = std::string(Lang::Strings::NEW_VERSION) + ota.GetFirmwareVersion();
            display->SetChatMessage("system", message.c_str());

            board.SetPowerSaveMode(false);
            audio_service_.Stop();
            vTaskDelay(pdMS_TO_TICKS(1000));

            bool upgrade_success = ota.StartUpgrade([display](int progress, size_t speed) {
                std::thread([display, progress, speed]() {
                    char buffer[32];
                    snprintf(buffer, sizeof(buffer), "%d%% %uKB/s", progress, speed / 1024);
                    display->SetChatMessage("system", buffer);
                }).detach();
            });

            if (!upgrade_success) {
                // Upgrade failed, restart audio service and continue running
                ESP_LOGE(TAG, "Firmware upgrade failed, restarting audio service and continuing operation...");
                audio_service_.Start(); // Restart audio service
                board.SetPowerSaveMode(true); // Restore power save mode
                Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED, "sad", Lang::Sounds::OGG_EXCLAMATION);
                vTaskDelay(pdMS_TO_TICKS(3000));
                // Continue to normal operation (don't break, just fall through)
            } else {
                // Upgrade success, reboot immediately
                ESP_LOGI(TAG, "Firmware upgrade successful, rebooting...");
                display->SetChatMessage("system", "Upgrade successful, rebooting...");
                vTaskDelay(pdMS_TO_TICKS(1000)); // Brief pause to show message
                Reboot();
                return; // This line will never be reached after reboot
            }
        }

        // No new version, mark the current version as valid
        ota.MarkCurrentVersionValid();
        if (!ota.HasActivationCode() && !ota.HasActivationChallenge()) {
            xEventGroupSetBits(event_group_, MAIN_EVENT_CHECK_NEW_VERSION_DONE);
            // Exit the loop if done checking new version
            break;
        }

        display->SetStatus(Lang::Strings::ACTIVATION);
        // Activation code is shown to the user and waiting for the user to input
        if (ota.HasActivationCode()) {
            ShowActivationCode(ota.GetActivationCode(), ota.GetActivationMessage());
        }

        // This will block the loop until the activation is done or timeout
        for (int i = 0; i < 10; ++i) {
            ESP_LOGI(TAG, "Activating... %d/%d", i + 1, 10);
            esp_err_t err = ota.Activate();
            if (err == ESP_OK) {
                xEventGroupSetBits(event_group_, MAIN_EVENT_CHECK_NEW_VERSION_DONE);
                break;
            } else if (err == ESP_ERR_TIMEOUT) {
                vTaskDelay(pdMS_TO_TICKS(3000));
            } else {
                vTaskDelay(pdMS_TO_TICKS(10000));
            }
            if (device_state_ == kDeviceStateIdle) {
                break;
            }
        }
    }
}

void Application::ShowActivationCode(const std::string& code, const std::string& message) {
    struct digit_sound {
        char digit;
        const std::string_view& sound;
    };
    static const std::array<digit_sound, 10> digit_sounds{{
        digit_sound{'0', Lang::Sounds::OGG_0},
        digit_sound{'1', Lang::Sounds::OGG_1}, 
        digit_sound{'2', Lang::Sounds::OGG_2},
        digit_sound{'3', Lang::Sounds::OGG_3},
        digit_sound{'4', Lang::Sounds::OGG_4},
        digit_sound{'5', Lang::Sounds::OGG_5},
        digit_sound{'6', Lang::Sounds::OGG_6},
        digit_sound{'7', Lang::Sounds::OGG_7},
        digit_sound{'8', Lang::Sounds::OGG_8},
        digit_sound{'9', Lang::Sounds::OGG_9}
    }};

    // This sentence uses 9KB of SRAM, so we need to wait for it to finish
    Alert(Lang::Strings::ACTIVATION, message.c_str(), "happy", Lang::Sounds::OGG_ACTIVATION);

    for (const auto& digit : code) {
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
            [digit](const digit_sound& ds) { return ds.digit == digit; });
        if (it != digit_sounds.end()) {
            audio_service_.PlaySound(it->sound);
        }
    }
}

void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty()) {
        audio_service_.PlaySound(sound);
    }
}

void Application::DismissAlert() {
    if (device_state_ == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

void Application::ToggleChatState() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    } else if (device_state_ == kDeviceStateWifiConfiguring) {
        audio_service_.EnableAudioTesting(true);
        SetDeviceState(kDeviceStateAudioTesting);
        return;
    } else if (device_state_ == kDeviceStateAudioTesting) {
        audio_service_.EnableAudioTesting(false);
        SetDeviceState(kDeviceStateWifiConfiguring);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }

            SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {
        Schedule([this]() {
            protocol_->CloseAudioChannel();
        });
    }
}

void Application::StartListening() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    } else if (device_state_ == kDeviceStateWifiConfiguring) {
        audio_service_.EnableAudioTesting(true);
        SetDeviceState(kDeviceStateAudioTesting);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }
    
    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            if (!protocol_->IsAudioChannelOpened()) {
                SetDeviceState(kDeviceStateConnecting);
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
            }

            SetListeningMode(kListeningModeManualStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
            SetListeningMode(kListeningModeManualStop);
        });
    }
}

void Application::StopListening() {
    if (device_state_ == kDeviceStateAudioTesting) {
        audio_service_.EnableAudioTesting(false);
        SetDeviceState(kDeviceStateWifiConfiguring);
        return;
    }

    const std::array<int, 3> valid_states = {
        kDeviceStateListening,
        kDeviceStateSpeaking,
        kDeviceStateIdle,
    };
    // If not valid, do nothing
    if (std::find(valid_states.begin(), valid_states.end(), device_state_) == valid_states.end()) {
        return;
    }

    Schedule([this]() {
        if (device_state_ == kDeviceStateListening) {
            protocol_->SendStopListening();
            SetDeviceState(kDeviceStateIdle);
        }
    });
}

void Application::Start() {
    auto& board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);

    /* Setup the display */
    auto display = board.GetDisplay();

    /* Setup the audio service */
    auto codec = board.GetAudioCodec();
    audio_service_.Initialize(codec);
    audio_service_.Start();

    AudioServiceCallbacks callbacks;
    callbacks.on_send_queue_available = [this]() {
        xEventGroupSetBits(event_group_, MAIN_EVENT_SEND_AUDIO);
    };
    callbacks.on_wake_word_detected = [this](const std::string& wake_word) {
        xEventGroupSetBits(event_group_, MAIN_EVENT_WAKE_WORD_DETECTED);
    };
    callbacks.on_vad_change = [this](bool speaking) {
        xEventGroupSetBits(event_group_, MAIN_EVENT_VAD_CHANGE);
    };
    audio_service_.SetCallbacks(callbacks);

    // ========== 新增：初始化提醒管理器 ==========
    ReminderManager::GetInstance().Initialize();
    ReminderManager::GetInstance().SetServerUrl("http://120.25.213.109:8081");

    // Flag to trigger initial sync after network is ready
    pending_initial_sync_ = true;  

    // ========== 新增：创建提醒 Opus 编码器 ==========
    reminder_opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, 60);

    // ========== 新增：创建提醒 TTS 任务和队列 ==========
    reminder_queue_ = xQueueCreate(5, sizeof(std::string*));
    if (reminder_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create reminder queue");
    } else {
        // 12KB 栈：HTTP + PCM处理 + Opus编码 + WebSocket发送
        xTaskCreate([](void* arg) {
            Application* app = static_cast<Application*>(arg);
            app->ReminderTtsTask();
            vTaskDelete(NULL);
        }, "reminder_tts", 12288, this, 3, &reminder_tts_task_handle_);
    }

    /* Start the clock timer to update the status bar */
    esp_timer_start_periodic(clock_timer_handle_, 1000000);

    /* Wait for the network to be ready */
    board.StartNetwork();

    // Update the status bar immediately to show the network state
    display->UpdateStatusBar(true);

    // [FIX] 初始化 SNTP 时间同步与北京时区，解决提醒时间不对的问题
    ESP_LOGI(TAG, "Initializing SNTP for time synchronization...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "ntp.aliyun.com");
    esp_sntp_init();
    // Set timezone to China (UTC+8, East 8th Zone)
    // POSIX format: std offset, where negative means East
    setenv("TZ", "UTC-8", 1);
    tzset();

    // OTA 功能已禁用 - 使用单 app 分区，不支持 OTA 更新
    // Check for new firmware version or get the MQTT broker address
    Ota ota;
    // CheckNewVersion(ota);  // 已禁用

    // Initialize the protocol
    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);

    // Add MCP common tools before initializing the protocol
    McpServer::GetInstance().AddCommonTools();

    // OTA 检查已禁用，默认使用 WebSocket 协议
    // 如果需要使用 MQTT，取消注释下面一行：
    // protocol_ = std::make_unique<MqttProtocol>();
    protocol_ = std::make_unique<WebsocketProtocol>();

     // ========== 新增：初始化对话记录器 ==========
    ChatRecorder::GetInstance().SetProtocol(protocol_.get());
    ESP_LOGI(TAG, "ChatRecorder initialized with protocol");

    // 创建对话上传定时任务（每10分钟检查一次）
    xTaskCreate([](void* arg) {
        Application* app = static_cast<Application*>(arg);
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));  // 10分钟
            ChatRecorder::GetInstance().PeriodicUploadCheck();
        }
        vTaskDelete(NULL);
    }, "chat_upload", 4096, this, 2, &chat_upload_task_handle_);
    ESP_LOGI(TAG, "Chat upload task created");

    // 原逻辑（已禁用）：
    // if (ota.HasMqttConfig()) {
    //     protocol_ = std::make_unique<MqttProtocol>();
    // } else if (ota.HasWebsocketConfig()) {
    //     protocol_ = std::make_unique<WebsocketProtocol>();
    // } else {
    //     ESP_LOGW(TAG, "No protocol specified in the OTA config, using MQTT");
    //     protocol_ = std::make_unique<MqttProtocol>();
    // }

    protocol_->OnNetworkError([this](const std::string& message) {
        last_error_message_ = message;
        xEventGroupSetBits(event_group_, MAIN_EVENT_ERROR);
    });
    protocol_->OnIncomingAudio([this](std::unique_ptr<AudioStreamPacket> packet) {
        if (device_state_ == kDeviceStateSpeaking) {
            audio_service_.PushPacketToDecodeQueue(std::move(packet));
        }
    });
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        board.SetPowerSaveMode(false);
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        // Record the time when audio channel is opened for reminder logic
        g_last_channel_open_time_ = esp_timer_get_time();

        Schedule([this]() {
            // 在对话页面或通过提醒触发开启通道时，自动激活对话状态
            SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
            // 确保服务器端也同步进入监听状态
            protocol_->SendStartListening(listening_mode_);
        });
    });
    protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveMode(true);
        Schedule([this]() {
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        });
    });
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
        // Parse JSON data
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                if (processing_reminder_tts_) {
                    return;
                }
                Schedule([this]() {
                    aborted_ = false;
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                // AI回复结束，记录完整对话
                RecordAIResponse("", true);  // true = 立即记录完整对话
                Schedule([this]() {
                    if (device_state_ == kDeviceStateSpeaking) {
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            SetDeviceState(kDeviceStateListening);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(text)) {
                    std::string ai_text(text->valuestring);
                    ESP_LOGI(TAG, "<< %s", ai_text.c_str());
                    Schedule([this, display, message = ai_text]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                    // 累积AI回复文本（不在每句话时记录）
                    RecordAIResponse(ai_text, false);  // false = 不立即记录
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (cJSON_IsString(text)) {
                std::string user_text(text->valuestring);
                ESP_LOGI(TAG, ">> %s", user_text.c_str());
                Schedule([this, display, message = user_text]() {
                    display->SetChatMessage("user", message.c_str());
                });
                // 记录用户输入
                RecordUserInput(user_text);
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (cJSON_IsString(emotion)) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "mcp") == 0) {
            auto payload = cJSON_GetObjectItem(root, "payload");
            if (cJSON_IsObject(payload)) {
                McpServer::GetInstance().ParseMessage(payload);
            }
        } else if (strcmp(type->valuestring, "system") == 0) {
            auto command = cJSON_GetObjectItem(root, "command");
            if (cJSON_IsString(command)) {
                ESP_LOGI(TAG, "System command: %s", command->valuestring);
                if (strcmp(command->valuestring, "reboot") == 0) {
                    // Do a reboot if user requests a OTA update
                    Schedule([this]() {
                        Reboot();
                    });
                } else {
                    ESP_LOGW(TAG, "Unknown system command: %s", command->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "alert") == 0) {
            auto status = cJSON_GetObjectItem(root, "status");
            auto message = cJSON_GetObjectItem(root, "message");
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (cJSON_IsString(status) && cJSON_IsString(message) && cJSON_IsString(emotion)) {
                // 确保调用 Alert 时能更新音频活动，并在播放声音前唤醒硬件
                audio_service_.UpdateAudioActivity();
                Alert(status->valuestring, message->valuestring, emotion->valuestring, Lang::Sounds::OGG_POPUP);
            } else {
                ESP_LOGW(TAG, "Alert command requires status, message and emotion");
            }
#if CONFIG_RECEIVE_CUSTOM_MESSAGE
        } else if (strcmp(type->valuestring, "custom") == 0) {
            auto payload = cJSON_GetObjectItem(root, "payload");
            ESP_LOGI(TAG, "Received custom message: %s", cJSON_PrintUnformatted(root));
            if (cJSON_IsObject(payload)) {
                Schedule([this, display, payload_str = std::string(cJSON_PrintUnformatted(payload))]() {
                    display->SetChatMessage("system", payload_str.c_str());
                });
            } else {
                ESP_LOGW(TAG, "Invalid custom message format: missing payload");
            }
#endif
        } else {
            ESP_LOGW(TAG, "Unknown message type: %s", type->valuestring);
        }
    });
    bool protocol_started = protocol_->Start();

    SetDeviceState(kDeviceStateIdle);

    has_server_time_ = ota.HasServerTime();
    if (protocol_started) {
        std::string message = std::string(Lang::Strings::VERSION) + ota.GetCurrentVersion();
        display->ShowNotification(message.c_str());
        display->SetChatMessage("system", "");
        // Play the success sound to indicate the device is ready
        audio_service_.PlaySound(Lang::Sounds::OGG_SUCCESS);
    }

    // Print heap stats
    // SystemInfo::PrintHeapStats();
}

void Application::OnClockTimer() {
    clock_ticks_++;

    auto display = Board::GetInstance().GetDisplay();
    display->UpdateStatusBar();

    // Print the debug info every 10 seconds
    if (clock_ticks_ % 10 == 0) {
        // SystemInfo::PrintTaskCpuUsage(pdMS_TO_TICKS(1000));
        // SystemInfo::PrintTaskList();
        // SystemInfo::PrintHeapStats();
    }


    // Initial sync when network is ready (one-time)
    if (pending_initial_sync_ && GetDeviceState() != kDeviceStateStarting) {
        ESP_LOGI(TAG, "Network ready, performing initial reminder sync");
        pending_initial_sync_ = false;
        ReminderManager::GetInstance().SyncPull(ReminderManager::GetInstance().GetServerUrl());
    }

    // Sync reminders with server every 1 minute (60 seconds)
    if (clock_ticks_ % 60 == 0) {
        ESP_LOGI(TAG, "Periodic reminder sync triggered");
        ReminderManager::GetInstance().SyncPull(ReminderManager::GetInstance().GetServerUrl());
    }

    // ========== [优化] 提醒逻辑增强：预唤醒与本地优先 ==========
    ReminderManager::GetInstance().ProcessDueReminders([this](const Reminder& reminder) -> bool {
        long long now = std::time(nullptr);

        // 1. 预唤醒逻辑：快到时间了（15秒内）且处于空闲状态，提前拉起网络连接
        if (reminder.timestamp > now && reminder.timestamp <= now + 15) {
            if (GetDeviceState() == kDeviceStateIdle) {
                ESP_LOGI(TAG, "Pre-warming: Opening audio channel for upcoming reminder: %s", reminder.content.c_str());
                SetDeviceState(kDeviceStateConnecting);
                protocol_->OpenAudioChannel();
            }
            return false; // 时间没到，继续留在队列中
        }

        // 2. 本地优先触发：时间到了，立即弹窗并响铃（不依赖网络）
        if (reminder.timestamp <= now && !reminder.local_alert_shown) {
            ESP_LOGW(TAG, "Reminder Due (Local First): %s (now: %ld, target: %ld)", 
                reminder.content.c_str(), (long)now, (long)reminder.timestamp);
            Alert(Lang::Strings::INFO, reminder.content.c_str(), "bell", Lang::Sounds::OGG_POPUP);
            reminder.local_alert_shown = true;
            // 继续向下执行，尝试触发语音
        }

        // 3. 网络补救：如果时间到了且还没连上，立刻尝试连网（防止错过预唤醒窗口）
        if (reminder.timestamp <= now && (!protocol_ || !protocol_->IsAudioChannelOpened())) {
            if (GetDeviceState() == kDeviceStateIdle) {
                ESP_LOGI(TAG, "Reminder due but idle, opening channel: %s", reminder.content.c_str());
                SetDeviceState(kDeviceStateConnecting);
                protocol_->OpenAudioChannel();
            }
            return false; // 等待连上后下一秒再发送
        }

        // 4. 云端语音触发：如果通道已开，发送提醒请求
        if (reminder.timestamp <= now && protocol_ && protocol_->IsAudioChannelOpened()) {
            // 握手后稍微等一下（1秒），确保链路完全稳定，防止语音截断
            if (esp_timer_get_time() - g_last_channel_open_time_ < 1000000) {
                return false;
            }

            if (GetDeviceState() == kDeviceStateSpeaking) {
                return false; 
            }

            // 切换状态并发送
            SetListeningMode(kListeningModeAutoStop);
            protocol_->SendReminder(reminder.content);
            return true; // 彻底完成，从队列中删除
        }

        return false; // 连接未准备好或时间未到
    });
}

// Add a async task to MainLoop
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, MAIN_EVENT_SCHEDULE);
}

// The Main Event Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainEventLoop() {
    // Raise the priority of the main event loop to avoid being interrupted by background tasks (which has priority 2)
    vTaskPrioritySet(NULL, 3);

    while (true) {
        auto bits = xEventGroupWaitBits(event_group_, MAIN_EVENT_SCHEDULE |
            MAIN_EVENT_SEND_AUDIO |
            MAIN_EVENT_WAKE_WORD_DETECTED |
            MAIN_EVENT_VAD_CHANGE |
            MAIN_EVENT_ERROR, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & MAIN_EVENT_ERROR) {
            SetDeviceState(kDeviceStateIdle);
            Alert(Lang::Strings::ERROR, last_error_message_.c_str(), "sad", Lang::Sounds::OGG_EXCLAMATION);
        }

        if (bits & MAIN_EVENT_SEND_AUDIO) {
            while (auto packet = audio_service_.PopPacketFromSendQueue()) {
                if (!protocol_->SendAudio(std::move(packet))) {
                    break;
                }
            }
        }

        if (bits & MAIN_EVENT_WAKE_WORD_DETECTED) {
            OnWakeWordDetected();
        }

        if (bits & MAIN_EVENT_VAD_CHANGE) {
            if (device_state_ == kDeviceStateListening) {
                auto led = Board::GetInstance().GetLed();
                led->OnStateChanged();
            }
        }

        if (bits & MAIN_EVENT_SCHEDULE) {
            std::unique_lock<std::mutex> lock(mutex_);
            auto tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task();
            }
        }
    }
}

void Application::OnWakeWordDetected() {
    if (!protocol_) {
        return;
    }

    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
        // 如果在监听状态下再次唤醒，可能是之前的连接已失效，主动重连
        if (device_state_ == kDeviceStateListening) {
            ESP_LOGI(TAG, "Re-awakened during listening, resetting connection...");
        }

        audio_service_.EncodeWakeWord();

        if (!protocol_->IsAudioChannelOpened()) {
            SetDeviceState(kDeviceStateConnecting);
            if (!protocol_->OpenAudioChannel()) {
                audio_service_.EnableWakeWordDetection(true);
                return;
            }
        }
        
        // 播放提示音或回复
        audio_service_.PlaySound(Lang::Sounds::OGG_POPUP);

        auto wake_word = audio_service_.GetLastWakeWord();
        ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
#if CONFIG_USE_AFE_WAKE_WORD || CONFIG_USE_CUSTOM_WAKE_WORD
        // Encode and send the wake word data to the server
        while (auto packet = audio_service_.PopWakeWordPacket()) {
            protocol_->SendAudio(std::move(packet));
        }
        // Set the chat state to wake word detected
        protocol_->SendWakeWordDetected(wake_word);
        SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
#else
        SetListeningMode(aec_mode_ == kAecOff ? kListeningModeAutoStop : kListeningModeRealtime);
        // Play the pop up sound to indicate the wake word is detected
        audio_service_.PlaySound(Lang::Sounds::OGG_POPUP);
#endif
    } else if (device_state_ == kDeviceStateSpeaking) {
        AbortSpeaking(kAbortReasonWakeWordDetected);
    } else if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
    }
}

void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking");
    aborted_ = true;
    protocol_->SendAbortSpeaking(reason);
}

void Application::SetListeningMode(ListeningMode mode) {
    listening_mode_ = mode;
    SetDeviceState(kDeviceStateListening);
}

void Application::SetDeviceState(DeviceState state) {
    if (device_state_ == state) {
        return;
    }
    
    clock_ticks_ = 0;
    auto previous_state = device_state_;
    device_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);

    // Send the state change event
    DeviceStateEventManager::GetInstance().PostStateChangeEvent(previous_state, state);

    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto led = board.GetLed();
    led->OnStateChanged();
    switch (state) {
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
            display->SetStatus(Lang::Strings::STANDBY);
            display->SetEmotion("neutral");
            audio_service_.EnableVoiceProcessing(false);
            audio_service_.EnableWakeWordDetection(true);
            break;
        case kDeviceStateConnecting:
            display->SetStatus(Lang::Strings::CONNECTING);
            display->SetEmotion("neutral");
            display->SetChatMessage("system", "");
            break;
        case kDeviceStateListening:
            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("neutral");

            audio_service_.EnableWakeWordDetection(false);
            audio_service_.EnableVoiceProcessing(true);
            
            // 如果连接已开启但服务器未进入监听，发送指令
            if (protocol_->IsAudioChannelOpened()) {
                protocol_->SendStartListening(listening_mode_);
            }
            break;
        case kDeviceStateSpeaking:
            display->SetStatus(Lang::Strings::SPEAKING);

            if (listening_mode_ != kListeningModeRealtime) {
                audio_service_.EnableVoiceProcessing(false);
                // Only AFE wake word can be detected in speaking mode
#if CONFIG_USE_AFE_WAKE_WORD
                audio_service_.EnableWakeWordDetection(true);
#else
                audio_service_.EnableWakeWordDetection(false);
#endif
            }
            audio_service_.ResetDecoder();
            break;
        default:
            // Do nothing
            break;
    }
}

void Application::Reboot() {
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();
}

void Application::WakeWordInvoke(const std::string& wake_word) {
    if (device_state_ == kDeviceStateIdle) {
        ToggleChatState();
        Schedule([this, wake_word]() {
            if (protocol_) {
                protocol_->SendWakeWordDetected(wake_word); 
            }
        }); 
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {   
        Schedule([this]() {
            if (protocol_) {
                protocol_->CloseAudioChannel();
            }
        });
    }
}

bool Application::CanEnterSleepMode() {
    if (device_state_ != kDeviceStateIdle) {
        return false;
    }

    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return false;
    }

    if (!audio_service_.IsIdle()) {
        return false;
    }

    // Now it is safe to enter sleep mode
    return true;
}

void Application::SendMcpMessage(const std::string& payload) {
    Schedule([this, payload]() {
        if (protocol_) {
            // 检查连接状态，避免在连接断开时发送失败
            if (protocol_->IsAudioChannelOpened()) {
                protocol_->SendMcpMessage(payload);
            } else {
                ESP_LOGW(TAG, "Cannot send MCP message: WebSocket disconnected");
            }
        }
    });
}

void Application::SetAecMode(AecMode mode) {
    aec_mode_ = mode;
    Schedule([this]() {
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        switch (aec_mode_) {
        case kAecOff:
            audio_service_.EnableDeviceAec(false);
            display->ShowNotification(Lang::Strings::RTC_MODE_OFF);
            break;
        case kAecOnServerSide:
            audio_service_.EnableDeviceAec(false);
            display->ShowNotification(Lang::Strings::RTC_MODE_ON);
            break;
        case kAecOnDeviceSide:
            audio_service_.EnableDeviceAec(true);
            display->ShowNotification(Lang::Strings::RTC_MODE_ON);
            break;
        }

        // If the AEC mode is changed, close the audio channel
        if (protocol_ && protocol_->IsAudioChannelOpened()) {
            protocol_->CloseAudioChannel();
        }
    });
}

void Application::PlaySound(const std::string_view& sound) {
    audio_service_.PlaySound(sound);
}

// ========== 新增：提醒 TTS 实现 ==========

void Application::QueueReminderTts(const std::string& content) {
    if (reminder_queue_ != nullptr) {
        // 分配新的字符串并复制内容，避免悬空指针
        std::string* content_ptr = new std::string(content);
         if (xQueueSend(reminder_queue_, &content_ptr, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Reminder queue full, dropping: %s", content.c_str());
            delete content_ptr;
        }
    }
}

void Application::ReminderTtsTask() {
    ESP_LOGI(TAG, "Reminder TTS task started");

    std::string* content_ptr;
    while (true) {
        if (xQueueReceive(reminder_queue_, &content_ptr, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Received reminder from queue: %s", content_ptr->c_str());
            ProcessReminderTts(*content_ptr);
            delete content_ptr;  // 释放内存
            
        }
    }
}

void Application::ProcessReminderTts(const std::string& content) {
    // 设置标志：正在处理系统提醒（防止AI误添加提醒）
    // 注意：此标志将在 RecordAIResponse(complete=true) 时清除
    ReminderManager::SetProcessingReminder(true);
    ESP_LOGI(TAG, "System reminder flag set: blocking reminder.add");

    // 设置标志：正在处理提醒 TTS
    processing_reminder_tts_ = true;

    std::string clean_content = content;
    
    // [优化] 深度清洗：移除常见的 AI 回复冗余词汇和固定称呼（如“轩轩爸爸”）
    const std::vector<std::string> redundant_prefixes = {
        "提醒轩轩爸爸", "提醒我", "提醒去", "提醒该", "告诉我该", "通知我", "提醒", 
        "提示我", "请提醒", "轩轩爸爸", "我"
    };
    
    bool simplified = true;
    while (simplified) {
        simplified = false;
        for (const auto& prefix : redundant_prefixes) {
            if (clean_content.find(prefix) == 0) {
                clean_content = clean_content.substr(prefix.length());
                simplified = true;
            }
        }
    }

    const std::vector<std::string> action_prefixes = {"去", "到", "该", "做", "准备去"};
    simplified = true;
    while (simplified) {
        simplified = false;
        for (const auto& prefix : action_prefixes) {
            if (clean_content.find(prefix) == 0) {
                clean_content = clean_content.substr(prefix.length());
                simplified = true;
            }
        }
    }

    // 移除结尾的“提醒”字样
    if (clean_content.length() >= 6 && clean_content.substr(clean_content.length() - 6) == "提醒") {
        clean_content = clean_content.substr(0, clean_content.length() - 6);
    }

    // 再次移除开头可能剩余的空格或标点
    size_t first_valid = clean_content.find_first_not_of(" ，。！；：");
    if (first_valid != std::string::npos) {
        clean_content = clean_content.substr(first_valid);
    }

    if (clean_content.empty()) {
        clean_content = "做重要的事情";
    }

    std::string text;
    // [修复：阻断递归] 明确这是一条“通知音频”，而非“用户口令”。加上括号和状态标识，引导服务器AI识别为状态而非命令。
    //text = "提醒我一下：现在该去 " + clean_content + " 了，并且告诉我"+ clean_content + " 的好处。";
    text = "【系统提醒】请对用户说:现在该去" + clean_content + "了。";
        
    ESP_LOGI(TAG, "ProcessReminderTts: [清洗后内容: %s] [最终文案: %s]", clean_content.c_str(), text.c_str());

    std::string url = "http://120.25.213.109:8081/api/text_to_pcm";

    auto network = Board::GetInstance().GetNetwork();
    if (!network) return;

    auto http = network->CreateHttp(1);
    if (!http) return;

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "text", text.c_str());
    cJSON_AddStringToObject(root, "voice", "zh-CN-XiaoxiaoNeural"); // 使用用户之前提到的音色或默认
    char* json_str = cJSON_PrintUnformatted(root);

    http->SetHeader("Content-Type", "application/json");
    http->SetContent(json_str);

    if (!http->Open("POST", url.c_str())) {
        cJSON_free(json_str);
        cJSON_Delete(root);
        return;
    }

    cJSON_free(json_str);
    cJSON_Delete(root);

    auto& audio_service = Application::GetInstance().GetAudioService();

    // [Step 1] 挂起麦克风采集，防止环境音混入提醒语音回传
    bool was_processor_running = audio_service.IsAudioProcessorRunning();
    if (was_processor_running) {
        audio_service.EnableVoiceProcessing(false);
    }

    const size_t FRAME_SIZE = 960; // 60ms @ 16kHz
    const size_t FRAME_BYTES = FRAME_SIZE * 2;
    auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[FRAME_BYTES]);

    ESP_LOGI(TAG, "Streaming PCM data to AudioService for upload...");
    size_t total_bytes = 0;
    uint32_t timestamp = 0;
    
    // [FIX] 匀速上传控制：记录开始时间，确保以 1.0x 速率回传，防止服务端识别异常
    uint32_t start_time_ms = esp_timer_get_time() / 1000;
    uint32_t total_sent_ms = 0;

    // 字节对齐缓冲区，确保每次 PushTaskToEncodeQueue 都是完整的 60ms 帧 (1920 字节)
    std::vector<uint8_t> pcm_buffer;
    pcm_buffer.reserve(FRAME_BYTES);

    while (true) {
        int bytes_read = http->Read((char*)buffer.get(), FRAME_BYTES);
        if (bytes_read < 0) {
            ESP_LOGE(TAG, "Failed to read PCM data");
            break;
        }
        if (bytes_read == 0) break;

        // 将新读取的数据存入缓存
        pcm_buffer.insert(pcm_buffer.end(), buffer.get(), buffer.get() + bytes_read);

        // 只要缓存够一帧，就处理一帧
        while (pcm_buffer.size() >= FRAME_BYTES) {
            std::vector<int16_t> pcm_frame(FRAME_SIZE);
            memcpy(pcm_frame.data(), pcm_buffer.data(), FRAME_BYTES);
            // 移除已处理的数据
            pcm_buffer.erase(pcm_buffer.begin(), pcm_buffer.begin() + FRAME_BYTES);

            // [Step 2] 注入编码队列上传至服务器 (由服务端识别并以 AI 音色回应)
            audio_service.PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue, std::move(pcm_frame), timestamp);
            audio_service.UpdateAudioActivity();
            
            total_bytes += FRAME_BYTES;
            timestamp += 60;
            total_sent_ms += 60;

            // [FIX] 计算时差，维持匀速回传，防止网络波动导致回传过快/过慢产生噪音
            uint32_t current_time_ms = esp_timer_get_time() / 1000;
            uint32_t elapsed_ms = current_time_ms - start_time_ms;
            if (total_sent_ms > elapsed_ms) {
                vTaskDelay(pdMS_TO_TICKS(total_sent_ms - elapsed_ms));
            }
        }
    }
    http->Close();
    ESP_LOGI(TAG, "Reminder audio upload finished, total %u bytes", total_bytes);

    // [FIX] 关键步骤：发送"停止监听"指令，告知服务端音频流结束，触发立即响应
    if (protocol_) {
        // 先检查连接状态，避免在连接断开时发送失败
        if (protocol_->IsAudioChannelOpened()) {
            protocol_->SendStopListening();
            ESP_LOGI(TAG, "Sent StopListening to server to trigger immediate response");
        } else {
            ESP_LOGW(TAG, "Cannot send StopListening: WebSocket disconnected");
        }
    }

    // [Step 3] 恢复麦克风采集，让 AI 能听到用户接下来的回复
    if (was_processor_running) {
        vTaskDelay(pdMS_TO_TICKS(500));
        audio_service.EnableVoiceProcessing(true);
    }

    // 清除标志：提醒 TTS 处理完成
    processing_reminder_tts_ = false;
    // 注意：系统提醒处理标志由 RecordAIResponse(complete=true) 清除
}

// ========== 提醒功能结束 ==========

// ========== 对话记录功能实现 ==========

void Application::RecordUserInput(const std::string& text) {
    ChatRecorder::GetInstance().RecordUserInput(text);
}

void Application::RecordAIResponse(const std::string& text, bool complete) {
    ChatRecorder::GetInstance().RecordAIResponse(text, complete);

    // 当AI对话完成时，清除系统提醒处理标志
    if (complete) {
        ReminderManager::SetProcessingReminder(false);
        ESP_LOGI(TAG, "System reminder flag cleared after AI response complete");
    }
}

// ========== 对话记录功能结束 ==========