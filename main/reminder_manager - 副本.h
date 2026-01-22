#ifndef REMINDER_MANAGER_H
#define REMINDER_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <cJSON.h>
#include "settings.h"

struct Reminder {
    std::string id;
    long long timestamp;
    std::string content;
    long long created_at;
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
    void AddReminder(long long timestamp, const std::string& content);
    bool RemoveReminder(const std::string& id);
    std::vector<Reminder> GetReminders() const;
    void ProcessDueReminders(std::function<bool(const Reminder&)> callback);

private:
    ReminderManager();
    ~ReminderManager() = default;

    void LoadReminders();
    void SaveReminders();

    std::vector<Reminder> reminders_;
    Settings settings_;
};

#endif // REMINDER_MANAGER_H
