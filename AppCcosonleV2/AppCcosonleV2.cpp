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
#include <future>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <string>


// ---------------- Serial COM Test ----------------


#pragma comment(lib,"ws2_32")

// ---------------- Thread Pool ----------------
class ThreadPool {
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{ false };

public:
    ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; i++) {
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

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stop_.store(true);
        }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }
};

// ---------------- Asynchronous Logger ----------------
class Logger {
private:
    std::string filename_;
    std::ofstream file_;
    ThreadPool pool_{ 2 }; // dedicated pool for logging
    std::mutex mtx_;

    Logger() = default;
    ~Logger() { if (file_.is_open()) file_.close(); }

public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static Logger& Instance() {
        static Logger instance;
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

// ---------------- Directory Watcher ----------------
void WatchDirectory(const std::wstring& directoryPath) {
    HANDLE hDir = CreateFileW(
        directoryPath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        Logger::Instance().error("Error opening directory handle.");
        return;
    }

    std::vector<BYTE> buffer(1024 * 64); // 64 KB buffer
    DWORD bytesReturned = 0;

    Logger::Instance().info("Watching directory...");

    while (true) {
        BOOL success = ReadDirectoryChangesW(
            hDir,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            NULL,
            NULL
        );

        if (!success) {
            Logger::Instance().error("ReadDirectoryChangesW failed.");
            break;
        }

        if (bytesReturned == 0) continue;

        auto* notification = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
        while (notification != nullptr) {
            int nameLength = notification->FileNameLength / sizeof(WCHAR);
            std::wstring fileName(notification->FileName, nameLength);

            Logger::Instance().info("File event: " + std::string(fileName.begin(), fileName.end()));

            if (notification->NextEntryOffset == 0) {
                notification = nullptr;
            }
            else {
                notification = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<BYTE*>(notification) + notification->NextEntryOffset
                    );
            }
        }
    }

    CloseHandle(hDir);
}

// ---------------- Socket Server ----------------
class SocketServer {
private:
    SOCKET server_fd;
    sockaddr_in address;

    SocketServer() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(8081);

        bind(server_fd, (struct sockaddr*)&address, sizeof(address));
        listen(server_fd, SOMAXCONN);
    }

    ~SocketServer() {
        if (server_fd >= 0) closesocket(server_fd);
        WSACleanup();
    }

public:
    SocketServer(const SocketServer&) = delete;
    SocketServer& operator=(const SocketServer&) = delete;

    static SocketServer& getInstance() {
        static SocketServer instance;
        return instance;
    }

    void run(ThreadPool& pool) {
        Logger::Instance().info("Server listening on port 8081...");
        while (true) {
            int addrlen = sizeof(address);
            SOCKET new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
            if (new_socket >= 0) {
                pool.enqueue([new_socket] {
                    std::string msg = "Hello from server!";
                    send(new_socket, msg.c_str(), msg.size(), 0);
                    closesocket(new_socket);
                    });
            }
        }
    }
};
class SerialPort {
private:
    HANDLE hSerial;
    std::string portName;

public:
    SerialPort(const std::string& port) : portName(port), hSerial(INVALID_HANDLE_VALUE) {}

    bool open() {
        hSerial = CreateFileA(
            portName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hSerial == INVALID_HANDLE_VALUE) {
            Logger::Instance().error("Failed to open COM port: " + portName);
            return false;
        }

        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(hSerial, &dcbSerialParams)) {
            Logger::Instance().error("Error getting COM state.");
            return false;
        }

        dcbSerialParams.BaudRate = CBR_9600;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;

        if (!SetCommState(hSerial, &dcbSerialParams)) {
            Logger::Instance().error("Error setting COM state.");
            return false;
        }

        Logger::Instance().info("COM port opened successfully: " + portName);
        return true;
    }

    void write(const std::string& data) {
        DWORD bytesWritten;
        if (!WriteFile(hSerial, data.c_str(), (DWORD)data.size(), &bytesWritten, NULL)) {
            Logger::Instance().error("Failed to write to COM port.");
        }
        else {
            Logger::Instance().info("Sent to COM: " + data);
        }
    }

    void read() {
        char buffer[128];
        DWORD bytesRead;
        if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            buffer[bytesRead] = '\0';
            Logger::Instance().info("Received from COM: " + std::string(buffer));
        }
        else {
            Logger::Instance().error("Failed to read from COM port.");
        }
    }

    void close() {
        if (hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
            Logger::Instance().info("COM port closed.");
        }
    }
};


