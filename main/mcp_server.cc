/*
 * MCP Server Implementation
 * Reference: https://modelcontextprotocol.io/specification/2024-11-05
 */

#include "mcp_server.h"
#include <esp_log.h>
#include <esp_app_desc.h>
#include <algorithm>
#include <cstring>
#include <esp_pthread.h>
#include <time.h>
#include <cinttypes> // For PRId64

#include "application.h"
#include "display.h"
#include "display/eye_display.h"
#include "display/multi_animation_manager.h"
#include "board.h"
#include "reminder_manager.h"

#define TAG "MCP"

#define DEFAULT_TOOLCALL_STACK_SIZE 6144

McpServer::McpServer() {
}

McpServer::~McpServer() {
    for (auto tool : tools_) {
        delete tool;
    }
    tools_.clear();
}

void McpServer::AddCommonTools() {
    // To speed up the response time, we add the common tools to the beginning of
    // the tools list to utilize the prompt cache.
    // Backup the original tools list and restore it after adding the common tools.
    auto original_tools = std::move(tools_);
    auto& board = Board::GetInstance();

    AddTool("self.get_device_status",
        "Provides the real-time information of the device, including the current time (Unix timestamp), local time, timezone, status of the audio speaker, screen, battery, network, etc.\n"
        "Returns:\n"
        "- timestamp: Unix timestamp (absolute time)\n"
        "- local_time: Local time in device's timezone (e.g., '2026-01-18 21:06:15')\n"
        "- timezone: Device's timezone (e.g., 'UTC-7' for Thailand)\n"
        "\n"
        "Use this tool to answer questions about current device status (e.g. what is the current volume? what time is it?).",
        PropertyList(),
        [&board](const PropertyList& properties) -> ReturnValue {
            std::string status_json = board.GetDeviceStatusJson();
            cJSON* root = cJSON_Parse(status_json.c_str());
            if (root == nullptr) {
                root = cJSON_CreateObject();
            }

            // Add system information with timezone and sync status
            cJSON* system = cJSON_CreateObject();
            time_t now = time(nullptr);
            cJSON_AddNumberToObject(system, "timestamp", (double)now);

            // 添加本地时间字符串（而非 UTC 时间）
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            cJSON_AddStringToObject(system, "local_time", time_str);

            // 添加时区信息
            char* tz = getenv("TZ");
            cJSON_AddStringToObject(system, "timezone", tz ? tz : "UTC");

            cJSON_AddItemToObject(root, "system", system);

            char* json_str = cJSON_PrintUnformatted(root);
            std::string result(json_str);
            cJSON_free(json_str);
            cJSON_Delete(root);
            return result;
        });

    AddTool("self.audio_speaker.set_volume", 
        "Set the volume of the audio speaker. If the current volume is unknown, you must call `self.get_device_status` tool first and then call this tool.",
        PropertyList({
            Property("volume", kPropertyTypeInteger, 0, 100)
        }), 
        [&board](const PropertyList& properties) -> ReturnValue {
            auto codec = board.GetAudioCodec();
            codec->SetOutputVolume(properties["volume"].value<int>());
            return true;
        });
    
    auto backlight = board.GetBacklight();
    if (backlight) {
        AddTool("self.screen.set_brightness",
            "Set the brightness of the screen.",
            PropertyList({
                Property("brightness", kPropertyTypeInteger, 0, 100)
            }),
            [backlight](const PropertyList& properties) -> ReturnValue {
                uint8_t brightness = static_cast<uint8_t>(properties["brightness"].value<int>());
                backlight->SetBrightness(brightness, true);
                return true;
            });
    }

    auto display = board.GetDisplay();
    if (display && !display->GetTheme().empty()) {
        AddTool("self.screen.set_theme",
            "Set the theme of the screen. The theme can be `light` or `dark`.",
            PropertyList({
                Property("theme", kPropertyTypeString)
            }),
            [display](const PropertyList& properties) -> ReturnValue {
                display->SetTheme(properties["theme"].value<std::string>().c_str());
                return true;
            });
    }

    auto camera = board.GetCamera();
    if (camera) {
        AddTool("self.camera.take_photo",
            "Take a photo and explain it. Use this tool when the user asks you to identify or explain something they show to the robot.\n"
            "\n"
            "IMPORTANT: Before taking the photo, you MUST acknowledge the user's request with a friendly response such as:\n"
            "  - '好的，稍等我帮你看看' (Okay, wait a moment, let me take a look for you)\n"
            "  - '好的，我来看看这是什么' (Okay, let me see what this is)\n"
            "  - '没问题，我来帮你识别一下' (No problem, let me help you identify this)\n"
            "\n"
            "The photo capture process takes a few seconds, so always inform the user that you are working on it.\n"
            "\n"
            "Args:\n"
            "  `question`: The question that you want to ask about the photo.\n"
            "\n"
            "Return:\n"
            "  A JSON object that provides the photo information.\n"
            "\n"
            "Examples:\n"
            "  - User: '帮我看一下这是什么' -> You: '好的，稍等我帮你看看' (then call this tool)\n"
            "  - User: '这是什么' -> You: '好的，我来看看这是什么' (then call this tool)",
            PropertyList({
                Property("question", kPropertyTypeString)
            }),
            [camera](const PropertyList& properties) -> ReturnValue {
                if (!camera->Capture()) {
                    return "{\"success\": false, \"message\": \"Failed to capture photo\"}";
                }
                auto question = properties["question"].value<std::string>();
                return camera->Explain(question);
            });
    }

    // 提醒功能工具
    AddTool("self.reminder.add",
        "设置提醒。当用户要求设置提醒时，你必须调用此工具！\n"
        "\n"
        "参数：\n"
        "- content (必需): 提醒内容，不要包含'每天'、'每日'等词 (例如: '喝水', '睡觉', '看书')\n"
        "- time (必需): 时间，格式为 HH:MM (例如: '10:59', '22:50')\n"
        "- is_daily (可选): 是否为每日提醒，默认为 false。如果用户说'每天'、'每日'等关键词，设为 true\n"
        "\n"
        "重要规则 - 每日提醒:\n"
        "- **不要在 content 中包含'每天'、'每日'等词！** 只在 is_daily 参数中指定\n"
        "- 例如: '每天8点喝水' → content='喝水', is_daily=true (不是 content='每天喝水')\n"
        "- 例如: '每日10点看书' → content='看书', is_daily=true (不是 content='每日看书')\n"
        "\n"
        "如何使用：\n"
        "1. 用户说类似'10点59提醒我喝水'\n"
        "2. 提取时间: '10:59' 和内容: '喝水'\n"
        "3. 如果用户说'每天'，设置 is_daily=true\n"
        "4. 调用此工具: reminder.add(content='喝水', time='10:59')\n"
        "\n"
        "示例：\n"
        "- 用户: '10点59提醒我喝水' → reminder.add(content='喝水', time='10:59')\n"
        "- 用户: '晚上9点50提醒我睡觉' → reminder.add(content='睡觉', time='21:50')\n"
        "- 用户: '每天8点50分提醒我喝水' → reminder.add(content='喝水', time='08:50', is_daily=true)\n"
        "- 用户: '每日早上10点提醒我看书' → reminder.add(content='看书', time='10:00', is_daily=true)\n"
        "\n"
        "重要：你必须调用此工具。不要只回复文本！直接调用工具即可。",
        std::vector<Property>({
            Property("content", kPropertyTypeString),
            Property("time", kPropertyTypeString),
            Property("is_daily", kPropertyTypeBoolean, false)  // 可选参数，默认 false
        }),
        [](const PropertyList& properties) -> ReturnValue {
            // 检查是否正在处理系统提醒（防止AI在处理提醒时误添加）
            if (ReminderManager::IsProcessingReminder()) {
                ESP_LOGW(TAG, "System reminder in progress, rejecting add request from AI");
                return "{\"success\": false, \"message\": \"正在处理系统提醒，请勿重复添加\"}";
            }

            std::string content = properties["content"].value<std::string>();
            std::string time_str = properties["time"].value<std::string>();
            bool is_daily = properties["is_daily"].value<bool>();  // 默认为 false

            if (content.empty()) {
                ESP_LOGE(TAG, "Content is required");
                return "{\"success\": false, \"message\": \"提醒内容不能为空\"}";
            }

            if (time_str.empty()) {
                ESP_LOGE(TAG, "Time is required");
                return "{\"success\": false, \"message\": \"必须提供时间参数\"}";
            }

            ESP_LOGI(TAG, "Adding reminder with time: %s, content: %s, is_daily: %s",
                     time_str.c_str(), content.c_str(), is_daily ? "true" : "false");

            // 直接将时间和内容传给 ReminderManager，让它发送到服务器
            bool success = ReminderManager::GetInstance().AddReminderWithTime(content, time_str, is_daily);
            if (success) {
                ESP_LOGI(TAG, "Reminder added successfully: %s at %s", content.c_str(), time_str.c_str());
                return true;
            } else {
                ESP_LOGE(TAG, "Failed to add reminder: %s", content.c_str());
                return "{\"success\": false, \"message\": \"网络连接失败，无法添加提醒。请检查WiFi连接或稍后重试。\"}";
            }
        });

    AddTool("self.reminder.list", "List all active reminders.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto reminders = ReminderManager::GetInstance().GetReminders();
            cJSON* root = cJSON_CreateArray();
            for (const auto& reminder : reminders) {
                cJSON* item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "id", reminder.id.c_str());
                cJSON_AddNumberToObject(item, "timestamp", (double)reminder.timestamp);
                cJSON_AddStringToObject(item, "scheduled_time", reminder.scheduled_time.c_str());
                cJSON_AddStringToObject(item, "content", reminder.content.c_str());
                cJSON_AddItemToArray(root, item);
            }
            char* json_str = cJSON_PrintUnformatted(root);
            std::string result(json_str);
            cJSON_free(json_str);
            cJSON_Delete(root);
            return result;
        });

    AddTool("self.reminder.update", "Update an existing reminder by ID.\n"
        "Parameters:\n"
        "- id: The ID of the reminder to update (use self.reminder.list to get IDs)\n"
        "- content: The new content for the reminder\n"
        "- scheduled_time: Optional. The new time in HH:MM format (e.g., '08:50'). If not provided, only content is updated.",
        PropertyList({
            Property("id", kPropertyTypeString),
            Property("content", kPropertyTypeString),
            Property("scheduled_time", kPropertyTypeString)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            auto id_str = properties["id"].value<std::string>();
            auto new_content = properties["content"].value<std::string>();
            auto new_time = properties["scheduled_time"].value<std::string>();

            if (id_str.empty() || new_content.empty()) {
                ESP_LOGE(TAG, "ID and content are required for update");
                return false;
            }

            // Parse ID as integer
            int reminder_id = -1;
            try {
                reminder_id = std::stoi(id_str);
            } catch (...) {
                ESP_LOGE(TAG, "Invalid reminder ID: %s", id_str.c_str());
                return false;
            }

            // Call UpdateRemote with optional time parameter and then sync
            if (ReminderManager::GetInstance().UpdateRemote(reminder_id, new_content, new_time)) {
                // Sync from server to get the updated list
                ReminderManager::GetInstance().SyncPull(ReminderManager::GetInstance().GetServerUrl());
                if (!new_time.empty()) {
                    ESP_LOGI(TAG, "Reminder updated: id=%d, new_content=%s, new_time=%s", reminder_id, new_content.c_str(), new_time.c_str());
                } else {
                    ESP_LOGI(TAG, "Reminder updated: id=%d, new_content=%s", reminder_id, new_content.c_str());
                }
                return true;
            }

            ESP_LOGE(TAG, "Failed to update reminder: id=%d", reminder_id);
            return false;
        });

    AddTool("self.reminder.refresh", "Refresh reminders from server to get latest updates.\n"
        "Use this after making changes on the web interface to sync immediately.\n"
        "Returns: true on success, or error message JSON on failure.\n"
        "No parameters required.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            ESP_LOGI(TAG, "Manual reminder refresh triggered");
            std::string server_url = ReminderManager::GetInstance().GetServerUrl();
            bool success = ReminderManager::GetInstance().SyncPull(server_url);
            if (success) {
                ESP_LOGI(TAG, "Reminder refresh completed successfully");
                return true;
            }
            ESP_LOGE(TAG, "Reminder refresh failed");
            // Return descriptive error message instead of just false
            return "{\"success\": false, \"message\": \"网络连接失败，无法从服务器刷新提醒。请检查WiFi连接或稍后重试。\"}";
        });

    AddTool("self.reminder.remove", "Remove a reminder by ID.",
        PropertyList({
            Property("id", kPropertyTypeString)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            auto id = properties["id"].value<std::string>();
            return ReminderManager::GetInstance().RemoveReminder(id);
        });

    // 眼睛主题切换工具（已禁用动画模式，只保留基础主题）
    AddTool("self.eye.set_theme",
        "Change the basic eye theme style (static rendering).\n"
        "Available themes:\n"
        "  - 'xingkong': Starry sky theme (default, dreamy purple-blue)\n"
        "  - 'shuimu': Ink painting theme (elegant black-white style)\n"
        "  - 'keji': Technology theme (futuristic cyan style)\n"
        "Note: For animated expressions, use 'self.eye.set_expression' instead.\n"
        "Args:\n"
        "  `theme`: Theme name (string)\n"
        "Examples:\n"
        "  - 'Change to starry sky eyes' -> theme='xingkong'\n"
        "  - 'Switch to technology style' -> theme='keji'",
        PropertyList({
            Property("theme", kPropertyTypeString, "xingkong")
        }),
        [](const PropertyList& properties) -> ReturnValue {
            std::string theme = properties["theme"].value<std::string>();
            EyeTheme theme_id;

            if (theme == "xingkong") {
                theme_id = EYE_THEME_XINGKONG;
            } else if (theme == "shuimu") {
                theme_id = EYE_THEME_SHUIMU;
            } else if (theme == "keji") {
                theme_id = EYE_THEME_KEJI;
            } else {
                return std::string("{\"success\": false, \"message\": \"Unknown theme: ") + theme +
                       std::string(". Available themes: xingkong, shuimu, keji\"}");
            }

            SetEyeTheme(theme_id);

            return std::string("{\"success\": true, \"message\": \"Eye theme changed to ") + theme +
                   std::string("\", \"theme\": \"") + theme + std::string("\"}");
        });

    // 动画表情切换工具
    AddTool("self.eye.set_expression",
        "Switch the eye animation expression. Use this tool to change between different animated eye expressions.\n"
        "\n"
        "Available expressions:\n"
        "  - 'eye' or 'default': Default eye animation (calm, friendly)\n"
        "  - 'grok': Grok animation (fun, expressive)\n"
        "  - 'next': Switch to next available expression (cycles through all expressions)\n"
        "  - 'previous': Switch to previous expression\n"
        "\n"
        "The tool will automatically loop animations by default.\n"
        "\n"
        "Args:\n"
        "  `expression`: Expression name (string). Use 'next' to cycle through expressions automatically.\n"
        "  `loop`: Loop animation (boolean, default: true)\n"
        "\n"
        "Examples:\n"
        "  - 'Switch to next expression' -> expression='next'\n"
        "  - 'Change animation' -> expression='next'\n"
        "  - 'Switch to Grok' -> expression='grok'\n"
        "  - 'Use default eye animation' -> expression='eye'\n"
        "  - 'Play next animation' -> expression='next'",
        PropertyList({
            Property("expression", kPropertyTypeString, "next"),
            Property("loop", kPropertyTypeBoolean, true)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            std::string expression = properties["expression"].value<std::string>();
            bool loop = properties["loop"].value<bool>();

            // 处理"下一个"命令
            if (expression == "next") {
                expression_type_t current = multi_anim_get_current_expression();
                if (current == EXPRESSION_EYE) {
                    expression = "grok";
                } else if (current == EXPRESSION_GROK) {
                    expression = "eye";
                } else {
                    expression = "eye";  // 默认从 eye 开始
                }
            }
            // 处理"上一个"命令
            else if (expression == "previous") {
                expression_type_t current = multi_anim_get_current_expression();
                if (current == EXPRESSION_EYE) {
                    expression = "grok";
                } else if (current == EXPRESSION_GROK) {
                    expression = "eye";
                } else {
                    expression = "grok";
                }
            }
            // 处理"default"别名
            else if (expression == "default") {
                expression = "eye";
            }

            esp_err_t ret = switch_expression(expression.c_str(), loop);

            if (ret == ESP_OK) {
                return std::string("{\"success\": true, \"message\": \"Expression changed to ") + expression +
                       std::string("\", \"expression\": \"") + expression +
                       std::string("\", \"loop\": ") + (loop ? "true" : "false") + std::string("}");
            } else {
                return std::string("{\"success\": false, \"message\": \"Failed to change expression to ") + expression +
                       std::string(". Available expressions: eye (default), grok, next\"}");
            }
        });


    // Restore the original tools list to the end of the tools list
    tools_.insert(tools_.end(), original_tools.begin(), original_tools.end());
}

void McpServer::AddTool(McpTool* tool) {
    // Prevent adding duplicate tools
    if (std::find_if(tools_.begin(), tools_.end(), [tool](const McpTool* t) { return t->name() == tool->name(); }) != tools_.end()) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name().c_str());
        return;
    }

    ESP_LOGI(TAG, "Add tool: %s", tool->name().c_str());
    tools_.push_back(tool);
}

