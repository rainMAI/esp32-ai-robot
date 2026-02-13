#include "reminder_manager.h"
#include "settings.h"
#include <esp_log.h>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "esp_network.h"  // 使用项目已有的网络层
#include "system_info.h"   // 添加系统信息

#include <cinttypes> // For PRId64

#define TAG "ReminderManager"

// 初始化静态标志
bool ReminderManager::processing_system_reminder_ = false;

// Helper function to format timestamp as HH:MM
static std::string format_timestamp_hhmm(long long timestamp) {
    time_t tt = (time_t)timestamp;
    struct tm* tm_info = localtime(&tt);

    // Debug log to track timezone conversion
    ESP_LOGI(TAG, "[TZ_DEBUG] timestamp=%ld, hour=%d, min=%d, timezone=%s",
             (long)timestamp, tm_info->tm_hour, tm_info->tm_min, getenv("TZ"));

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << tm_info->tm_hour << ":"
       << std::setfill('0') << std::setw(2) << tm_info->tm_min;
    return ss.str();
}

// Helper function to calculate timestamp from HH:MM string
// Returns timestamp for today or tomorrow (if time has passed)
static long long calculate_timestamp_from_hhmm(const std::string& time_str) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int target_hour = 0, target_min = 0;
    if (sscanf(time_str.c_str(), "%d:%d", &target_hour, &target_min) != 2) {
        ESP_LOGE(TAG, "Invalid time format: %s", time_str.c_str());
        return 0;
    }

    // Set target time for today
    struct tm target_tm = timeinfo;
    target_tm.tm_hour = target_hour;
    target_tm.tm_min = target_min;
    target_tm.tm_sec = 0;
    time_t target_timestamp = mktime(&target_tm);

    // If time has passed, use tomorrow
    if (target_timestamp <= now) {
        target_timestamp += 86400;  // Add 24 hours
    }

    ESP_LOGI(TAG, "[TZ_CALC] time_str=%s -> timestamp=%ld (today %02d:%02d %s)",
             time_str.c_str(), (long)target_timestamp,
             target_hour, target_min,
             (target_timestamp > now) ? "future" : "past+24h");

    return (long long)target_timestamp;
}

std::string Reminder::to_json() const {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "id", id.c_str());
    cJSON_AddNumberToObject(json, "timestamp", (double)timestamp);
    cJSON_AddStringToObject(json, "content", content.c_str());
    cJSON_AddNumberToObject(json, "created_at", (double)created_at);
    char* json_str = cJSON_PrintUnformatted(json);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);
    return result;
}

