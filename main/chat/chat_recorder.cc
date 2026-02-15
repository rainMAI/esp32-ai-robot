#include "chat_recorder.h"
#include <esp_log.h>
#include <chrono>
#include "esp_system.h"
#include "esp_network.h"
#include "system_info.h"

#define TAG "ChatRecorder"

// 静态常量，设置15条对话或10分钟就上传一次对话数据。可根据需要调整，现在设置为较小值以便测试，实际使用时可改为更大值
static const size_t DEFAULT_BATCH_SIZE = 15;
static const int64_t DEFAULT_UPLOAD_INTERVAL_MS = 10 * 60 * 1000;  // 10分钟

ChatRecorder& ChatRecorder::GetInstance() {
    static ChatRecorder instance;
    return instance;
}

ChatRecorder::ChatRecorder()
    : protocol_(nullptr)
    , batch_size_threshold_(DEFAULT_BATCH_SIZE)
    , last_upload_time_(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() - DEFAULT_UPLOAD_INTERVAL_MS)
    , upload_interval_ms_(DEFAULT_UPLOAD_INTERVAL_MS) {
    ESP_LOGI(TAG, "ChatRecorder initialized (batch_size=%d, interval=%lld ms, last_upload=%lld)",
             (int)batch_size_threshold_,
             (long long)upload_interval_ms_,
             (long long)last_upload_time_);
}

void ChatRecorder::SetProtocol(Protocol* protocol) {
    std::lock_guard<std::mutex> lock(mutex_);
    protocol_ = protocol;
    ESP_LOGI(TAG, "Protocol set: %p", protocol);
}

void ChatRecorder::RecordUserInput(const std::string& text) {
    if (text.empty()) {
        ESP_LOGW(TAG, "Empty user text, ignoring");
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 保存用户输入，并清空之前的AI文本
    pending_user_text_ = text;
    pending_ai_text_.clear();

    ESP_LOGI(TAG, "User input recorded: %s", text.c_str());
}

void ChatRecorder::RecordAIResponse(const std::string& text, bool complete) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 如果有待处理的用户输入
    if (!pending_user_text_.empty()) {
        if (complete) {
            // 完成记录：组合用户输入和累积的AI文本
            if (!pending_ai_text_.empty()) {
                ChatMessage msg(pending_user_text_, pending_ai_text_);
                buffer_.push_back(msg);

                ESP_LOGI(TAG, "Complete dialogue recorded (user: %s, ai: %s)",
                         pending_user_text_.c_str(), pending_ai_text_.c_str());

                // 清空待处理文本
                pending_user_text_.clear();
                pending_ai_text_.clear();

                // 检查是否需要上传
                if (ShouldUpload()) {
                    ESP_LOGI(TAG, "Batch size threshold reached, triggering upload");
                    DoUpload();
                } else {
                    ESP_LOGI(TAG, "Buffer size: %d/%d", (int)buffer_.size(), (int)batch_size_threshold_);
                }
            } else {
                ESP_LOGW(TAG, "Complete flag set but no AI text accumulated");
            }
        } else {
            // 累积模式：添加AI文本片段
            if (!text.empty()) {
                if (!pending_ai_text_.empty()) {
                    pending_ai_text_ += " ";  // 句子间添加空格
                }
                pending_ai_text_ += text;
                ESP_LOGI(TAG, "AI text accumulated: %s", pending_ai_text_.c_str());
            }
        }
    } else {
        ESP_LOGW(TAG, "No pending user text for AI response (complete=%d)", complete);
    }
}

bool ChatRecorder::ShouldUpload() const {
    // 条件1：缓冲区达到批量大小阈值
    if (buffer_.size() >= batch_size_threshold_) {
        return true;
    }

    // 条件2：距离上次上传超过时间间隔
    if (last_upload_time_ > 0) {
        int64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        if (current_time - last_upload_time_ >= upload_interval_ms_) {
            return true;
        }
    }

    return false;
}

void ChatRecorder::UploadBatch() {
    std::lock_guard<std::mutex> lock(mutex_);
    DoUpload();
}

