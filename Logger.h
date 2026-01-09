#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>

class Logger
{
  public:
    static void log(const std::string &level, const std::string &message)
    {
        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);

        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);

        std::cout << "[" << std::put_time(std::localtime(&now_time), "%H:%M:%S")
                  << "] "
                  << "[" << level << "] " << message << std::endl;
    }

    static void info(const std::string &message) { log("INFO", message); }
    static void error(const std::string &message) { log("ERROR", message); }

    static void logRequest(const std::string &method, const std::string &url)
    {
        info("Incoming Request: " + method + " " + url);
    }
};