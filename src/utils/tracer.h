#pragma once

class ChromeProfiler {
public:
    enum class EventType {
        Begin = 'B',
        End = 'E',
        Complete = 'X',
        Instant = 'i',
        Counter = 'C',
        AsyncStart = 'b',
        AsyncEnd = 'e'
    };

    struct Event {
        std::string name;
        std::string category;
        EventType type;
        std::chrono::high_resolution_clock::time_point timestamp;
        std::chrono::microseconds duration;
        std::thread::id thread_id;
        uint64_t process_id;
        std::string args;

        Event(const std::string& n, const std::string& cat, EventType t,
              std::chrono::high_resolution_clock::time_point ts,
              std::chrono::microseconds dur = std::chrono::microseconds(0))
            : name(n), category(cat), type(t), timestamp(ts), duration(dur),
              thread_id(std::this_thread::get_id()), process_id(getProcessId()) {}
    };

private:
    std::vector<Event> events;
    std::mutex events_mutex;
    std::chrono::high_resolution_clock::time_point start_time;
    bool is_recording;

    static uint64_t getProcessId() {
        #ifdef _WIN32
            return GetCurrentProcessId();
        #else
            return getpid();
        #endif
    }

    uint64_t getMicrosecondsSinceStart(std::chrono::high_resolution_clock::time_point timestamp) const {
        auto elapsed = timestamp - start_time;
        return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    }

    std::string threadIdToString(std::thread::id id) const {
        std::stringstream ss;
        ss << id;
        return ss.str();
    }

    std::string escapeJson(const std::string& str) const {
        std::string escaped;
        escaped.reserve(str.length());
        for (char c : str) {
            switch (c) {
                case '"':  escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b"; break;
                case '\f': escaped += "\\f"; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:   escaped += c; break;
            }
        }
        return escaped;
    }

public:
    static ChromeProfiler& getInstance() {
        static ChromeProfiler instance;
        return instance;
    }

    ChromeProfiler() : start_time(std::chrono::high_resolution_clock::now()), is_recording(true) {
        events.reserve(10000); // Pre-allocate for better performance
    }

    void startRecording() {
        std::lock_guard<std::mutex> lock(events_mutex);
        is_recording = true;
        start_time = std::chrono::high_resolution_clock::now();
        events.clear();
    }

    void stopRecording() {
        std::lock_guard<std::mutex> lock(events_mutex);
        is_recording = false;
    }

    void addEvent(const std::string& name, const std::string& category, EventType type,
                  const std::string& args = "") {
        if (!is_recording) return;

        auto now = std::chrono::high_resolution_clock::now();
        std::lock_guard<std::mutex> lock(events_mutex);
        events.emplace_back(name, category, type, now);
        if (!args.empty()) {
            events.back().args = args;
        }
    }

    void addCompleteEvent(const std::string& name, const std::string& category,
                         std::chrono::high_resolution_clock::time_point start,
                         std::chrono::high_resolution_clock::time_point end,
                         const std::string& args = "") {
        if (!is_recording) return;

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::lock_guard<std::mutex> lock(events_mutex);
        events.emplace_back(name, category, EventType::Complete, start, duration);
        if (!args.empty()) {
            events.back().args = args;
        }
    }

    void addCounterEvent(const std::string& name, const std::string& category,
                        int64_t value, const std::string& counter_name = "value") {
        if (!is_recording) return;

        std::string args = "{\"" + counter_name + "\":" + std::to_string(value) + "}";
        addEvent(name, category, EventType::Counter, args);
    }

