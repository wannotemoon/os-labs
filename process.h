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

    // lab4：准备时已分配分页资源，但未到达的任务只放pending，不参与dispatch。
    // finished中的进程已回收分页资源，只保留PCB用于后续时间统计。
    PCBQueue pending;
    PCBQueue finished;
    long long currentTime=0;
    bool schedulingStarted=false;

    MemoryManager memory;

    int nextPid;

    void dispatch();
    // 清理所有进程：pending/ready/blocked/running先回收资源再删除，finished只删除。
    // 清理后重置模拟时间和开始标志，不重置nextPid。
    void clearProcesses();
    const PCB* findProcess(int pid) const;

public:
    ProcessManager(int memorySizeBytes=64*1024,
                   int swapSizeBytes=128*1024,
                   int pageSizeBytes=1024);
    ~ProcessManager();

    bool createProcess(const string& name,int sizeBytes,int residentPages=-1);

    // lab4：实验四入口使用独立实例，不与原手动进程操作混用。
    // 到达时间>=0、服务时间>0；创建时预分配资源，成功后仅按到达时间放入pending。
    // 拒绝已开始的模拟，或已有running/ready/blocked/finished内容的对象。
    // 失败不消耗PID、不入队，也不撤销之前成功准备的任务；不推进时间或调用dispatch。
    bool createScheduledProcess(const string& name,int sizeBytes,
                                long long arrivalTime,long long burstTime,int residentPages=-1);

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
