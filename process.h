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
    const PCB* findProcess(int pid) const;

public:
    ProcessManager(int memorySizeBytes=64*1024,
                   int swapSizeBytes=128*1024,
                   int pageSizeBytes=1024);
    ~ProcessManager();

    bool createProcess(const string& name,int sizeBytes,int residentPages=-1);

    bool timeOut();
    bool blockProcess();
    bool wakeProcess();
    bool terminateProcess();

    bool accessCurrent(int logicalAddress,bool isWrite,int& physicalAddress);

    void showProcess() const;
    void showMemory() const;
    bool showPageTable(int pid) const;
    bool showStatistics(int pid) const;
};

#endif
