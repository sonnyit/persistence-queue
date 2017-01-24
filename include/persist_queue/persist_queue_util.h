/* 
 * File:   persist_queue_util.h
 * Author: sonny
 *
 * Created on January 5, 2017, 2:19 PM
 */

#ifndef PERSIST_QUEUE_UTIL_H
#define PERSIST_QUEUE_UTIL_H

namespace zqueue_util {
    long long memtoll(const char* p, long long defaul_num);
    std::string collapseUnit(const long long inBytes);
    long long getFileSize(const std::string& fileName);
    bool checkFileExist(const std::string& fileName);
    std::string getExePath();
    unsigned parse_line(char* line);
    unsigned get_vmrss();
    unsigned get_vmsize();
    unsigned get_pid();
}

#endif /* PERSIST_QUEUE_UTIL_H */

