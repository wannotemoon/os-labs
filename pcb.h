//
// Created by 28794 on 2026/9/3.
//

#ifndef OS_PCB_H
#define OS_PCB_H

#include <string>
#include <vector>
#include <queue>
using namespace std;

enum ProcessState {
    READY,
    RUNNING,
    BLOCKED
};

struct PageTableEntry {
    bool present=false;
    int frameNo=-1;
    int swapNo=-1;
    bool modified=false;
};

struct PCB {
    int pid;
    string name;
    ProcessState state;

    int memStart;
    int memSize;

    PCB* next;

    //lab2
    int sizeBytes=0;
    int residentLimit=0;

    vector<PageTableEntry> pageTable{};
    queue<int> fifoPages{};

    unsigned long long accessCount=0;
    unsigned long long pageFaultCount=0;
    unsigned long long replacementCount=0;

};

#endif //OS_PCB_H