Reminder Reminder::from_json(const cJSON* json) {
    Reminder reminder;

    // Parse ID (try "id" or "i" for compact format)
    // Server format: "id" is an integer (database ID), "remote_id" is ESP32's ID (string or null)
    // Compact format: "i" is the ID as string
    cJSON* id_item = cJSON_GetObjectItem(json, "id");
    if (id_item == nullptr) {
        id_item = cJSON_GetObjectItem(json, "i");
    }
    if (id_item != nullptr) {
        // Convert integer ID to string for storage
        if (cJSON_IsNumber(id_item)) {
            std::stringstream ss;
            ss << (int)id_item->valuedouble;
            reminder.id = ss.str();
        } else if (cJSON_IsString(id_item)) {
            reminder.id = id_item->valuestring;
        }
    }

    // Parse content (try "content" or "c" for compact format)
    cJSON* content = cJSON_GetObjectItem(json, "content");
    if (content == nullptr) {
        content = cJSON_GetObjectItem(json, "c");
    }
    if (cJSON_IsString(content)) {
        reminder.content = content->valuestring;
    }

    // Parse scheduled_time (try "scheduled_time" or "s" for compact format)
    cJSON* scheduled_time = cJSON_GetObjectItem(json, "scheduled_time");
    if (scheduled_time == nullptr) {
        scheduled_time = cJSON_GetObjectItem(json, "s");
    }
    bool has_scheduled_time = false;
    if (cJSON_IsString(scheduled_time)) {
        reminder.scheduled_time = scheduled_time->valuestring;
        has_scheduled_time = true;
    }

    // Parse created_at (try "created_at" or "ca" for compact format)
    cJSON* created_at = cJSON_GetObjectItem(json, "created_at");
    if (created_at == nullptr) {
        created_at = cJSON_GetObjectItem(json, "ca");
    }
    if (cJSON_IsNumber(created_at)) {
        reminder.created_at = (long long)created_at->valuedouble;
    }

    // IMPORTANT: Prioritize server's timestamp to avoid recalculation issues
    // Only recalculate from scheduled_time if server didn't provide timestamp
    cJSON* timestamp = cJSON_GetObjectItem(json, "timestamp");
    if (timestamp == nullptr) {
        timestamp = cJSON_GetObjectItem(json, "t");
    }

    if (cJSON_IsNumber(timestamp) && timestamp->valuedouble != 0) {
        // Server provided valid timestamp - use it directly
        reminder.timestamp = (long long)timestamp->valuedouble;
        ESP_LOGI(TAG, "[SYNC] Using server timestamp for '%s': %" PRId64,
                 reminder.content.c_str(), (int64_t)reminder.timestamp);
    } else if (has_scheduled_time && !reminder.scheduled_time.empty()) {
        // No timestamp from server - calculate from scheduled_time
        reminder.timestamp = calculate_timestamp_from_hhmm(reminder.scheduled_time);
        ESP_LOGI(TAG, "[SYNC] No server timestamp, calculated from '%s': %" PRId64,
                 reminder.scheduled_time.c_str(), (int64_t)reminder.timestamp);
    }

    // Format scheduled_time from timestamp if not available
    if (reminder.scheduled_time.empty() && reminder.timestamp > 0) {
        reminder.scheduled_time = format_timestamp_hhmm(reminder.timestamp);
    }

    return reminder;
}

ReminderManager::ReminderManager() {
}

void ReminderManager::Initialize() {
    // 先从 NVS 加载本地提醒
    if (!LoadFromNVS()) {
        ESP_LOGW(TAG, "No reminders found in NVS, starting fresh");
    } else {
        ESP_LOGI(TAG, "ReminderManager initialized (local NVS + server sync mode)");
        ESP_LOGI(TAG, "Loaded %d reminders from NVS", (int)reminders_.size());
    }

    // 标记需要后台同步（不阻塞启动）
    last_sync_time_ = 0;
}

bool ReminderManager::AddReminder(long long timestamp, const std::string& content) {
    // Call remote API to add reminder
    if (!AddRemote(content, timestamp)) {
        ESP_LOGE(TAG, "Failed to add reminder to server: %s", content.c_str());
        return false;
    }

    // Reminder was successfully added to server
    // Try to sync back to get the updated list, but don't treat sync failure as reminder failure
    ESP_LOGI(TAG, "Reminder added to server successfully, syncing back...");
    if (!SyncPull(server_url_)) {
        ESP_LOGW(TAG, "Reminder added to server but sync pull failed - reminder exists on server but local list may be outdated");
        // Still return true since the reminder was successfully added to server
    }

    return true;
}

