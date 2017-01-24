/* 
 * File:   persist_queue_util.cpp
 * Author: sonny
 *
 * Created on January 5, 2017, 2:23 PM
 */

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace zqueue_util {

    long long memtoll(const char* p, long long defaul_num) {
        const char* u;
        char buf[128];
        long mul = 1;
        double val = 1;
        unsigned int digits;

        u = p;
        while (*u && (isdigit(*u) || *u == '.' || *u == '-')) u++;
        if (*u == '\0' || !strcasecmp(u, "b")) {
            mul = 1;
        } else if (!strcasecmp(u, "k")) {
            mul = 1000;
        } else if (!strcasecmp(u, "kb")) {
            mul = 1024;
        } else if (!strcasecmp(u, "m")) {
            mul = 1000 * 1000;
        } else if (!strcasecmp(u, "mb")) {
            mul = 1024 * 1024;
        } else if (!strcasecmp(u, "g")) {
            mul = 1000L * 1000 * 1000;
        } else if (!strcasecmp(u, "gb")) {
            mul = 1024L * 1024 * 1024;
        } else {
            return defaul_num;
        }

        /* Copy the digits into a buffer, we'll use strtoll() to convert
         * the digit (without the unit) into a number. */
        digits = u - p;
        if (digits >= sizeof (buf)) {
            return defaul_num;
        }
        memcpy(buf, p, digits);
        buf[digits] = '\0';

        char *endptr;
        errno = 0;
        val = strtod(buf, &endptr);
        if ((val == 0 && errno == EINVAL) || *endptr != '\0') {
            return defaul_num;
        }
        return val*mul;
    }

    std::string collapseUnit(const long long inBytes) {
        double size = inBytes;
        std::string unit = "b";

        if (size > 1024 && (!strcasecmp(unit.c_str(), "b"))) {
            size /= 1024;
            unit = "kb";
        }
        if (size > 1024 && (!strcasecmp(unit.c_str(), "kb"))) {
            size /= 1024;
            unit = "mb";
        }
        if (size > 1024 && (!strcasecmp(unit.c_str(), "mb"))) {
            size /= 1024;
            unit = "gb";
        }
        
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << size;
        return ss.str() + unit;
    }

    long long getFileSize(const std::string& fileName) {
        struct stat statbuf;

        if (stat(fileName.c_str(), &statbuf) == -1) {
            return 0;
        }

        return (long long) statbuf.st_size;
    }

    bool checkFileExist(const std::string& fileName) {
        struct stat buffer;
        return (stat(fileName.c_str(), &buffer) == 0);
    }

    std::string getExePath() {
        char result[1024];
        ssize_t count = readlink("/proc/self/exe", result, 1024);
        std::string fullPath = std::string(result, (count > 0) ? count : 0);
        size_t t = fullPath.find_last_of("/");
        return fullPath.substr(0, t) + std::string("/");
    }

    unsigned parse_line(char* line) {
        unsigned length = strlen(line);
        while (*line < '0' || *line > '9')
            ++line;

        line[length - 3] = '\0';
        return atoi(line);
    }

    unsigned get_vmrss() {
        FILE* file = fopen("/proc/self/status", "r");
        unsigned result = 0;
        char line[128];

        while (fgets(line, 128, file) != NULL) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                result = parse_line(line);
                break;
            }
        }
        fclose(file);
        return result;
    }

    unsigned get_vmsize() {
        FILE* file = fopen("/proc/self/status", "r");
        unsigned result = 0;
        char line[128];

        while (fgets(line, 128, file) != NULL) {
            if (strncmp(line, "VmSize:", 7) == 0) {
                result = parse_line(line);
                break;
            }
        }
        fclose(file);
        return result;
    }
    
    unsigned get_pid() {
        FILE* file = fopen("/proc/self/status", "r");
        unsigned result = 0;
        char line[128];

        while (fgets(line, 128, file) != NULL) {
            if (strncmp(line, "Pid:", 4) == 0) {
                result = parse_line(line);
                break;
            }
        }
        fclose(file);
        return result;
    }
}