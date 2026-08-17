#ifndef ACCESS_LOGGER_HPP
#define ACCESS_LOGGER_HPP

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <atomic>

namespace sysdb {

struct LogEntry {
    std::string timestamp_start;
    std::string timestamp_end;
    std::string client_id;
    std::string handler_id;
    std::string query_body;
    std::string status;
};

class AccessLogger {
public:
    static AccessLogger& instance() {
        static AccessLogger logger;
        return logger;
    }

    bool initialize(const std::string& log_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return true;
        log_file_.open(log_path, std::ios::app);
        if (!log_file_.is_open()) return false;
        running_ = true;
        worker_ = std::thread([this]() { workerLoop(); });
        return true;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_one();
        if (worker_.joinable()) worker_.join();
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_file_.is_open()) log_file_.close();
    }

    void log(LogEntry entry) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(entry));
        }
        cv_.notify_one();
    }

    void setClientId(const std::string& id) { client_id_ = id; }
    const std::string& getClientId() const { return client_id_; }

    static std::string formatTime(std::chrono::system_clock::time_point tp) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()) % 1000;
        auto time_t_val = std::chrono::system_clock::to_time_t(tp);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_val);
#else
        localtime_r(&time_t_val, &tm_buf);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y.%m.%d-%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

private:
    AccessLogger() = default;
    ~AccessLogger() { shutdown(); }
    AccessLogger(const AccessLogger&) = delete;
    AccessLogger& operator=(const AccessLogger&) = delete;

    void workerLoop() {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !queue_.empty() || !running_; });
            while (!queue_.empty()) {
                LogEntry entry = std::move(queue_.front());
                queue_.pop();
                lock.unlock();
                writeEntry(entry);
                lock.lock();
            }
            if (!running_ && queue_.empty()) break;
        }
    }

    void writeEntry(const LogEntry& entry) {
        std::lock_guard<std::mutex> lock(file_mutex_);
        if (!log_file_.is_open()) return;
        log_file_ << "[" << entry.timestamp_start << "] "
                  << "[" << entry.timestamp_end << "] "
                  << "[client:" << entry.client_id << "] "
                  << "[handler:" << entry.handler_id << "] "
                  << "[status:" << entry.status << "] "
                  << entry.query_body << "\n";
        log_file_.flush();
    }

    std::thread worker_;
    std::mutex mutex_;
    std::mutex file_mutex_;
    std::condition_variable cv_;
    std::queue<LogEntry> queue_;
    std::ofstream log_file_;
    std::atomic<bool> running_{false};
    std::string client_id_{"unknown"};
};

} // namespace sysdb

#endif // ACCESS_LOGGER_HPP