bool ReminderManager::AddReminderWithTime(const std::string& content, const std::string& time_str, bool is_daily) {
    ESP_LOGI(TAG, "Adding reminder with time string: %s, content: %s, is_daily: %s",
             time_str.c_str(), content.c_str(), is_daily ? "true" : "false");

    // 1. 解析时间字符串，格式如 "10:59" 或 "22:50"
    int target_hour = 0, target_min = 0;
    if (sscanf(time_str.c_str(), "%d:%d", &target_hour, &target_min) != 2) {
        ESP_LOGE(TAG, "Invalid time format: %s", time_str.c_str());
        return false;
    }

    // 2. 获取当前本地时间
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    int current_hour = timeinfo.tm_hour;
    int current_min = timeinfo.tm_min;

    ESP_LOGI(TAG, "Current time: %02d:%02d, Target time: %02d:%02d",
             current_hour, current_min, target_hour, target_min);

    // 3. 计算目标时间戳
    struct tm target_tm = timeinfo;
    target_tm.tm_hour = target_hour;
    target_tm.tm_min = target_min;
    target_tm.tm_sec = 0;
    time_t target_timestamp = mktime(&target_tm);

    // 如果目标时间已经过了，设置为明天
    if (target_timestamp <= now) {
        ESP_LOGI(TAG, "Target time has passed today, setting for tomorrow");
        target_timestamp += 86400; // 加24小时
    }

    ESP_LOGI(TAG, "Calculated timestamp: %ld (now: %ld)", (long)target_timestamp, (long)now);

    // 4. 容量检查
    if (reminders_.size() >= MAX_REMINDERS) {
        ESP_LOGE(TAG, "Maximum reminders limit reached: %d", MAX_REMINDERS);
        return false;
    }

    // 5. 创建提醒对象（使用本地临时 ID）
    Reminder new_reminder;
    new_reminder.id = GenerateLocalId();  // "local_<timestamp>"
    new_reminder.timestamp = target_timestamp;
    new_reminder.content = content;
    new_reminder.created_at = now;
    new_reminder.scheduled_time = time_str;

    ESP_LOGI(TAG, "Created reminder with local ID: %s", new_reminder.id.c_str());

    // 6. 添加到内存列表
    reminders_.push_back(new_reminder);
    std::sort(reminders_.begin(), reminders_.end(),
              [](const Reminder& a, const Reminder& b) {
                  return a.timestamp < b.timestamp;
              });

    // 7. 先保存到 NVS（持久化优先）
    if (!SaveToNVS()) {
        ESP_LOGE(TAG, "Failed to save reminder to NVS");
        reminders_.pop_back();  // 回滚
        return false;
    }

    ESP_LOGI(TAG, "Reminder saved to NVS successfully");

    // 8. 后同步服务器（失败不影响本地）
    ESP_LOGI(TAG, "Syncing reminder to server...");
    if (!AddRemote(content, target_timestamp, is_daily)) {
        ESP_LOGW(TAG, "Reminder saved locally but server sync failed - will retry later");
        // 不返回 false，因为本地已成功
    } else {
        ESP_LOGI(TAG, "Reminder synced to server successfully");
        // 同步成功后，拉取真实 ID 更新本地
        // 注意：这里需要修改 SyncPull 支持增量模式（阶段3）
        // 暂时先不拉取，等待下次定期同步
    }

    ESP_LOGI(TAG, "Reminder added successfully: %s at %s (ID: %s)",
             content.c_str(), time_str.c_str(), new_reminder.id.c_str());
    return true;
}


