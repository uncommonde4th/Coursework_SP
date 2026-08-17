#ifndef TELEMETRY_HPP
#define TELEMETRY_HPP

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace sysdb {

struct MetricsSnapshot {
    double current_rps;        // Текущий RPS (за последнюю секунду)
    double avg_rps_10min;      // Средний RPS за 10 минут
    double max_rps_10min;      // Максимальный RPS за 10 минут
    double avg_latency_10sec;  // Среднее время обработки за 10 секунд (мс)
    int64_t error_count_1min;  // Количество ошибок за последнюю минуту
};

class TelemetryCollector {
public:
    static TelemetryCollector& instance() {
        static TelemetryCollector collector;
        return collector;
    }

    void start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return;
        running_ = true;
        worker_ = std::thread([this]() { backgroundLoop(); });
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_one();
        if (worker_.joinable()) worker_.join();
    }

    // Вызывается после каждого запроса
    void recordRequest(double latency_ms, bool is_error) {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(data_mutex_);

        rps_events_.push_back(now);
        latency_events_.push_back({now, latency_ms});
        if (is_error) {
            error_events_.push_back(now);
        }
    }

    MetricsSnapshot getSnapshot() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(data_mutex_);

        MetricsSnapshot snap{};

        // Текущий RPS: события за последнюю 1 секунду
        auto one_sec_ago = now - std::chrono::seconds(1);
        snap.current_rps = static_cast<double>(
            std::count_if(rps_events_.begin(), rps_events_.end(),
                [one_sec_ago](const auto& t) { return t >= one_sec_ago; }));

        // Средний и макс RPS за 10 минут
        auto ten_min_ago = now - std::chrono::minutes(10);
        int64_t count_10min = std::count_if(rps_events_.begin(), rps_events_.end(),
            [ten_min_ago](const auto& t) { return t >= ten_min_ago; });
        snap.avg_rps_10min = count_10min / 600.0;

        // Макс RPS за 10 минут: считаем по секундным окнам
        snap.max_rps_10min = computeMaxRps(ten_min_ago, now);

        // Среднее время обработки за 10 секунд
        auto ten_sec_ago = now - std::chrono::seconds(10);
        double sum_latency = 0.0;
        int64_t lat_count = 0;
        for (const auto& ev : latency_events_) {
            if (ev.first >= ten_sec_ago) {
                sum_latency += ev.second;
                lat_count++;
            }
        }
        snap.avg_latency_10sec = (lat_count > 0) ? (sum_latency / lat_count) : 0.0;

        // Ошибки за последнюю минуту
        auto one_min_ago = now - std::chrono::minutes(1);
        snap.error_count_1min = std::count_if(error_events_.begin(), error_events_.end(),
            [one_min_ago](const auto& t) { return t >= one_min_ago; });

        return snap;
    }

private:
    TelemetryCollector() = default;
    ~TelemetryCollector() { stop(); }
    TelemetryCollector(const TelemetryCollector&) = delete;
    TelemetryCollector& operator=(const TelemetryCollector&) = delete;

    // Фоновый поток для очистки устаревших данных
    void backgroundLoop() {
        while (running_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait_for(lock, std::chrono::seconds(1), [this]() { return !running_; });
            }
            if (!running_) break;
            cleanup();
        }
    }

    void cleanup() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(data_mutex_);

        auto ten_min_ago = now - std::chrono::minutes(10);
        auto ten_sec_ago = now - std::chrono::seconds(10);
        auto one_min_ago = now - std::chrono::minutes(1);

        while (!rps_events_.empty() && rps_events_.front() < ten_min_ago)
            rps_events_.pop_front();
        while (!latency_events_.empty() && latency_events_.front().first < ten_sec_ago)
            latency_events_.pop_front();
        while (!error_events_.empty() && error_events_.front() < one_min_ago)
            error_events_.pop_front();
    }

    double computeMaxRps(std::chrono::steady_clock::time_point from,
                         std::chrono::steady_clock::time_point to) {
        if (rps_events_.empty()) return 0.0;

        double max_rps = 0.0;
        // Проходим по всем событиям и считаем количество событий в окне 1 сек начиная от каждого
        for (auto it = rps_events_.begin(); it != rps_events_.end(); ++it) {
            if (*it < from) continue;
            auto window_end = *it + std::chrono::seconds(1);
            int64_t count = 0;
            for (auto jt = it; jt != rps_events_.end() && *jt < window_end; ++jt) {
                count++;
            }
            max_rps = std::max(max_rps, static_cast<double>(count));
        }
        return max_rps;
    }

    std::thread worker_;
    std::mutex mutex_;
    std::mutex data_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};

    std::deque<std::chrono::steady_clock::time_point> rps_events_;
    std::deque<std::pair<std::chrono::steady_clock::time_point, double>> latency_events_;
    std::deque<std::chrono::steady_clock::time_point> error_events_;
};

} // namespace sysdb

#endif // TELEMETRY_HPP