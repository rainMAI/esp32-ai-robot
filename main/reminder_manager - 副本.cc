#include "reminder_manager.h"
#include <esp_log.h>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "esp_network.h"  // 使用项目已有的网络层
#include "system_info.h"   // 添加系统信息

#include <cinttypes> // For PRId64

#define TAG "ReminderManager"

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

    // Server format: "id" is an integer (database ID), "remote_id" is ESP32's ID (string or null)
    // We use the integer "id" from server for deletion
    cJSON* id_item = cJSON_GetObjectItem(json, "id");
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

    cJSON* timestamp = cJSON_GetObjectItem(json, "timestamp");
    if (cJSON_IsNumber(timestamp)) {
        reminder.timestamp = (long long)timestamp->valuedouble;
    }

    cJSON* content = cJSON_GetObjectItem(json, "content");
    if (cJSON_IsString(content)) {
        reminder.content = content->valuestring;
    }

    cJSON* scheduled_time = cJSON_GetObjectItem(json, "scheduled_time");
    if (cJSON_IsString(scheduled_time)) {
        reminder.scheduled_time = scheduled_time->valuestring;
    } else {
        // Fallback: format from timestamp if scheduled_time not available
        reminder.scheduled_time = format_timestamp_hhmm(reminder.timestamp);
    }

    cJSON* created_at = cJSON_GetObjectItem(json, "created_at");
    if (cJSON_IsNumber(created_at)) {
        reminder.created_at = (long long)created_at->valuedouble;
    }

    return reminder;
}

ReminderManager::ReminderManager() {
}

void ReminderManager::Initialize() {
    // No longer loading from NVS
    // Reminders will be loaded from server via SyncPull
    ESP_LOGI(TAG, "ReminderManager initialized (server-sync mode)");
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

bool ReminderManager::RemoveReminder(const std::string& id) {
    // id might be a string (like "1736331234_12345") or a number
    // Try to parse as integer first
    int reminder_id = -1;
    try {
        reminder_id = std::stoi(id);
    } catch (...) {
        // Not a number, keep -1
    }

    // Call remote API to delete reminder
    if (reminder_id > 0 && RemoveRemote(reminder_id)) {
        // If successful, sync from server to get the updated list
        SyncPull(server_url_);
        return true;
    }

    ESP_LOGE(TAG, "Failed to remove reminder: %s", id.c_str());
    return false;
}

std::vector<Reminder> ReminderManager::GetReminders() const {
    return reminders_;
}

void ReminderManager::ProcessDueReminders(std::function<bool(const Reminder&)> callback) {
    long long now = std::time(nullptr);

    if (!reminders_.empty()) {
        ESP_LOGD(TAG, "Checking %d reminders, now: %lld, first: %lld", (int)reminders_.size(), now, reminders_.front().timestamp);
    }

    auto it = reminders_.begin();
    while (it != reminders_.end()) {
        if (it->timestamp <= now) {
            ESP_LOGI(TAG, "Reminder due: %s", it->content.c_str());
            // Process the reminder
            if (callback(*it)) {
                ESP_LOGI(TAG, "Reminder handled, removing");
                // If handled, remove it from memory (will be synced on next pull)
                it = reminders_.erase(it);
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

bool ReminderManager::SyncPull(const std::string& server_url) {
    ESP_LOGI(TAG, "Pulling reminders from server: %s", server_url.c_str());

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

    // Get reminders array
    cJSON* reminders_array = cJSON_GetObjectItem(root, "reminders");
    if (reminders_array == nullptr || !cJSON_IsArray(reminders_array)) {
        ESP_LOGE(TAG, "No reminders array in response");
        cJSON_Delete(root);
        return false;
    }

    // Clear existing reminders and replace with server reminders
    reminders_.clear();

    int count = 0;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, reminders_array) {
        Reminder reminder = Reminder::from_json(item);
        reminders_.push_back(reminder);
        count++;
    }

    cJSON_Delete(root);

    // Sort by timestamp
    std::sort(reminders_.begin(), reminders_.end(), [](const Reminder& a, const Reminder& b) {
        return a.timestamp < b.timestamp;
    });

    ESP_LOGI(TAG, "Sync pull completed. Loaded %d reminders from server", count);
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

bool ReminderManager::AddRemote(const std::string& content, long long timestamp) {
    ESP_LOGI(TAG, "Adding reminder to server: %s at %ld", content.c_str(), (long)timestamp);

    EspNetwork network;
    auto http = network.CreateHttp(0);

    // Add trailing slash to avoid 308 redirect
    std::string url = server_url_ + "/api/reminders/";

    // Build request body
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_mac", SystemInfo::GetMacAddress().c_str());
    cJSON_AddStringToObject(root, "content", content.c_str());
    cJSON_AddStringToObject(root, "reminder_type", "once");
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