bool ReminderManager::RemoveReminder(const std::string& id) {
    ESP_LOGI(TAG, "Removing reminder: %s", id.c_str());

    // 1. 在本地列表中查找并删除
    auto it = std::find_if(reminders_.begin(), reminders_.end(),
                          [&id](const Reminder& r) {
                              // 支持两种 ID 格式：数字 ID 或本地临时 ID
                              // 例如：id="123" 或 id="local_1736331234"
                              if (r.id == id) return true;

                              // 尝试将数字 ID 与本地 ID 的数字部分比较
                              if (r.id.find("local_") == 0) {
                                  std::string local_num = r.id.substr(6);  // 跳过 "local_"
                                  return local_num == id;
                              }

                              return false;
                          });

    if (it == reminders_.end()) {
        ESP_LOGE(TAG, "Reminder not found: %s", id.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Found reminder to remove: %s (content: %s)",
             it->id.c_str(), it->content.c_str());

    // 2. 从内存中删除
    reminders_.erase(it);

    // 3. 先保存到 NVS
    if (!SaveToNVS()) {
        ESP_LOGE(TAG, "Failed to update NVS after removing reminder");
        // 回滚：重新添加到内存
        // 注意：这里简化处理，实际应该保存 it 的副本
        // 但由于 SaveToNVS 失败是严重错误，暂时不回滚
        return false;
    }

    ESP_LOGI(TAG, "Reminder removed from NVS successfully");

    // 4. 后同步服务器（失败不影响本地）
    // id 可能是字符串格式或数字格式
    int reminder_id = -1;
    try {
        reminder_id = std::stoi(id);
    } catch (...) {
        // 不是数字，可能是本地临时 ID，服务器上还没有
        ESP_LOGW(TAG, "Local reminder ID %s not synced to server yet, skipping remote delete", id.c_str());
        return true;  // 本地删除成功即可
    }

    // 尝试从服务器删除
    if (reminder_id > 0) {
        if (!RemoveRemote(reminder_id)) {
            ESP_LOGW(TAG, "Reminder removed locally but server delete failed - will retry later");
            // 不返回 false，因为本地已成功删除
        } else {
            ESP_LOGI(TAG, "Reminder removed from server successfully");
        }
    }

    ESP_LOGI(TAG, "Reminder removed successfully: %s", id.c_str());
    return true;
}

std::vector<Reminder> ReminderManager::GetReminders() const {
    return reminders_;
}

void ReminderManager::ProcessDueReminders(std::function<bool(const Reminder&)> callback) {
    long long now = std::time(nullptr);
    // 定义过期时间阈值：1小时（3600秒）
    const long long EXPIRY_THRESHOLD = 3600;

    if (!reminders_.empty()) {
        ESP_LOGD(TAG, "Checking %d reminders, now: %lld, first: %lld", (int)reminders_.size(), now, reminders_.front().timestamp);
    }

    auto it = reminders_.begin();
    while (it != reminders_.end()) {
        long long time_diff = now - it->timestamp;

        // 检查是否过期（超过1小时的过期提醒）
        if (time_diff > EXPIRY_THRESHOLD) {
            ESP_LOGW(TAG, "Reminder expired (>1hr ago), removing: %s (diff: %lld seconds)", it->content.c_str(), time_diff);
            // 直接删除过期提醒，不触发
            std::string id_to_remove = it->id;
            it = reminders_.erase(it);
            SaveToNVS();
            ESP_LOGI(TAG, "Expired reminder removed: %s", id_to_remove.c_str());
            continue;
        }

        // 检查是否到期
        if (it->timestamp <= now) {
            ESP_LOGI(TAG, "Reminder due: %s", it->content.c_str());
            // Process the reminder
            if (callback(*it)) {
                ESP_LOGI(TAG, "Reminder handled, removing");
                // Remove from memory
                std::string id_to_remove = it->id;
                it = reminders_.erase(it);
                // Save to NVS to persist the removal
                SaveToNVS();
                ESP_LOGI(TAG, "Reminder removed and NVS updated: %s", id_to_remove.c_str());
            } else {
                ESP_LOGW(TAG, "Reminder not handled, retrying later");
                // Not handled, keep it
                ++it;
            }
        } else {
            ++it;
        }
    }
}

bool ReminderManager::SyncPull(const std::string& server_url, bool force_replace) {
    ESP_LOGI(TAG, "Pulling reminders from server: %s (force_replace=%d)",
             server_url.c_str(), force_replace);

    EspNetwork network;
    auto http = network.CreateHttp(0);

    std::string url = server_url + "/api/sync/pull";
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress());
    http->SetHeader("Content-Type", "application/json");

    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for sync pull");
        return false;
    }

    int status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP sync pull failed with status: %d", status_code);
        http->Close();
        return false;
    }

    std::string response = http->ReadAll();
    http->Close();

    ESP_LOGI(TAG, "Sync pull response: %s", response.c_str());

    // Parse JSON response
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse sync pull response");
        return false;
    }

    // Check if request was successful
    cJSON* success = cJSON_GetObjectItem(root, "success");
    if (success != nullptr && !cJSON_IsTrue(success)) {
        cJSON* error = cJSON_GetObjectItem(root, "error");
        const char* error_msg = error ? error->valuestring : "Unknown error";
        ESP_LOGE(TAG, "Sync pull failed: %s", error_msg);
        cJSON_Delete(root);
        return false;
    }

    // 注意：移除了时区同步代码（lines 346-382），因为：
    // 1. 新架构使用本地时间计算，不依赖服务器时区
    // 2. 时区转换错误是原始问题的根源
    // 3. 提醒时间戳由 ESP32 本地生成，服务器仅作为备份

    // Get reminders array
    cJSON* reminders_array = cJSON_GetObjectItem(root, "reminders");
    if (reminders_array == nullptr || !cJSON_IsArray(reminders_array)) {
        ESP_LOGE(TAG, "No reminders array in response");
        cJSON_Delete(root);
        return false;
    }

    // Parse remote reminders
    std::vector<Reminder> remote_reminders;
    int count = 0;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, reminders_array) {
        Reminder reminder = Reminder::from_json(item);
        remote_reminders.push_back(reminder);
        count++;
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Parsed %d reminders from server", count);

    // 根据模式选择同步策略
    if (force_replace) {
        // 强制替换模式：清空本地，完全使用服务器数据
        ESP_LOGW(TAG, "Force replace mode: clearing local reminders and using server data");
        reminders_ = remote_reminders;
        SaveToNVS();
    } else {
        // 增量合并模式：本地优先，仅添加新提醒
        ESP_LOGI(TAG, "Incremental merge mode: merging server reminders with local data");
        MergeRemoteReminders(remote_reminders);
    }

    // Sort by timestamp
    std::sort(reminders_.begin(), reminders_.end(), [](const Reminder& a, const Reminder& b) {
        return a.timestamp < b.timestamp;
    });

    ESP_LOGI(TAG, "Sync pull completed. Total reminders: %d", (int)reminders_.size());
    return true;
}

