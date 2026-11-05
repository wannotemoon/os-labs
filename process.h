#ifndef OS_PROCESS_H
#define OS_PROCESS_H

#include <string>
using namespace std;

#include "pcb.h"
#include "pcb_queue.h"
#include "memory.h"

// lab4：CPU调度策略，与实验二用于页面置换的FIFO是不同概念。
// 使用SchedulingAlgorithm::FCFS/SJF/RR，避免把算法编号和进程状态混用。
enum class SchedulingAlgorithm {
    FCFS,
    SJF,
    RR
};

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

    // 将pending队头中arrivalTime<=currentTime的全部任务依次移入ready，状态改为READY。
    // 只搬运已有节点，不调度、不分配/释放资源、不改时钟或其他时间字段。
    void moveArrivedProcesses();

    // CPU空闲时选择运行者：FCFS/RR取ready队头，SJF取服务时间最短者，成功改为RUNNING。
    // 已有running、ready为空或算法无效时返回false且状态不变；不在此推进执行时间。
    bool dispatchScheduled(SchedulingAlgorithm algorithm);

    // duration为本次实际执行时长，必须>0且不超过剩余服务时间；合法时同时推进两个时间。
    // 无有效运行者、时间字段非法或加法溢出时返回false，全部状态不变。
    // 不自动完成、回队或收取新到达任务。
    bool advanceRunning(long long duration);

    // 仅当running已执行完全部服务时间时，回收分页资源，记录完成时刻并移入finished。
    // 失败保留运行者；成功后running为空，不删除PCB、不立即调度下一个进程。
    bool finishRunningProcess();

    // CPU空闲、ready为空且pending队头尚未到达时，直接将时钟设为其到达时间。
    // true表示时钟确实前移；不满足条件或时钟非法时false且状态不变；只跳时，不搬队列。
    bool jumpToNextArrival();

    // RR中把仍有剩余服务的合法运行者追加到ready队尾，改为READY并清空running。
    // 不推进时间、不收取到达者、不自动选下一个进程；失败时全部状态不变。
    bool requeueRunning();

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

    // 显示模拟时钟、运行者的CPU时间，以及pending/ready/blocked/finished的队列顺序。
    // 只读取状态，不移动或删除PCB，不调度，也不计算周转统计。
    void showSchedulingState() const;

    // 对已通过createScheduledProcess准备的一批任务连续调度一次，保留finished供后续统计。
    // FCFS/SJF忽略timeQuantum；RR要求timeQuantum>0，片末先收取到达者再回队。
    // 启动校验失败时状态不变；启动后的失败保留已有进度和资源，返回false，不自动重试。
    bool runScheduling(SchedulingAlgorithm algorithm,long long timeQuantum);

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