void McpServer::AddTool(const std::string& name, const std::string& description, const PropertyList& properties, std::function<ReturnValue(const PropertyList&)> callback) {
    AddTool(new McpTool(name, description, properties, callback));
}

void McpServer::ParseMessage(const std::string& message) {
    cJSON* json = cJSON_Parse(message.c_str());
    if (json == nullptr) {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message.c_str());
        return;
    }
    ParseMessage(json);
    cJSON_Delete(json);
}

void McpServer::ParseCapabilities(const cJSON* capabilities) {
    auto vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        auto url = cJSON_GetObjectItem(vision, "url");
        auto token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            auto camera = Board::GetInstance().GetCamera();
            if (camera) {
                std::string url_str = std::string(url->valuestring);
                std::string token_str;
                if (cJSON_IsString(token)) {
                    token_str = std::string(token->valuestring);
                }
                camera->SetExplainUrl(url_str, token_str);
            }
        }
    }
}

void McpServer::ParseMessage(const cJSON* json) {
    // Check JSONRPC version
    auto version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == nullptr || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        return;
    }
    
    // Check method
    auto method = cJSON_GetObjectItem(json, "method");
    if (method == nullptr || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing method");
        return;
    }
    
    auto method_str = std::string(method->valuestring);
    ESP_LOGI(TAG, "MCP RPC method: %s", method_str.c_str());  // ✅ 打印方法名
    if (method_str.find("notifications") == 0) {
        return;
    }
    
    // Check params
    auto params = cJSON_GetObjectItem(json, "params");
    if (params != nullptr && !cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str.c_str());
        return;
    }

    auto id = cJSON_GetObjectItem(json, "id");
    if (id == nullptr || !cJSON_IsNumber(id)) {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str.c_str());
        return;
    }
    auto id_int = id->valueint;
    
    if (method_str == "initialize") {
        if (cJSON_IsObject(params)) {
            auto capabilities = cJSON_GetObjectItem(params, "capabilities");
            if (cJSON_IsObject(capabilities)) {
                ParseCapabilities(capabilities);
            }
        }
        auto app_desc = esp_app_get_description();
        std::string message = "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"" BOARD_NAME "\",\"version\":\"";
        message += app_desc->version;
        message += "\"}}";
        ReplyResult(id_int, message);
    } else if (method_str == "tools/list") {
        std::string cursor_str = "";
        if (params != nullptr) {
            auto cursor = cJSON_GetObjectItem(params, "cursor");
            if (cJSON_IsString(cursor)) {
                cursor_str = std::string(cursor->valuestring);
            }
        }
        GetToolsList(id_int, cursor_str);
    } else if (method_str == "tools/call") {
        if (!cJSON_IsObject(params)) {
            ESP_LOGE(TAG, "tools/call: Missing params");
            ReplyError(id_int, "Missing params");
            return;
        }
        auto tool_name = cJSON_GetObjectItem(params, "name");
        if (!cJSON_IsString(tool_name)) {
            ESP_LOGE(TAG, "tools/call: Missing name");
            ReplyError(id_int, "Missing name");
            return;
        }
        auto tool_arguments = cJSON_GetObjectItem(params, "arguments");
        if (tool_arguments != nullptr && !cJSON_IsObject(tool_arguments)) {
            ESP_LOGE(TAG, "tools/call: Invalid arguments");
            ReplyError(id_int, "Invalid arguments");
            return;
        }
        auto stack_size = cJSON_GetObjectItem(params, "stackSize");
        if (stack_size != nullptr && !cJSON_IsNumber(stack_size)) {
            ESP_LOGE(TAG, "tools/call: Invalid stackSize");
            ReplyError(id_int, "Invalid stackSize");
            return;
        }
        DoToolCall(id_int, std::string(tool_name->valuestring), tool_arguments, stack_size ? stack_size->valueint : DEFAULT_TOOLCALL_STACK_SIZE);
    } else {
        ESP_LOGE(TAG, "Method not implemented: %s", method_str.c_str());
        ReplyError(id_int, "Method not implemented: " + method_str);
    }
}

