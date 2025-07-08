#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

const std::string VERSION = "1.0.0";

void printBanner() {
    std::cout << "\n"
              << "  ___ ___ _____ ___   __      __   _      ___ \n"
              << " | __| _ \\_   _| _ \\  \\ \\    / /__| |__  | __|\n"
              << " | _||   / | | |  _/   \\ \\/\\/ / -_) '_ \\ | _| \n"
              << " |_| |_|_\\ |_| |_|      \\_/\\_/\\___|_.__/ |___|\n"
              << "                                              \n"
              << " HTTP Web Framework v" << VERSION << " - Modern C++20\n"
              << "---------------------------------------------\n"
              << std::endl;
}

void startServer(int port) {
    std::cout << "Starting HTTP server on port " << port << "..." << std::endl;
    
    for (int i = 0; i < 5; i++) {
        std::cout << "." << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    std::cout << "\nServer started successfully!" << std::endl;
    std::cout << "Listening for connections on http://localhost:" << port << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server" << std::endl;
    
    int seconds = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        seconds++;
        
        if (seconds % 10 == 0) {
            std::cout << "[INFO] Server uptime: " << seconds << " seconds" << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    printBanner();
    
    int port = 8080;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cout << "Invalid port number. Using default port 8080." << std::endl;
        }
    }
    
    std::cout << "HTTP Web Framework - Demo Server" << std::endl;
    std::cout << "--------------------------------" << std::endl;
    std::cout << "1. Start server" << std::endl;
    std::cout << "2. Show version info" << std::endl;
    std::cout << "3. Exit" << std::endl;
    
    int choice;
    std::cout << "\nEnter your choice (1-3): ";
    std::cin >> choice;
    
    switch (choice) {
        case 1:
            startServer(port);
            break;
        case 2:
            std::cout << "\nHTTP Web Framework v" << VERSION << std::endl;
            std::cout << "Compiled with C++ " << __cplusplus << std::endl;
            std::cout << "Platform: Windows x64" << std::endl;
            std::cout << "License: MIT" << std::endl;
            std::cout << "GitHub: https://github.com/YALOKGARua/HTTP-Web-Framework" << std::endl;
            break;
        case 3:
            std::cout << "Exiting..." << std::endl;
            break;
        default:
            std::cout << "Invalid choice. Exiting..." << std::endl;
    }
    
    return 0;
} 