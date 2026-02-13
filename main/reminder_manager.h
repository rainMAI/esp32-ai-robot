#ifndef REMINDER_MANAGER_H
#define REMINDER_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <cJSON.h>

struct Reminder {
    std::string id;
    long long timestamp;
    std::string content;
    long long created_at;
    std::string scheduled_time;  // HH:MM format from server
    mutable bool local_alert_shown = false;

    std::string to_json() const;
    static Reminder from_json(const cJSON* json);
};

class ReminderManager {
public:
    static ReminderManager& GetInstance() {
        static ReminderManager instance;
        return instance;
    }

    void Initialize();
    bool AddReminder(long long timestamp, const std::string& content);
    bool AddReminderWithTime(const std::string& content, const std::string& time_str, bool is_daily = false);
    bool RemoveReminder(const std::string& id);
    std::vector<Reminder> GetReminders() const;
    void ProcessDueReminders(std::function<bool(const Reminder&)> callback);

    // 系统提醒处理标志（防止AI在处理提醒时误添加提醒）
    static void SetProcessingReminder(bool processing) { processing_system_reminder_ = processing; }
    static bool IsProcessingReminder() { return processing_system_reminder_; }

    // Remote API calls - sync with server immediately
    bool AddRemote(const std::string& content, long long timestamp, bool is_daily = false);
    bool UpdateRemote(int reminder_id, const std::string& new_content, const std::string& new_time = "");
    bool RemoveRemote(int reminder_id);

    // HTTP Sync functions
    bool SyncPull(const std::string& server_url, bool force_replace = false);
    bool SyncPush(const std::string& server_url);
    void SetServerUrl(const std::string& url) { server_url_ = url; }
    std::string GetServerUrl() const { return server_url_; }

private:
    ReminderManager();
    ~ReminderManager() = default;

    std::vector<Reminder> reminders_;
    std::string server_url_;

    // 静态标志：是否正在处理系统提醒
    static bool processing_system_reminder_;

    // NVS 存储辅助函数
    bool LoadFromNVS();
    bool SaveToNVS();
    std::string RemindersToJson() const;
    std::vector<Reminder> JsonToReminders(const std::string& json_str) const;
    bool MergeRemoteReminders(const std::vector<Reminder>& remote_reminders);
    std::string GenerateLocalId();

    // 分片存储支持
    bool SaveToNVSSharded(const std::string& json_str);
    std::string LoadFromNVSSharded();

    // 同步状态追踪
    bool HasNewReminders() const;
    long long last_sync_time_ = 0;

    // 容量限制
    static const int MAX_REMINDERS = 30;
};

#endif // REMINDER_MANAGER_H
