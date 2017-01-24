/* 
 * File:   persist_queue.h
 * Author: sonny
 *
 * Created on December 29, 2016, 9:59 AM
 */

#ifndef PERSIST_QUEUE_H
#define PERSIST_QUEUE_H

#include <deque>
#include <fstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <climits>
#include <type_traits>
#include <typeinfo>
#include <system_error>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>

#include "spdlog/spdlog.h"
#include "spdlog/logger.h"
#include "spdlog/common.h"

#include "profiler.h"
#include "persist_queue_util.h"

namespace spd = spdlog;

template <class T>
class TPersistQueue {
private:
    typedef std::shared_ptr<T> ItemPtr;
    typedef std::deque<ItemPtr> ItemQueue;
    typedef boost::lock_guard<boost::mutex> LockType;

    struct Item {
        bool _isPush;
        T _item;

        Item() : _isPush(false), _item(T()) {
        }
        
        Item(bool isPush, T item) : _isPush(isPush), _item(item) {
        }

        template <class Archive>
        void serialize(Archive & ar) {
            ar(_isPush, _item);
        }
    };

public:
    /*-----------------------------------------------------------------------------
     * Constructors & Destructor
     *----------------------------------------------------------------------------*/
    //    static TPersistQueue& getInstance();

    TPersistQueue() {
        /* init logger */
        _logger = spdlog::get("console_5sf4a56s4fds");
        if (!_logger) {
            _logger = spd::stdout_color_mt("console_5sf4a56s4fds");
            _logger->set_pattern("[%H:%M:%S] [%l] %v");
            _logger->set_level(spd::level::info);
        }
        //_logger->debug("TPersistQueue constructor called\n");

        _name = "";
        _maxQueueSize = LONG_MAX;

        //_itemQueueBufBin = "";
        _cwd = zqueue_util::getExePath();
        _dbFileName = _cwd + "db.db";
        _dbTmpFileName = _cwd + "dbTmp.db";

        _size2RewriteDb = LONG_LONG_MAX;
        _size2RewriteDbInternal = LONG_LONG_MAX;
        _approximateDbSize = 0;

        _childPID = -1;
        _noForkTime = 0;
    }

    ~TPersistQueue() {
    }

    TPersistQueue& setName(const std::string& name) {
        _name = name;
        _dbFileName = _cwd + _name + ".db";
        _dbTmpFileName = _cwd + _name + "Tmp.db";
        if (_ofsDb.is_open()) {
            _ofsDb.close();
            _ofsDb.open(_dbFileName.c_str(), std::ios_base::binary | std::ios_base::app);
        }
        /* TODO: check if name is empty */
        return *this;
    }

    TPersistQueue& setMaxQueueSize(long size) {
        _maxQueueSize = size;
        return *this;
    }

    TPersistQueue& setSizeToRewriteDb(long long size) {
        _size2RewriteDb = size;
        _size2RewriteDbInternal = size;
        /* TODO: check if size is too small */
        return *this;
    }

    TPersistQueue& setLogLevel(spd::level::level_enum level) {
        _logger->set_level(level);
        return *this;
    }

    /*-----------------------------------------------------------------------------
     * Element access
     *----------------------------------------------------------------------------*/

    /* TODO: does this function need lock mutex? */
    ItemPtr operator[](const int index) {
        return _itemQueue[index];
    }

    /*-----------------------------------------------------------------------------
     * Capacity
     *----------------------------------------------------------------------------*/
    bool empty() const {
        LockType lock(_mutex);
        return _itemQueue.empty();
    }

    bool isFull() const {
        LockType lock(_mutex);
        return _itemQueue.size() >= _maxQueueSize ? true : false;
    }

    size_t size() const {
        LockType lock(_mutex);
        return _itemQueue.size();
    }

    size_t maxSize() const {
        return _maxQueueSize;
    }