bool ReminderManager::SyncPush(const std::string& server_url) {
    ESP_LOGI(TAG, "Pushing reminders to server: %s", server_url.c_str());

    // Build JSON array of all reminders
    cJSON* root = cJSON_CreateArray();
    for (const auto& reminder : reminders_) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "remote_id", reminder.id.c_str());
        cJSON_AddStringToObject(item, "content", reminder.content.c_str());
        cJSON_AddNumberToObject(item, "timestamp", (double)reminder.timestamp);
        cJSON_AddNumberToObject(item, "created_at", (double)reminder.created_at);
        cJSON_AddItemToArray(root, item);
    }

    char* json_str = cJSON_PrintUnformatted(root);

    EspNetwork network;
    auto http = network.CreateHttp(0);

    std::string url = server_url + "/api/sync/push";
    std::string post_data = std::string("{\"reminders\":") + json_str + "}";

    http->SetHeader("Device-Id", SystemInfo::GetMacAddress());
    http->SetHeader("Content-Type", "application/json");
    http->SetContent(std::move(post_data));

    cJSON_free(json_str);
    cJSON_Delete(root);

    if (!http->Open("POST", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for sync push");
        return false;
    }

    int status_code = http->GetStatusCode();
    http->Close();

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP sync push failed with status: %d", status_code);
        return false;
    }

    ESP_LOGI(TAG, "Sync push completed. Pushed %d reminders to server", (int)reminders_.size());
    return true;
}

