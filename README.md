# Info

All necessary file in include/persist_queue

This project implement a persistence queue. When u enqueue or dequeue, it will serialize that item to filesystem. Then later in the future, u can reload the queue from db file with loadDb() function.

# Dependency

* boost system
* boost filesystem
* [cereal](http://uscilab.github.io/cereal/) for serialize queue's item to filesystem
* [spdlog](https://github.com/gabime/spdlog) for logger
* pthreads

# Build

```
cd build
cmake ..
make
./example
```

# Example

```
#include <cstdlib>
#include <string>
#include <iostream>
#include "include/persist_queue/persist_queue.h"

using namespace std;

int main(int argc, char** argv) {
    TPersistQueue<int> pqueue;
    pqueue.setName("int").setSizeToRewriteDb(1024).setLogLevel(spd::level::debug);
    pqueue.loadDb();
    
    for (int i = 0; i < pqueue.size(); ++i) {
        cout << *pqueue[i] << " ";
    }
    cout << endl;
    
    for (int i = 0; i < 1000; ++i) {
        pqueue.enqueue(make_shared<int>(i));
    }
    pqueue.syncForClose();
}
```

Let see example.cpp file for more.

# More detail

## Config

You can set config for this queue:

```
.setName("dbName") /* for queue name, and db name (dbName.db) */
.setMaxQueueSize(long size)
.setSizeToRewriteDb(long long size) /* in bytes, when db file over this size, queue will fork another
																			 process to rewrite db */
.setLogLevel(spd::level::level_enum level) /* see spd repo for more detail about how to using this logger */
```

(will update for info later..)
