#ifndef CHAT_RECORDER_H
#define CHAT_RECORDER_H

#include <string>
#include <vector>
#include <mutex>
#include <cJSON.h>
#include "protocol.h"

/**
 * @brief 对话记录器
 *
 * 功能：
 * 1. 记录用户与AI的对话文本（STT + TTS）
 * 2. 批量缓存对话消息（默认5条或10分钟）
 * 3. 通过Protocol层批量上传到服务器
 * 4. 支持NVS持久化（掉电保护）
 */
class ChatRecorder {
public:
    /**
     * @brief 单条对话消息
     */
    struct ChatMessage {
        std::string user_text;      // 用户输入文本（STT识别结果）
        std::string ai_text;        // AI回复文本（TTS合成源）
        bool is_complete;           // 是否完整（已有用户和AI内容）

        ChatMessage() : is_complete(false) {}
        ChatMessage(const std::string& user, const std::string& ai)
            : user_text(user), ai_text(ai), is_complete(true) {}
    };

    /**
     * @brief 获取单例实例
     */
    static ChatRecorder& GetInstance();

    /**
     * @brief 记录用户输入（STT识别结果）
     * @param text 用户输入的文本
     */
    void RecordUserInput(const std::string& text);

    /**
     * @brief 记录AI回复（TTS合成文本）
     * @param text AI回复的文本片段
     * @param complete 是否完成记录（true=记录完整对话，false=累积文本）
     */
    void RecordAIResponse(const std::string& text, bool complete = true);

    /**
     * @brief 手动触发批量上传
     */
    void UploadBatch();

    /**
     * @brief 设置Protocol实例
     * @param protocol 协议层指针
     */
    void SetProtocol(Protocol* protocol);

    /**
     * @brief 定时任务入口（每10分钟调用）
     */
    void PeriodicUploadCheck();

private:
    ChatRecorder();
    ~ChatRecorder() = default;
    ChatRecorder(const ChatRecorder&) = delete;
    ChatRecorder& operator=(const ChatRecorder&) = delete;

    /**
     * @brief 检查是否应该触发上传
     * @return true 如果满足上传条件
     */
    bool ShouldUpload() const;

    /**
     * @brief 执行批量上传
     */
    void DoUpload();

    /**
     * @brief 构建批量上传的JSON
     */
    std::string BuildUploadJson();

    /**
     * @brief 清空缓冲区
     */
    void ClearBuffer();

    // 成员变量
    std::vector<ChatMessage> buffer_;     // 批量缓冲区
    std::mutex mutex_;                    // 线程安全保护
    Protocol* protocol_;                  // 协议层指针
    size_t batch_size_threshold_;         // 批量大小阈值（默认5条）
    int64_t last_upload_time_;            // 上次上传时间（毫秒）
    int64_t upload_interval_ms_;          // 上传间隔（默认10分钟）

    // 临时存储当前对话的用户输入
    std::string pending_user_text_;

    // 临时累积AI回复的多个句子
    std::string pending_ai_text_;
};

#endif // CHAT_RECORDER_H
