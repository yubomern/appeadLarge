#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <sstream>
#include <regex>
#include <chrono>

class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stop{ false };

public:
    ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; i++) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lk(mtx_);
                        cv_.wait(lk, [this] { return stop.load() || !tasks.empty(); });
                        if (stop.load() && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
                });
        }
    }

    template<class F> void enqueue(F&& f) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            tasks.emplace(std::forward<F>(f));
        }
        cv_.notify_one();
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stop.store(true);
        }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }
};

// 🔍 Function to search regex in a line
void searchInLine(const std::string& line, const std::regex& pattern) {
    if (std::regex_search(line, pattern)) {
        std::cout << "[MATCH] " << line << std::endl;
    }
    // Simulate heavy work
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

int main() {
    ThreadPool pool(4);
    std::ifstream file("log.txt");
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return 1;
    }

    const size_t CHUNK = 64 * 1024;
    std::vector<char> buffer(CHUNK);
    std::regex pattern("Failed|FAIL"); // Example regex

    while (file.read(buffer.data(), CHUNK) || file.gcount() > 0) {
        size_t bytesRead = file.gcount();
        pool.enqueue([buf = std::vector<char>(buffer.begin(), buffer.begin() + bytesRead), pattern] {
            std::istringstream iss(std::string(buf.begin(), buf.end()));
            std::string line;
            while (std::getline(iss, line)) {
                searchInLine(line, pattern);
            }
            });
    }
}