    std::string stat() const {
        LockType lock(_mutex);

        std::stringstream ss;
        ss << "# Memory\n";
        ss << "vmrss:" << zqueue_util::collapseUnit((long long) zqueue_util::get_vmrss() * 1024) << "\n";
        ss << "vmsize:" << zqueue_util::collapseUnit((long long) zqueue_util::get_vmsize() * 1024) << "\n";
        ss << "max_num_of_elements:" << _maxQueueSize << "\n";
        ss << "current_num_of_elements:" << _itemQueue.size() << "\n\n";
        ss << "# Disk\n";
        ss << "min_aof_rewrite_size:" << zqueue_util::collapseUnit(_size2RewriteDb) << "\n";
        ss << "min_internal_aof_rewrite_size:" << zqueue_util::collapseUnit(_size2RewriteDbInternal) << "\n";
        ss << "num_of_rewrite_db_times:" << _noForkTime << "\n";
        ss << "path_to_db:" << _dbFileName << "\n";
        return ss.str();
    }

    /*-----------------------------------------------------------------------------
     * Modifiers
     *----------------------------------------------------------------------------*/
    bool enqueue(ItemPtr pItem) {
        LockType lock(_mutex);

        if (_itemQueue.size() >= _maxQueueSize) {
            return false;
        }
        /* in mem */
        _itemQueue.push_back(pItem);
        /* in disk */
        /* if forked, merge and replace db */
        isEndAofProcess();
        /* continue persist to disk */
        serializeToDbFile(true, pItem);
        /* if forking, add step write to buffer */
        if (_childPID != -1) {
            serializeToBuffer(true, pItem);
        }
        /* check is need to rewrite db? */
        isStartAofProcess();

        return true;
    }

    ItemPtr dequeue() {
        LockType lock(_mutex);

        if (_itemQueue.empty())
            return nullptr;

        /* in mem */
        ItemPtr pItem = _itemQueue.front();
        _itemQueue.pop_front();
        /* in disk */
        /* if forked, merge and replace db */
        isEndAofProcess();
        /* continue persist to disk */
        serializeToDbFile(false, nullptr);
        /* if forking, add step write to buffer */
        if (_childPID != -1) {
            serializeToBuffer(false, nullptr);
        }
        /* check is need to rewrite db? */
        isStartAofProcess();

        return pItem;
    }

    void clear() {
        LockType lock(_mutex);
        _itemQueue.clear();
    }

    void syncForClose() {
        LockType lock(_mutex);
        /* TODO optimize */
        while (_childPID != -1) {
            usleep(1000);
            isEndAofProcess();
        }

        if (_ofsDb.is_open()) {
            _ofsDb.close();
        }
    }

    bool loadDb() {
        LockType lock(_mutex);
        PROFILER_TASK(loadDb);

        /* TODO: check */
        _itemQueue.clear();

        Item item;
        std::ifstream ifs(_dbFileName.c_str());
        {
            cereal::BinaryInputArchive ia(ifs);
            try {
                while (ifs) {
                    ia(item);
                    if (item._isPush) {
                        _itemQueue.push_back(std::make_shared<T>(item._item));
                    } else {
                        _itemQueue.pop_front();
                    }
                }
            } catch (cereal::Exception e) {
                _logger->debug("Load db done! Read all data successfully!");
            }
        }

        return false;
    }

private:
    std::string _name;
    long _maxQueueSize;

    ItemQueue _itemQueue;
    std::stringstream _itemQueueBufBin;

    mutable boost::mutex _mutex;

    std::ofstream _ofsDb;
    std::ofstream _ofsTmpDb;

    std::string _cwd;
    std::string _dbFileName;
    std::string _dbTmpFileName;

    long long _size2RewriteDb; /* in byte */
    long long _size2RewriteDbInternal;
    long long _approximateDbSize;

    std::shared_ptr<spd::logger> _logger;
    int _childPID;
    int _noForkTime;

private:



    TPersistQueue(const TPersistQueue&); // don't impl copy constructor
    TPersistQueue& operator=(const TPersistQueue&); //dont impl assigment operator

    /*-----------------------------------------------------------------------------
     * Utility private functions
     *----------------------------------------------------------------------------*/
    void rewriteZPersistQueue() {
        /* currently in child process */
        /* parse main deque and write to temp file */
        if (!_ofsTmpDb.is_open()) {
            _ofsTmpDb.open(_dbTmpFileName.c_str(), std::ios_base::binary);
        }
        /* TODO: check if cannot open file for write */
        for (typename ItemQueue::iterator it = _itemQueue.begin(); it != _itemQueue.end(); ++it) {
            serializeToTmpDbFile(true, *it);
        }
        _ofsTmpDb.close();

        exit(0);
    }

