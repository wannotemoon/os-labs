//
// Created by 28794 on 2026/9/3.
//

#ifndef OS_PCB_H
#define OS_PCB_H

#include <string>
using namespace std;

enum ProcessState {
    READY,
    RUNNING,
    BLOCKED
};

struct PCB {
    int pid;
    string name;
    ProcessState state;

    int memStart;
    int memSize;

    PCB* next;
};

#endif //OS_PCB_H