void McpServer::ReplyResult(int id, const std::string& result) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id) + ",\"result\":";
    payload += result;
    payload += "}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::ReplyError(int id, const std::string& message) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id);
    payload += ",\"error\":{\"message\":\"";
    payload += message;
    payload += "\"}}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::GetToolsList(int id, const std::string& cursor) {
    const int max_payload_size = 8000;
    std::string json = "{\"tools\":[";
    
    bool found_cursor = cursor.empty();
    auto it = tools_.begin();
    std::string next_cursor = "";
    
    while (it != tools_.end()) {
        // 如果我们还没有找到起始位置，继续搜索
        if (!found_cursor) {
            if ((*it)->name() == cursor) {
                found_cursor = true;
            } else {
                ++it;
                continue;
            }
        }
        
        // 添加tool前检查大小
        std::string tool_json = (*it)->to_json() + ",";
        if (json.length() + tool_json.length() + 30 > max_payload_size) {
            // 如果添加这个tool会超出大小限制，设置next_cursor并退出循环
            next_cursor = (*it)->name();
            break;
        }
        
        json += tool_json;
        ++it;
    }
    
    if (json.back() == ',') {
        json.pop_back();
    }
    
    if (json.back() == '[' && !tools_.empty()) {
        // 如果没有添加任何tool，返回错误
        ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit", next_cursor.c_str());
        ReplyError(id, "Failed to add tool " + next_cursor + " because of payload size limit");
        return;
    }

    if (next_cursor.empty()) {
        json += "]}";
    } else {
        json += "],\"nextCursor\":\"" + next_cursor + "\"}";
    }
    
    ReplyResult(id, json);
}

