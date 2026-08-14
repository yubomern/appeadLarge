#pragma once
#include <windows.h>
#include <iostream>
#include <string>
#include <future>

// Function to open and configure COM port
HANDLE openComPort(const std::string& portName, DWORD baudRate = CBR_9600) {
    HANDLE hSerial = CreateFile((LPCWSTR)portName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Error opening COM port");
    }

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        throw std::runtime_error("Error getting COM state");
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        CloseHandle(hSerial);
        throw std::runtime_error("Error setting COM state");
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(hSerial, &timeouts);

    return hSerial;
}

// Function to write data
void writeComPort(HANDLE hSerial, const std::string& data) {
    DWORD bytesWritten;
    if (!WriteFile(hSerial, data.c_str(), data.size(), &bytesWritten, NULL)) {
        throw std::runtime_error("Error writing to COM port");
    }
}

// Function to read asynchronously using std::future
std::future<std::string> readComPortAsync(HANDLE hSerial) {
    return std::async(std::launch::async, [hSerial]() {
        char buffer[128];
        DWORD bytesRead;
        std::string result;

        if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            buffer[bytesRead] = '\0';
            result = buffer;
        }
        else {
            result = "Error reading from COM port";
        }
        return result;
        });
}

int maintest() {
    try {
        // Use \\.\COM10 for ports above COM9
        HANDLE hSerial = openComPort("\\\\.\\COM3");

        // Write test string
        writeComPort(hSerial, "Hello COM Port!\n");

        // Read asynchronously
        auto futureRead = readComPortAsync(hSerial);

        // Do other work here while reading happens in background...

        // Get result
        std::string received = futureRead.get();
        std::cout << "Received: " << received << std::endl;

        CloseHandle(hSerial);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
