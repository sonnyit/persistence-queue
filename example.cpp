/* 
 * File:   example.cpp
 * Author: minhnh3
 *
 * Created on January 24, 2017, 11:18 AM
 */

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
    
    TPersistQueue<string> pqueueString;
    pqueueString.setName("string").setSizeToRewriteDb(1024).setLogLevel(spd::level::debug);
    pqueueString.loadDb();
    
    for (int i = 0; i < pqueueString.size(); ++i) {
        cout << *pqueueString[i] << " ";
    }
    cout << endl;
    
    for (int i = 0; i < 1000; ++i) {
        pqueueString.enqueue(make_shared<string>("number_" + to_string(i)));
    }
    pqueueString.syncForClose();
    
    return 0;
}