void ChatRecorder::PeriodicUploadCheck() {
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    ESP_LOGI(TAG, "[PeriodicUploadCheck] called: buffer_size=%d, last_upload=%lld, interval=%lld, elapsed=%lld",
             (int)buffer_.size(), (long long)last_upload_time_, (long long)upload_interval_ms_,
             (long long)(current_time - last_upload_time_));

    if (buffer_.empty()) {
        ESP_LOGI(TAG, "[PeriodicUploadCheck] buffer empty, skipping");
        return;
    }

    // 检查时间条件
    if (last_upload_time_ > 0) {
        if (current_time - last_upload_time_ >= upload_interval_ms_) {
            ESP_LOGI(TAG, "Periodic upload triggered (time interval reached)");
            DoUpload();
        } else {
            ESP_LOGI(TAG, "[PeriodicUploadCheck] time not reached yet");
        }
    } else {
        ESP_LOGI(TAG, "[PeriodicUploadCheck] last_upload_time_ <= 0, skipping");
    }
}

void ChatRecorder::DoUpload() {
    if (buffer_.empty()) {
        ESP_LOGW(TAG, "Empty buffer, nothing to upload");
        return;
    }

    // 构建JSON
    std::string json_str = BuildUploadJson();

    ESP_LOGI(TAG, "Uploading %d dialogues via HTTP...", (int)buffer_.size());

    // 使用EspNetwork直接HTTP POST到web_server
    EspNetwork network;
    auto http = network.CreateHttp(0);

    std::string server_url = "http://120.25.213.109:8081/api/chats/batch";
    http->SetTimeout(30000);  // 设置30秒超时（跨网络请求需要更长时间）
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress());
    http->SetHeader("Content-Type", "application/json");
    http->SetContent(std::move(json_str));  // 设置请求体，避免使用分块传输

    if (!http->Open("POST", server_url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for chat upload");
        return;
    }

    int status_code = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    if (status_code == 200 || status_code == 201) {
        ESP_LOGI(TAG, "Chat upload completed successfully (status=%d)", status_code);

        // 解析响应验证成功
        cJSON* root = cJSON_Parse(response.c_str());
        if (root != nullptr) {
            cJSON* success = cJSON_GetObjectItem(root, "success");
            if (success != nullptr && cJSON_IsTrue(success)) {
                // 记录上传时间
                last_upload_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();

                // 清空缓冲区
                ClearBuffer();

                ESP_LOGI(TAG, "Upload completed and buffer cleared");
            } else {
                ESP_LOGE(TAG, "Upload failed (server returned success=false)");
            }
            cJSON_Delete(root);
        } else {
            // 即使解析失败,如果状态码是200也认为上传成功
            last_upload_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            ClearBuffer();
        }
    } else {
        ESP_LOGE(TAG, "HTTP chat upload failed with status: %d, response: %s",
                 status_code, response.c_str());
    }
}

std::string ChatRecorder::BuildUploadJson() {
    cJSON* root = cJSON_CreateObject();
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return "{}";
    }

    cJSON* messages_array = cJSON_CreateArray();
    if (messages_array == nullptr) {
        ESP_LOGE(TAG, "Failed to create JSON array");
        cJSON_Delete(root);
        return "{}";
    }

    for (const auto& msg : buffer_) {
        cJSON* item = cJSON_CreateObject();
        if (item == nullptr) {
            continue;
        }

        cJSON_AddStringToObject(item, "user_text", msg.user_text.c_str());
        cJSON_AddStringToObject(item, "ai_text", msg.ai_text.c_str());

        cJSON_AddItemToArray(messages_array, item);
    }

    cJSON_AddItemToObject(root, "messages", messages_array);

    char* json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str ? json_str : "{}");

    if (json_str) {
        cJSON_free(json_str);
    }
    cJSON_Delete(root);

    return result;
}

void ChatRecorder::ClearBuffer() {
    size_t cleared = buffer_.size();
    buffer_.clear();
    ESP_LOGI(TAG, "Buffer cleared (%d dialogues)", (int)cleared);
}