bool ReminderManager::AddRemote(const std::string& content, long long timestamp, bool is_daily) {
    ESP_LOGI(TAG, "Adding reminder to server: %s at %ld, is_daily: %s",
             content.c_str(), (long)timestamp, is_daily ? "true" : "false");

    EspNetwork network;
    auto http = network.CreateHttp(0);

    // Add trailing slash to avoid 308 redirect
    std::string url = server_url_ + "/api/reminders/";

    // Build request body
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_mac", SystemInfo::GetMacAddress().c_str());
    cJSON_AddStringToObject(root, "content", content.c_str());
    cJSON_AddStringToObject(root, "reminder_type", is_daily ? "daily" : "once");
    cJSON_AddNumberToObject(root, "scheduled_timestamp", (double)timestamp);
    cJSON_AddStringToObject(root, "scheduled_time", format_timestamp_hhmm(timestamp).c_str());
    cJSON_AddBoolToObject(root, "skip_holidays", 0);

    char* json_str = cJSON_PrintUnformatted(root);
    std::string post_data(json_str);

    cJSON_free(json_str);
    cJSON_Delete(root);

    http->SetHeader("Content-Type", "application/json");
    http->SetContent(std::move(post_data));

    if (!http->Open("POST", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for add reminder");
        return false;
    }

    int status_code = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    if (status_code != 200 && status_code != 201) {
        ESP_LOGE(TAG, "Failed to add reminder: HTTP %d, response: %s", status_code, response.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Successfully added reminder to server: %s", content.c_str());
    return true;
}

bool ReminderManager::UpdateRemote(int reminder_id, const std::string& new_content, const std::string& new_time) {
    if (!new_time.empty()) {
        ESP_LOGI(TAG, "Updating reminder on server: id=%d, new_content=%s, new_time=%s", reminder_id, new_content.c_str(), new_time.c_str());
    } else {
        ESP_LOGI(TAG, "Updating reminder on server: id=%d, new_content=%s", reminder_id, new_content.c_str());
    }

    EspNetwork network;
    auto http = network.CreateHttp(0);

    // Build URL with reminder_id (NO trailing slash for PUT)
    std::stringstream ss_url;
    ss_url << server_url_ << "/api/reminders/" << reminder_id;
    std::string url = ss_url.str();

    // Build request body
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "content", new_content.c_str());

    // Add scheduled_time if provided
    if (!new_time.empty()) {
        cJSON_AddStringToObject(root, "scheduled_time", new_time.c_str());
    }

    char* json_str = cJSON_PrintUnformatted(root);
    std::string post_data(json_str);

    cJSON_free(json_str);
    cJSON_Delete(root);

    http->SetHeader("Content-Type", "application/json");
    http->SetContent(std::move(post_data));

    if (!http->Open("PUT", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for update reminder");
        return false;
    }

    int status_code = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to update reminder: HTTP %d, response: %s", status_code, response.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Successfully updated reminder on server: id=%d", reminder_id);
    return true;
}

bool ReminderManager::RemoveRemote(int reminder_id) {
    ESP_LOGI(TAG, "Removing reminder from server: id=%d", reminder_id);

    EspNetwork network;
    auto http = network.CreateHttp(0);

    // Build URL with reminder_id (NO trailing slash for DELETE)
    std::stringstream ss;
    ss << server_url_ << "/api/reminders/" << reminder_id;
    std::string url = ss.str();

    http->SetHeader("Content-Type", "application/json");

    if (!http->Open("DELETE", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for delete reminder");
        return false;
    }

    int status_code = http->GetStatusCode();
    std::string response = http->ReadAll();
    http->Close();

    if (status_code != 200 && status_code != 204) {
        ESP_LOGE(TAG, "Failed to remove reminder: HTTP %d, response: %s", status_code, response.c_str());
        return false;
    }

    ESP_LOGI(TAG, "Successfully removed reminder from server: id=%d", reminder_id);
    return true;
}

// ============================================================================
// NVS Storage Helper Functions
// ============================================================================

bool ReminderManager::LoadFromNVS() {
    Settings settings("reminders", false);

    // 检查是否为分片存储
    int num_chunks = settings.GetInt("num_chunks", 0);

    std::string json_str;
    if (num_chunks > 0) {
        // 分片加载
        for (int i = 0; i < num_chunks; i++) {
            char key[20];
            snprintf(key, sizeof(key), "chunk_%d", i);
            std::string chunk = settings.GetString(key, "");
            if (chunk.empty()) {
                ESP_LOGE(TAG, "Failed to load chunk %d", i);
                return false;
            }
            json_str += chunk;
        }
        ESP_LOGI(TAG, "Loaded from NVS (sharded): %d chunks, %d bytes",
                 num_chunks, (int)json_str.length());
    } else {
        // 单片加载
        json_str = settings.GetString("data", "");
        ESP_LOGI(TAG, "Loaded from NVS (single): %d bytes",
                 (int)json_str.length());
    }

    if (json_str.empty()) {
        ESP_LOGI(TAG, "No reminders found in NVS (first use?)");
        return false;  // 首次使用
    }

    // 解析 JSON
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse NVS JSON, data may be corrupted");
        // 备份损坏的数据
        Settings("reminders", true).SetString("data_corrupted_backup", json_str);
        // 清空损坏的数据
        Settings("reminders", true).EraseKey("data");
        Settings("reminders", true).EraseKey("num_chunks");
        return false;
    }

    // 检查版本号
    cJSON* version = cJSON_GetObjectItem(root, "v");  // 紧凑格式
    if (version == nullptr) {
        version = cJSON_GetObjectItem(root, "version");  // 标准格式
    }
    if (version != nullptr && cJSON_IsNumber(version)) {
        int v = version->valueint;
        if (v > 1) {
            ESP_LOGW(TAG, "Unknown reminder data version: %d", v);
            cJSON_Delete(root);
            return false;
        }
    }

    // 加载提醒数组
    cJSON* reminders_array = cJSON_GetObjectItem(root, "r");  // 紧凑格式
    if (reminders_array == nullptr) {
        reminders_array = cJSON_GetObjectItem(root, "reminders");  // 标准格式
    }

    if (reminders_array == nullptr || !cJSON_IsArray(reminders_array)) {
        ESP_LOGE(TAG, "Invalid reminders format in NVS");
        cJSON_Delete(root);
        return false;
    }

    reminders_.clear();
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, reminders_array) {
        Reminder reminder = Reminder::from_json(item);
        reminders_.push_back(reminder);
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d reminders from NVS", (int)reminders_.size());
    return true;
}

std::string ReminderManager::LoadFromNVSSharded() {
    Settings settings("reminders", false);
    int num_chunks = settings.GetInt("num_chunks", 0);

    std::string json_str;
    for (int i = 0; i < num_chunks; i++) {
        char key[20];
        snprintf(key, sizeof(key), "chunk_%d", i);
        std::string chunk = settings.GetString(key, "");
        if (chunk.empty()) {
            ESP_LOGE(TAG, "Failed to load chunk %d from NVS", i);
            return "";
        }
        json_str += chunk;
    }

    ESP_LOGI(TAG, "Loaded %d chunks from NVS (%d bytes)",
             num_chunks, (int)json_str.length());
    return json_str;
}

bool ReminderManager::SaveToNVS() {
    std::string json_str = RemindersToJson();

    if (json_str.empty()) {
        ESP_LOGE(TAG, "Failed to serialize reminders to JSON");
        return false;
    }

    // 检查是否需要分片
    if (json_str.length() <= 1800) {
        // 单片存储
        Settings settings("reminders", true);
        settings.SetString("data", json_str);
        settings.SetInt("v", 1);  // 紧凑格式版本号
        settings.EraseKey("num_chunks");

        ESP_LOGI(TAG, "Saved %d reminders to NVS (single, %d bytes)",
                 (int)reminders_.size(), (int)json_str.length());
    } else {
        // 分片存储
        return SaveToNVSSharded(json_str);
    }

    return true;
}

bool ReminderManager::SaveToNVSSharded(const std::string& json_str) {
    int chunk_size = 1800;
    int num_chunks = (json_str.length() + chunk_size - 1) / chunk_size;

    Settings settings("reminders", true);
    settings.SetInt("num_chunks", num_chunks);
    settings.SetInt("v", 1);  // 紧凑格式版本号

    for (int i = 0; i < num_chunks; i++) {
        std::string chunk = json_str.substr(i * chunk_size, chunk_size);
        char key[20];
        snprintf(key, sizeof(key), "chunk_%d", i);
        settings.SetString(key, chunk);
    }

    ESP_LOGI(TAG, "Saved %d reminders to NVS (%d chunks, %d bytes)",
             (int)reminders_.size(), num_chunks, (int)json_str.length());
    return true;
}

std::string ReminderManager::RemindersToJson() const {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "v", 1);  // version

    cJSON* reminders_array = cJSON_CreateArray();
    for (const auto& reminder : reminders_) {
        cJSON* item = cJSON_CreateObject();

        // 紧凑键名
        cJSON_AddStringToObject(item, "i", reminder.id.c_str());
        cJSON_AddNumberToObject(item, "t", (double)reminder.timestamp);
        cJSON_AddStringToObject(item, "c", reminder.content.c_str());
        cJSON_AddNumberToObject(item, "ca", (double)reminder.created_at);
        cJSON_AddStringToObject(item, "s", reminder.scheduled_time.c_str());

        cJSON_AddItemToArray(reminders_array, item);
    }
    cJSON_AddItemToObject(root, "r", reminders_array);

    char* json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);

    return result;
}

