/* 
 * File:   profiler.cpp
 * Author: sonny
 *
 * Created on January 18, 2017, 10:07 AM
 */

#include <stdexcept>
#include "profiler.h"

using namespace std;

/*-----------------------------------------------------------------------------
 * Task class
 *----------------------------------------------------------------------------*/

Task::Task(const std::string& name) : _name(name) {
    _time_point_start = std::chrono::steady_clock::now();
}

Task::~Task() {
    Profiler::getInstance().addProfiler(_name, chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - _time_point_start).count());
}

/*-----------------------------------------------------------------------------
 * Profiler class
 *----------------------------------------------------------------------------*/
bool Profiler::_destroyed = false;

Profiler::Profiler() {
}

Profiler::~Profiler() {
    _destroyed = true;
}

Profiler& Profiler::getInstance() {
    static Profiler instance;
    if (_destroyed) {
        throw std::runtime_error("Profiler: Dead reference access");
    }
    return instance;
}

void Profiler::reset(const std::string& task_name) {
    lock_guard<mutex> lock(_mutex);
    if (_map_table.find(task_name) != _map_table.end()) { /* found -> reset task */
        ProfilerTask task;
        _map_table[task_name] = task;
    }
}

void Profiler::addProfiler(const std::string& task_name, long ms) {
    lock_guard<mutex> lock(_mutex);
    if (_map_table.find(task_name) != _map_table.end()) {
        ++_map_table[task_name].count;
        _map_table[task_name].time_pass += ms;
        _map_table[task_name].last_time_pass = ms;
    } else {
        ProfilerTask task;
        ++task.count;
        task.time_pass += ms;
        task.last_time_pass = ms;
        _map_table[task_name] = task;
    }
}

const std::map<std::string, ProfilerTask>& Profiler::getAll() const {
    return _map_table;
}

ProfilerTask Profiler::get(const std::string& task_name) {
    lock_guard<mutex> lock(_mutex);
    if (_map_table.find(task_name) != _map_table.end()) {
        return _map_table[task_name];
    }
    return ProfilerTask();
}