    void isEndAofProcess() {
        if (_childPID != -1) { /* had child process doing aof */
            int status;
            pid_t return_pid = waitpid(_childPID, &status, WNOHANG); /* WNOHANG def'd in wait.h */
            if (return_pid == -1) {
                /* error */
                _logger->critical("rewrite process error state! exit!");
                exit(1);

            } else if (return_pid == 0) {
                /* child is still running */

            } else if (return_pid == _childPID) {
                /* child is finished. exit status in status */
                _childPID = -1; /* IMPORTANT */
                if (status == 0) {
                    long long newDbSize = zqueue_util::getFileSize(_dbTmpFileName) * 2;
                    _size2RewriteDbInternal = newDbSize > _size2RewriteDb ? newDbSize : _size2RewriteDb;
                    _logger->debug("rewrite file {} done success! - with size {}", _dbFileName, zqueue_util::collapseUnit(zqueue_util::getFileSize(_dbTmpFileName)));

                    /* close db to prepare replace */
                    if (_ofsDb.is_open()) {
                        _ofsDb.close();
                    }
                    /* merge from buf to aof temp file */
                    if (!_ofsTmpDb.is_open()) {
                        _ofsTmpDb.open(_dbTmpFileName.c_str(), std::ios_base::app | std::ios_base::binary);
                    }
                    /* TODO: check if open tmp file fail */
                    _ofsTmpDb << _itemQueueBufBin.str();
                    _ofsTmpDb.close();
                    _itemQueueBufBin.clear();

                    /* replace db by new rewrite db */
                    std::string cmd = "mv " + _dbTmpFileName + " " + _dbFileName;
                    system(cmd.c_str());
                    //boost::filesystem::rename(_dbTmpFileName, _dbFileName);

                    /* reassign _approximateDbSize */
                    _approximateDbSize = zqueue_util::getFileSize(_dbFileName);

                } else {
                    _logger->critical("rewrite process terminated fail, status {}!", status);
                }
            } else {
                _logger->critical("rewrite process terminated with unknown pid: {}", return_pid);
            }
        }
    }

    void isStartAofProcess() {
        long long dbSize = zqueue_util::getFileSize(_dbFileName);
        if (dbSize >= _size2RewriteDb && dbSize >= _size2RewriteDbInternal) { /* if file aof large */
            if (_childPID == -1) { // no child process
                _logger->debug("file {} is large. rewrite process start!", _dbFileName);
                _noForkTime++;
                pid_t pid = fork();

                if (pid == 0) {
                    /* child process - do rewrite db */
                    rewriteZPersistQueue();
                } else if (pid > 0) {
                    /* parent process */
                    _childPID = pid;
                    _itemQueueBufBin.clear();
                } else {
                    _logger->critical("fork process to rewrite db fail!");
                    exit(1);
                }
            }
        }
    }

    void serializeToDbFile(bool isPush, ItemPtr pItem) {
        Item item(isPush, *pItem);

        if (!_ofsDb.is_open()) {
            _ofsDb.open(_dbFileName.c_str(), std::ios_base::binary | std::ios_base::app);
        }
        cereal::BinaryOutputArchive oa(_ofsDb);
        oa(item);
    }

    void serializeToTmpDbFile(bool isPush, ItemPtr pItem) {
        Item item(isPush, *pItem);

        if (!_ofsTmpDb.is_open()) {
            _ofsTmpDb.open(_dbTmpFileName.c_str(), std::ios_base::binary | std::ios_base::app);
        }
        cereal::BinaryOutputArchive oa(_ofsTmpDb);
        oa(item);
    }

    void serializeToBuffer(bool isPush, ItemPtr pItem) {
        Item item(isPush, *pItem);
        cereal::BinaryOutputArchive oa(_itemQueueBufBin);
        oa(item);
    }
};

#endif /* PERSIST_QUEUE_H */

