//
// Created by 28794 on 2026/9/3.
//

#ifndef OS_PROCESS_H
#define OS_PROCESS_H

#include <string>
using namespace std;

#include "pcb.h"
#include "pcb_queue.h"
#include "memory.h"

class ProcessManager {
private:
    PCB* running;
    PCBQueue ready;
    PCBQueue blocked;

    MemoryManager memory;

    int nextPid;

    void dispatch();

    void clearProcesses();

public:
    ProcessManager(int memorySize=64);
    ~ProcessManager();

    bool createProcess(const string& name,int size);

    bool timeOut();
    bool blockProcess();
    bool wakeProcess();
    bool terminateProcess();

    void showProcess() const;
    void showMemory() const;

};

#endif //OS_PROCESS_H
