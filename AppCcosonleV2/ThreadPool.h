#pragma once
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>
#include <string>

class SafeThread {



private  :
	std::vector<std::thread> workers_;
	std::queue<std::function<void()>> tasks_;
	std::mutex mtx_;
	std::condition_variable cv_;
	std::atomic<bool> stop_{ false };

public  :

	SafeThread(size_t threads_) {
        for (size_t i = 0; i < threads_; i++) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lk(mtx_);
                        cv_.wait(lk, [this] { return stop_.load() || !tasks_.empty(); });
                        if (stop_.load() && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    try {
                        task();
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Task exception: " << e.what() << std::endl;
                    }
                }
                });
        }
	}

    template<class F>
    void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            tasks_.emplace(std::forward<F>(f));
        }
        cv_.notify_one();
    }

    ~SafeThread() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stop_.store(true);
        }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }

};

class LoggerV2 {
private:
    std::string filename_;
    std::ofstream file_;
    SafeThread pool_{ 2 }; // dedicated pool for logging
    std::mutex mtx_;

    LoggerV2() = default;
    ~LoggerV2() { if (file_.is_open()) file_.close(); }

public:
    LoggerV2(const LoggerV2&) = delete;
    LoggerV2& operator=(const LoggerV2&) = delete;

    static LoggerV2& Instance() {
        static LoggerV2 instance;
        return instance;
    }

    void setFile(const std::string& filename) {
        std::lock_guard<std::mutex> lk(mtx_);
        filename_ = filename;
        file_.open(filename_, std::ios::out | std::ios::trunc);
        if (!file_.is_open()) {
            std::cerr << "Logger error: Could not open file." << std::endl;
        }
    }

    void log(const std::string& message) {
        pool_.enqueue([this, message] {
            std::lock_guard<std::mutex> lk(mtx_);
            std::cout << "[LOG] " << message << std::endl;
            if (file_.is_open()) {
                file_ << message << std::endl;
                file_.flush();
            }
            });
    }

    void info(const std::string& msg) { log("INFO: " + msg); }
    void error(const std::string& msg) { log("ERROR: " + msg); }
};
int mainLog() {
    LoggerV2& logger = LoggerV2::Instance();
    logger.setFile("log.txt");

    SafeThread pool(4);

    // Example async tasks
    for (int i = 0; i < 10; i++) {
        pool.enqueue([i, &logger] {
            logger.info("Processing task #" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            logger.info("Finished task #" + std::to_string(i));
            });
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
    logger.error("Main thread completed all tasks.");

    return 0;
}