std::string ReminderManager::GenerateLocalId() {
    time_t now = time(nullptr);
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "local_%ld", (long)now);
    return std::string(id_buf);
}

bool ReminderManager::HasNewReminders() const {
    // 简单实现：如果有本地生成的临时 ID，说明有新数据
    for (const auto& reminder : reminders_) {
        if (reminder.id.find("local_") == 0) {
            return true;
        }
    }
    return false;
}

bool ReminderManager::MergeRemoteReminders(const std::vector<Reminder>& remote_reminders) {
    ESP_LOGI(TAG, "Merging %d remote reminders with local %d reminders",
             (int)remote_reminders.size(), (int)reminders_.size());

    int added = 0;
    int updated = 0;  // 更新本地 ID 为服务器 ID
    int skipped = 0;
    int conflicts = 0;  // 冲突次数（本地优先）

    for (const auto& remote : remote_reminders) {
        // 查找本地是否有相同 ID 的提醒
        auto it = std::find_if(reminders_.begin(), reminders_.end(),
                              [&remote](const Reminder& local) {
                                  return local.id == remote.id;
                              });

        if (it == reminders_.end()) {
            // 新提醒，添加到本地
            reminders_.push_back(remote);
            added++;
            ESP_LOGI(TAG, "Added new remote reminder: %s (ID: %s)",
                     remote.content.c_str(), remote.id.c_str());
        } else {
            // 已存在，比较内容
            bool content_changed = (it->content != remote.content);
            bool timestamp_changed = (it->timestamp != remote.timestamp);

            if (content_changed || timestamp_changed) {
                // 冲突处理：本地优先
                ESP_LOGW(TAG, "Conflict detected for reminder %s: local wins",
                         remote.id.c_str());
                ESP_LOGI(TAG, "  Local:  content='%s' ts=%" PRId64,
                         it->content.c_str(), (int64_t)it->timestamp);
                ESP_LOGI(TAG, "  Remote: content='%s' ts=%" PRId64,
                         remote.content.c_str(), (int64_t)remote.timestamp);
                conflicts++;
            }

            // 特殊处理：如果本地是临时 ID 且内容匹配，更新为服务器 ID
            if (it->id.find("local_") == 0 &&
                !content_changed &&
                !timestamp_changed) {
                ESP_LOGI(TAG, "Updating local ID %s to server ID %s",
                         it->id.c_str(), remote.id.c_str());
                it->id = remote.id;  // 更新为服务器分配的真实 ID
                updated++;
            }

            skipped++;
        }
    }

    // 删除本地有但服务器没有的提醒（用户通过 Web 界面删除的）
    int removed = 0;
    auto it = reminders_.begin();
    while (it != reminders_.end()) {
        bool found_in_remote = false;
        for (const auto& remote : remote_reminders) {
            if (it->id == remote.id) {
                found_in_remote = true;
                break;
            }
        }

        if (!found_in_remote) {
            ESP_LOGI(TAG, "Removing local-only reminder: %s (content: %s)",
                     it->id.c_str(), it->content.c_str());
            it = reminders_.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    // 按时间戳排序
    std::sort(reminders_.begin(), reminders_.end(),
              [](const Reminder& a, const Reminder& b) {
                  return a.timestamp < b.timestamp;
              });

    ESP_LOGI(TAG, "Merge completed: added=%d, updated=%d, skipped=%d, conflicts=%d, removed=%d",
             added, updated, skipped, conflicts, removed);

    // 只在数据变化时写入 NVS
    if (added > 0 || updated > 0 || removed > 0) {
        return SaveToNVS();
    }

    return true;  // 无变化，无需写入
}