#pragma once
#include <windows.h>
#include <iostream>

// Thread function must have this signature
DWORD WINAPI MyThreadFunction(LPVOID lpParam) {
    int value = *reinterpret_cast<int*>(lpParam);
    std::cout << "Thread running with value: " << value << std::endl;

    // Simulate work
    for (int i = 0; i < value; i++) {
        std::cout << "Thread loop #" << i << std::endl;
        Sleep(500); // Sleep 500 ms
    }

    std::cout << "Thread finished." << std::endl;
    return 0; // Exit code
}

int mainThhhh() {
    int param = 42;

    // Create thread
    HANDLE hThread = CreateThread(
        NULL,                // default security attributes
        0,                   // default stack size
        MyThreadFunction,    // thread function
        &param,              // parameter to thread function
        0,                   // default creation flags (run immediately)
        NULL                 // thread ID (optional)
    );

    if (hThread == NULL) {
        std::cerr << "Failed to create thread!" << std::endl;
        return 1;
    }

    std::cout << "Main thread continues while worker runs..." << std::endl;

    // Wait for thread to finish
    WaitForSingleObject(hThread, INFINITE);

    // Close thread handle
    CloseHandle(hThread);

    std::cout << "Main thread exiting." << std::endl;
    return 0;
}