class DBConnection {

private :
   
    DBConnection(const DBConnection&) = delete;
    DBConnection& operator=(const DBConnection&) = delete;

    DBConnection(DBConnection&&) = delete;
    DBConnection& operator=(DBConnection&&) = delete;

    // Private static instance
    static std::shared_ptr<DBConnection> instance;
public :
    DBConnection() { std::cout << "DBCOnnection " << std::endl; }
    static std::shared_ptr<DBConnection> getInstance() {
        if (!DBConnection::instance) {
            DBConnection::instance = std::make_shared<DBConnection>();
        }
        return DBConnection::instance;
    }
};
std::shared_ptr<DBConnection> DBConnection::instance = nullptr;


#include "Main.h"
#include "Client.h"
#include "ThreadPool.h"
#include "CraeetTh.h"
// ---------------- Main ----------------
int mainv2222() {
    Logger& logger = Logger::Instance();
    logger.setFile("log.txt");

    ThreadPool pool(4);

    std::shared_ptr<DBConnection> db1 = DBConnection::getInstance();
 

    // Get the singleton instance again and use it
    std::shared_ptr<DBConnection> db2 = DBConnection::getInstance();
    if (db1 == db2) std::cout << "saome ref" << std::endl;

    std::future<void> comFuture = std::async(std::launch::async, [&logger] {
        SerialPort com("COM3"); // Adjust to your port
        if (com.open()) {
            com.write("Hello Device!\r\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            com.read();
            com.close();
        }
        });


    maindata(0, nullptr);
    // Keep your server, watcher, and tasks running
    logger.info("Main thread continues with server + watcher + regex tasks...");

    comFuture.get(); // Wait for COM test to finish
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    mainClient(0, nullptr);
    // Start server in background thread
    std::thread serverThread([&pool] {
        SocketServer::getInstance().run(pool);
        });

    // Async directory watcher
    std::wstring folderToWatch = L".";
    std::future<void> watchfut = std::async(std::launch::async, WatchDirectory, folderToWatch);

    logger.info("Main thread continues running...");

    // Example async tasks
    for (int i = 0; i < 5; i++) {
        pool.enqueue([i, &logger] {
            logger.info("Processing task #" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            logger.info("Finished task #" + std::to_string(i));
            });
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));
    logger.error("Main thread completed all tasks.");

    serverThread.join();
    watchfut.get();

    return 0;
}




int main() {
    Logger& logger = Logger::Instance();
    logger.setFile("log.txt");

    ThreadPool pool(4);

    std::shared_ptr<DBConnection> db1 = DBConnection::getInstance();
    std::shared_ptr<DBConnection> db2 = DBConnection::getInstance();
    if (db1 == db2) std::cout << "same ref" << std::endl;

    // Async COM test
    std::future<void> comFuture = std::async(std::launch::async, [&logger] {
        SerialPort com("COM3"); // Adjust to your port
        if (com.open()) {
            com.write("Hello Device!\r\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            com.read();
            com.close();
        }
        });

    // Run maindata asynchronously
    std::future<void> dataFuture = std::async(std::launch::async, [] {
        maindata(0, nullptr);
        });

    // Run mainClient asynchronously
    std::future<void> clientFuture = std::async(std::launch::async, [] {
        mainClient(0, nullptr);
        });

    logger.info("Main thread continues with server + watcher + regex tasks...");

    // Wait for COM test
    comFuture.get();

    // Start server in background thread
    std::thread serverThread([&pool] {
        SocketServer::getInstance().run(pool);
        });
    mainThhhh();

    std::future<int> _ml = std::async(std::launch::async, mainLog);

    // Async directory watcher
    std::wstring folderToWatch = L".";
    std::future<void> watchfut = std::async(std::launch::async, WatchDirectory, folderToWatch);
    _ml.get();
    // Example async tasks
    for (int i = 0; i < 5; i++) {
        pool.enqueue([i, &logger] {
            logger.info("Processing task #" + std::to_string(i));
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            logger.info("Finished task #" + std::to_string(i));
            });
    }

    // Synchronize maindata + mainClient before shutdown
    dataFuture.get();
    clientFuture.get();

    std::this_thread::sleep_for(std::chrono::seconds(10));
    logger.error("Main thread completed all tasks.");

    serverThread.join();
    watchfut.get();

    return 0;
}

