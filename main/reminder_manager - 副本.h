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
    bool RemoveReminder(const std::string& id);
    std::vector<Reminder> GetReminders() const;
    void ProcessDueReminders(std::function<bool(const Reminder&)> callback);

    // Remote API calls - sync with server immediately
    bool AddRemote(const std::string& content, long long timestamp);
    bool UpdateRemote(int reminder_id, const std::string& new_content, const std::string& new_time = "");
    bool RemoveRemote(int reminder_id);

    // HTTP Sync functions
    bool SyncPull(const std::string& server_url);
    bool SyncPush(const std::string& server_url);
    void SetServerUrl(const std::string& url) { server_url_ = url; }
    std::string GetServerUrl() const { return server_url_; }

private:
    ReminderManager();
    ~ReminderManager() = default;

    std::vector<Reminder> reminders_;
    std::string server_url_;
};

#endif // REMINDER_MANAGER_H
