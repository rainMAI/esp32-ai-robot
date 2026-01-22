#include "reminder_manager.h"
#include <esp_log.h>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <cinttypes> // For PRId64

#define TAG "ReminderManager"

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
    cJSON* id = cJSON_GetObjectItem(json, "id");
    if (cJSON_IsString(id)) {
        reminder.id = id->valuestring;
    }
    cJSON* timestamp = cJSON_GetObjectItem(json, "timestamp");
    if (cJSON_IsNumber(timestamp)) {
        reminder.timestamp = (long long)timestamp->valuedouble;
    }
    cJSON* content = cJSON_GetObjectItem(json, "content");
    if (cJSON_IsString(content)) {
        reminder.content = content->valuestring;
    }
    cJSON* created_at = cJSON_GetObjectItem(json, "created_at");
    if (cJSON_IsNumber(created_at)) {
        reminder.created_at = (long long)created_at->valuedouble;
    }
    return reminder;
}

ReminderManager::ReminderManager() : settings_("reminder", true) {
}

void ReminderManager::Initialize() {
    LoadReminders();
}

void ReminderManager::LoadReminders() {
    std::string json_str = settings_.GetString("list", "[]");
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse reminders json");
        return;
    }

    reminders_.clear();
    if (cJSON_IsArray(root)) {
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, root) {
            reminders_.push_back(Reminder::from_json(item));
        }
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d reminders", reminders_.size());
}

void ReminderManager::SaveReminders() {
    cJSON* root = cJSON_CreateArray();
    for (const auto& reminder : reminders_) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", reminder.id.c_str());
        cJSON_AddNumberToObject(item, "timestamp", (double)reminder.timestamp);
        cJSON_AddStringToObject(item, "content", reminder.content.c_str());
        cJSON_AddNumberToObject(item, "created_at", (double)reminder.created_at);
        cJSON_AddItemToArray(root, item);
    }
    char* json_str = cJSON_PrintUnformatted(root);
    settings_.SetString("list", json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
}

void ReminderManager::AddReminder(long long timestamp, const std::string& content) {
    Reminder reminder;
    // Generate a simple unique ID
    std::stringstream ss;
    ss << std::time(nullptr) << "_" << rand();
    reminder.id = ss.str();

    reminder.timestamp = timestamp;
    reminder.content = content;
    reminder.created_at = std::time(nullptr);

    reminders_.push_back(reminder);
    // Sort by timestamp
    std::sort(reminders_.begin(), reminders_.end(), [](const Reminder& a, const Reminder& b) {
        return a.timestamp < b.timestamp;
    });

    SaveReminders();
    ESP_LOGI(TAG, "Added reminder: %s at %ld (now: %ld)", content.c_str(), (long)timestamp, (long)std::time(nullptr));
}

bool ReminderManager::RemoveReminder(const std::string& id) {
    auto it = std::remove_if(reminders_.begin(), reminders_.end(), [&id](const Reminder& r) {
        return r.id == id;
    });

    if (it != reminders_.end()) {
        reminders_.erase(it, reminders_.end());
        SaveReminders();
        ESP_LOGI(TAG, "Removed reminder: %s", id.c_str());
        return true;
    }
    return false;
}

std::vector<Reminder> ReminderManager::GetReminders() const {
    return reminders_;
}

void ReminderManager::ProcessDueReminders(std::function<bool(const Reminder&)> callback) {
    long long now = std::time(nullptr);
    bool changed = false;

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
                // If handled, remove it
                it = reminders_.erase(it);
                changed = true;
            } else {
                ESP_LOGW(TAG, "Reminder not handled, retrying later");
                // Not handled, keep it
                ++it;
            }
        } else {
            ++it;
        }
    }

    if (changed) {
        SaveReminders();
    }
}