    bool writeToFile(const std::string& filename) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(events_mutex));
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        file << "{\n";
        file << "  \"traceEvents\": [\n";

        for (size_t i = 0; i < events.size(); ++i) {
            const auto& event = events[i];
            
            file << "    {\n";
            file << "      \"name\": \"" << escapeJson(event.name) << "\",\n";
            file << "      \"cat\": \"" << escapeJson(event.category) << "\",\n";
            file << "      \"ph\": \"" << static_cast<char>(event.type) << "\",\n";
            file << "      \"ts\": " << getMicrosecondsSinceStart(event.timestamp) << ",\n";
            file << "      \"pid\": " << event.process_id << ",\n";
            file << "      \"tid\": \"" << threadIdToString(event.thread_id) << "\"";

            if (event.type == EventType::Complete) {
                file << ",\n      \"dur\": " << event.duration.count();
            }

            if (!event.args.empty()) {
                file << ",\n      \"args\": " << event.args;
            }

            file << "\n    }";
            if (i < events.size() - 1) {
                file << ",";
            }
            file << "\n";
        }

        file << "  ],\n";
        file << "  \"displayTimeUnit\": \"ms\",\n";
        file << "  \"systemTraceEvents\": \"SystemTraceData\",\n";
        file << "  \"otherData\": {\n";
        file << "    \"version\": \"Chrome Profiler C++ v1.0\"\n";
        file << "  },\n";
        file << "  \"stackFrames\": {},\n";
        file << "  \"samples\": []\n";
        file << "}\n";

        return true;
    }

    size_t getEventCount() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(events_mutex));
        return events.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(events_mutex);
        events.clear();
    }
};

// RAII Helper class for automatic begin/end events
class ScopedProfiler {
private:
    std::string name;
    std::string category;
    std::chrono::high_resolution_clock::time_point start_time;

public:
    ScopedProfiler(const std::string& name, const std::string& category = "function")
        : name(name), category(category), start_time(std::chrono::high_resolution_clock::now()) {
        ChromeProfiler::getInstance().addEvent(name, category, ChromeProfiler::EventType::Begin);
    }

    ~ScopedProfiler() {
        auto end_time = std::chrono::high_resolution_clock::now();
        ChromeProfiler::getInstance().addEvent(name, category, ChromeProfiler::EventType::End);
        // Also add a complete event for better visualization
        ChromeProfiler::getInstance().addCompleteEvent(name, category, start_time, end_time);
    }
};

// Convenience macros
#define PROFILE_FUNCTION() ScopedProfiler _prof(__FUNCTION__)
#define PROFILE_SCOPE(name) ScopedProfiler _prof(name)
#define PROFILE_SCOPE_CAT(name, category) ScopedProfiler _prof(name, category)


// Example usage file (save as example.cpp)
/*
#include "chrome_profiler.h"
#include <iostream>
#include <thread>
#include <vector>
#include <random>

void expensiveFunction(int iterations) {
    PROFILE_FUNCTION();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1000);
    
    volatile int sum = 0;
    for (int i = 0; i < iterations; ++i) {
        sum += dis(gen);
    }
}

void networkSimulation() {
    PROFILE_SCOPE_CAT("Network Request", "network");
    
    // Simulate network delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    ChromeProfiler::getInstance().addCounterEvent("Bytes Received", "network", 1024);
}

void databaseQuery() {
    PROFILE_SCOPE_CAT("Database Query", "database");
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

void workerThread(int id) {
    PROFILE_SCOPE_CAT("Worker Thread " + std::to_string(id), "threading");
    
    for (int i = 0; i < 5; ++i) {
        {
            PROFILE_SCOPE("Work Item " + std::to_string(i));
            expensiveFunction(10000);
        }
        
        if (i % 2 == 0) {
            networkSimulation();
        } else {
            databaseQuery();
        }
    }
}

int main() {
    auto& profiler = ChromeProfiler::getInstance();
    
    std::cout << "Starting profiling session...\n";
    profiler.startRecording();
    
    {
        PROFILE_SCOPE_CAT("Main Application", "app");
        
        // Single-threaded work
        expensiveFunction(50000);
        networkSimulation();
        databaseQuery();
        
        // Multi-threaded work
        {
            PROFILE_SCOPE_CAT("Multi-threaded Section", "threading");
            
            std::vector<std::thread> threads;
            for (int i = 0; i < 4; ++i) {
                threads.emplace_back(workerThread, i);
            }
            
            for (auto& t : threads) {
                t.join();
            }
        }
        
        // Add some counter events
        for (int i = 0; i < 10; ++i) {
            profiler.addCounterEvent("Memory Usage", "system", 1000 + i * 100, "bytes");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    profiler.stopRecording();
    
    std::cout << "Recorded " << profiler.getEventCount() << " events\n";
    
    if (profiler.writeToFile("profile_trace.json")) {
        std::cout << "Profile saved to profile_trace.json\n";
        std::cout << "Open chrome://tracing in Chrome and load the file to view the profile\n";
    } else {
        std::cout << "Failed to save profile\n";
    }
    
    return 0;
}
*/