void McpServer::DoToolCall(int id, const std::string& tool_name, const cJSON* tool_arguments, int stack_size) {
    auto tool_iter = std::find_if(tools_.begin(), tools_.end(), 
                                 [&tool_name](const McpTool* tool) { 
                                     return tool->name() == tool_name; 
                                 });
    
    if (tool_iter == tools_.end()) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name.c_str());
        ReplyError(id, "Unknown tool: " + tool_name);
        return;
    }

    PropertyList arguments = (*tool_iter)->properties();
    try {
        for (auto& argument : arguments) {
            bool found = false;
            if (cJSON_IsObject(tool_arguments)) {
                auto value = cJSON_GetObjectItem(tool_arguments, argument.name().c_str());
                if (argument.type() == kPropertyTypeBoolean && cJSON_IsBool(value)) {
                    argument.set_value<bool>(value->valueint == 1);
                    found = true;
                } else if (argument.type() == kPropertyTypeInteger && cJSON_IsNumber(value)) {
                    argument.set_value<int>(value->valueint);
                    found = true;
                } else if (argument.type() == kPropertyTypeString && cJSON_IsString(value)) {
                    argument.set_value<std::string>(value->valuestring);
                    found = true;
                }
            }

            if (!argument.has_default_value() && !found) {
                ESP_LOGE(TAG, "tools/call: Missing valid argument: %s", argument.name().c_str());
                ReplyError(id, "Missing valid argument: " + argument.name());
                return;
            }
        }
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "tools/call: %s", e.what());
        ReplyError(id, e.what());
        return;
    }

    // Start a task to receive data with stack size
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = "tool_call";
    cfg.stack_size = stack_size;
    cfg.prio = 1;
    esp_pthread_set_cfg(&cfg);

    // Use a thread to call the tool to avoid blocking the main thread
    tool_call_thread_ = std::thread([this, id, tool_iter, arguments = std::move(arguments)]() {
        try {
            ReplyResult(id, (*tool_iter)->Call(arguments));
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "tools/call: %s", e.what());
            ReplyError(id, e.what());
        }
    });
    tool_call_thread_.detach();
}