/* 
 * File:   profiler.h
 * Author: sonny
 *
 * Created on January 18, 2017, 10:07 AM
 */

#ifndef PROFILER_H
#define PROFILER_H

#include <chrono>
#include <map>
#include <thread>
#include <mutex>

struct ProfilerTask {
    long long time_pass;
    long long count;
    long last_time_pass;

    ProfilerTask() : time_pass(0), count(0), last_time_pass(0) {
    }
};

class Task {
public:
    Task(const std::string& name);
    ~Task();

private:
    std::string _name;
    std::chrono::steady_clock::time_point _time_point_start;
};

class Profiler {
public:
    static bool _destroyed;

public:
    static Profiler& getInstance();
    void addProfiler(const std::string& task_name, long ms);
    void reset(const std::string& task_name);

    ProfilerTask get(const std::string& task_name);
    const std::map<std::string, ProfilerTask>& getAll() const;

private:
    std::map<std::string, ProfilerTask> _map_table;
    std::mutex _mutex;

private:
    Profiler();
    Profiler(const Profiler&);
    Profiler& operator=(const Profiler&);
    ~Profiler();
};

#define _QUOTE(x) # x
#define QUOTE(x) _QUOTE(x)

#define PROFILER_TASK(x) Task task(QUOTE(x))
#define PROFILER_GET(x) Profiler::getInstance().get(QUOTE(x))
#define PROFILER_GETALL Profiler::getInstance().getAll()

#endif /* PROFILER_H */

