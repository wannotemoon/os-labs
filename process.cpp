#include <iostream>
#include <new>
#include <limits>
#include <algorithm>
#include <iomanip>
using namespace std;

#include "process.h"

ProcessManager::ProcessManager(int memorySizeBytes,int swapSizeBytes,int pageSizeBytes)
    : memory(memorySizeBytes,swapSizeBytes,pageSizeBytes) {
    running = nullptr;
    nextPid = 1;
}

ProcessManager::~ProcessManager() {
    clearProcesses();
}

void ProcessManager::dispatch() {
    if (running == nullptr && !ready.empty()) {
        running = ready.pop();
        running->state = ProcessState::RUNNING;
    }
}

/* [lab4 第三轮：任务1，请你实现moveArrivedProcesses]
 * 方法定义：void ProcessManager::moveArrivedProcesses()
 * 无参数；读取成员currentTime和pending，返回void。空队列或尚无人到达时直接结束。
 */
void ProcessManager::moveArrivedProcesses() {
    while (!pending.empty() && pending.front()->arrivalTime<=currentTime) {
        PCB * pcb = pending.pop();
        pcb->state = ProcessState::READY;
        ready.push(pcb);
    }
}

/* [lab4 第三轮：任务2，请你实现dispatchScheduled]
 * 方法定义：bool ProcessManager::dispatchScheduled(SchedulingAlgorithm algorithm)
 * algorithm是CPU调度策略；返回true表示这次确实从ready选出一个运行者。
 */
bool ProcessManager::dispatchScheduled(SchedulingAlgorithm algorithm) {
    if (running!=nullptr || ready.empty()) {
        return false;
    }
    if (algorithm==SchedulingAlgorithm::FCFS || algorithm==SchedulingAlgorithm::RR) {
        PCB * pcb = ready.pop();
        running=pcb;
    }else if (algorithm==SchedulingAlgorithm::SJF) {
        PCB * pcb = ready.popShortest();
        running=pcb;
    }else {
        return false;
    }
    running->state = ProcessState::RUNNING;
    return true;
}

/* [lab4 第四轮：任务1，请你实现advanceRunning]
 * 方法定义：bool ProcessManager::advanceRunning(long long duration)
 * duration是实际执行时长，不是“执行到哪个时刻”；返回true才表示时间确实推进。
 */
bool ProcessManager::advanceRunning(long long duration) {
    if (running && running->state == ProcessState::RUNNING && duration>0) {
        if (running->burstTime>0 && running->burstTime>=running->executedTime
            && running->executedTime>=0 && running->finishTime==-1
            && currentTime>=0 && running->arrivalTime>=0 && running->arrivalTime<=currentTime) {
            long long left = running->burstTime - running->executedTime;
            if (duration>left) {
                return false;
            }
            if (duration>numeric_limits<long long>::max()-currentTime) {
                return false;
            }
            currentTime+=duration;
            running->executedTime+=duration;
            return true;
        }else {
            return false;
        }
    }else {
        return false;
    }
}

/* [lab4 第四轮：任务2，请你实现finishRunningProcess]
 * 方法定义：bool ProcessManager::finishRunningProcess()
 * 没有参数；成功表示一个确实执行完的运行进程已回收资源并进入完成队列。
 */
bool ProcessManager::finishRunningProcess() {
    if (running!=nullptr && running->state == ProcessState::RUNNING
        && running->burstTime>0 && running->burstTime==running->executedTime
        && running->finishTime==-1 && running->arrivalTime>=0
        && running->arrivalTime<=currentTime && currentTime>=0) {
        bool release = memory.release(*running);
        if (release) {
            running->finishTime=currentTime;
            running->state=ProcessState::FINISHED;
            finished.push(running);
            running=nullptr;
            return true;
        }else {
            return false;
        }
    }else {
        return false;
    }
}

/* [lab4 第五轮：任务1，请你实现jumpToNextArrival]
 * 方法定义：bool ProcessManager::jumpToNextArrival()
 * 无参数；使用成员running、ready、pending和currentTime。
 * true表示本次时钟确实向前跳转；false表示不应跳转，所有状态保持不变。
 */
bool ProcessManager::jumpToNextArrival() {
    if (running==nullptr && ready.empty() && !pending.empty() && currentTime>=0) {
        if (pending.front()->arrivalTime>currentTime) {
            currentTime=pending.front()->arrivalTime;
            return true;
        }else {
            return false;
        }
    }else {
        return false;
    }
}

/* [lab4 第五轮：任务2，请你实现requeueRunning]
 * 方法定义：bool ProcessManager::requeueRunning()
 * 无参数；true表示原运行者已回到就绪队尾并腾空CPU，false时全部状态不变。
 * [使用场景]
 * RR本片执行完但进程尚未完成时使用；片长和本次执行量由后续主循环决定。
 * 本方法只处理回队，不判断本片是否真的用完，也不重新选择运行者。
 */
bool ProcessManager::requeueRunning() {
    if (running!=nullptr && running->state == ProcessState::RUNNING) {
        if (running->burstTime>0 && running->burstTime>running->executedTime
            && running->executedTime>=0 && running->finishTime==-1) {
            if (currentTime>=0 && running->arrivalTime>=0
                && running->arrivalTime<=currentTime) {
                running->state=ProcessState::READY;
                ready.push(running);
                running=nullptr;
                return true;
            }else {
                return false;
            }
        }else {
            return false;
        }
    }else {
        return false;
    }
}

/* [lab4 第六轮：任务1，请你实现showSchedulingState]
 * 方法定义：void ProcessManager::showSchedulingState() const
 * 无参数、无返回值；const表示本方法只读取调度状态，用cout输出一次快照。
 * 先写这个方法，再写下面的主循环，便于观察每次调用已有方法后的结果。
 */
void ProcessManager::showSchedulingState() const {
    cout<<"currentTime:"<<currentTime<<endl;
    cout<<"running:";
    if (running==nullptr) {
        cout<<"null"<<endl;
    }else {
        cout<<"pid:"<<running->pid<<" name"<<running->name
        <<" arrivalTime:"<<running->arrivalTime
        <<" burstTime:"<<running->burstTime
        <<" executedTime:"<<running->executedTime
        <<" left time"<<running->burstTime-running->executedTime<<endl;
    }
    cout<<"pending:";
    pending.show();
    cout<<"ready:";
    ready.show();
    cout<<"blocked:";
    blocked.show();
    cout<<"finished:";
    finished.show();
}

/* [lab4 第六轮：任务2，请你实现runScheduling]
 * 方法定义：
 * bool ProcessManager::runScheduling(SchedulingAlgorithm algorithm,long long timeQuantum)
 * 两个参数都要传：algorithm选择算法；timeQuantum是RR时间片，FCFS/SJF忽略它。
 */
bool ProcessManager::runScheduling(SchedulingAlgorithm algorithm, long long timeQuantum) {
    if (schedulingStarted==false && currentTime==0
        && running==nullptr && ready.empty() && !pending.empty()
        && blocked.empty() && finished.empty()) {
    }else {
        return false;
    }
    if (algorithm==SchedulingAlgorithm::FCFS || algorithm==SchedulingAlgorithm::SJF
        || (algorithm==SchedulingAlgorithm::RR && timeQuantum>0)) {
        schedulingStarted=true;
        showSchedulingState();
        moveArrivedProcesses();
        while (!pending.empty() || !ready.empty() || running) {
            moveArrivedProcesses();
            showSchedulingState();
            if (running==nullptr && ready.empty()) {
                if (!jumpToNextArrival()) {
                    return false;
                }
                moveArrivedProcesses();
                showSchedulingState();
            }
            if (running==nullptr && !ready.empty()) {
                bool dispatch_scheduled = dispatchScheduled(algorithm);
                if (!dispatch_scheduled) {
                    return false;
                }else {
                    showSchedulingState();
                }
            }
            if (algorithm==SchedulingAlgorithm::FCFS || algorithm==SchedulingAlgorithm::SJF) {
                if (!advanceRunning(running->burstTime-running->executedTime)) {
                    return false;
                }
            }else {
                if (!advanceRunning(min(timeQuantum,running->burstTime-running->executedTime))) {
                    return false;
                }
            }
            moveArrivedProcesses();
            showSchedulingState();
            if (running->burstTime==running->executedTime) {
                if (!finishRunningProcess()) {
                    return false;
                }
                showSchedulingState();
            }else if (algorithm==SchedulingAlgorithm::RR) {
                if (!requeueRunning()) {
                    return false;
                }
                showSchedulingState();
            }
        }
        return true;
    }else {
        return false;
    }
}

void ProcessManager::showSchedulingStatistics() const {
    cout<<"-------- 调度统计（按完成顺序） --------"<<endl;
    if (finished.empty()) {
        // 没有完成者就没有可用的分母；不把空批次或全部失败的结果显示为平均0。
        cout<<"暂无已完成进程，无法计算平均值"<<endl;
        return;
    }
    if (running!=nullptr || !pending.empty() || !ready.empty() || !blocked.empty()) {
        cout<<"当前还有未完成进程，以下只统计已完成进程"<<endl;
    }

    // 周转量由已有时间推导，不新增PCB字段；finish和arrival均非负且finish>=arrival。
    // 累加使用long double，避免多个合法的long long周转量相加后超出整数上界。
    long double totalTurnaround=0;
    long double totalWeighted=0;
    const auto oldFlags=cout.flags();
    const auto oldPrecision=cout.precision();
    cout<<fixed<<setprecision(3);
    cout<<"PID\t名称\t到达时间\t服务时间\t完成时间\t周转时间\t带权周转时间"<<endl;
    for (const PCB* pcb=finished.front();pcb!=nullptr;pcb=pcb->next) {
        const long long turnaround=pcb->finishTime-pcb->arrivalTime;
        // 先转浮点再除，4/3才能得到1.333...，而不是整数除法得到1。
        const long double weighted=static_cast<long double>(turnaround)/pcb->burstTime;
        totalTurnaround+=turnaround;
        totalWeighted+=weighted;
        cout<<pcb->pid<<'\t'<<pcb->name<<'\t'<<pcb->arrivalTime<<'\t'<<pcb->burstTime
            <<'\t'<<pcb->finishTime<<'\t'<<turnaround<<'\t'<<weighted<<endl;
    }
    const int count=finished.size();
    cout<<"已完成进程数："<<count<<endl;
    cout<<"平均周转时间："<<totalTurnaround/count<<endl;
    // 平均带权周转是“各进程带权周转之和/个数”，不是总周转量除以总服务量。
    cout<<"平均带权周转时间："<<totalWeighted/count<<endl;
    // cout是全局输出流，恢复格式，防止后面的分页缺页率等输出也被固定成三位小数。
    cout.flags(oldFlags);
    cout.precision(oldPrecision);
}

/* [lab4 第二轮：任务1，扩展已有clearProcesses]
 * 先补这个方法，再写下面的createScheduledProcess，避免退出时遗漏新队列里的PCB。
 * 原running、ready、blocked的清理保留；在后面补pending和finished。
 */
void ProcessManager::clearProcesses() {
    if (running) {
        memory.release(*running);
        delete running;
        running = nullptr;
    }
    while (ready.front()!=nullptr) {
        memory.release(*ready.front());
        delete ready.pop();
    }
    while (blocked.front()!=nullptr) {
        memory.release(*blocked.front());
        delete blocked.pop();
    }
    while (pending.front()!=nullptr) {
        memory.release(*pending.front());
        delete pending.pop();
    }
    while (finished.front()!=nullptr) {
        delete finished.pop();
    }
    currentTime=0;
    schedulingStarted=false;
}

bool ProcessManager::createProcess(const string &name,int sizeBytes,int residentPages) {
    if (sizeBytes<=0) {
        return false;
    }

    PCB* pcb=nullptr;
    try {
        pcb=new PCB{};
        pcb->pid=nextPid;
        pcb->name=name;
        pcb->state=ProcessState::READY;
        pcb->memStart=-1;
        pcb->memSize=0;
        pcb->next=nullptr;

        if (memory.allocate(*pcb,sizeBytes,residentPages)) {
            nextPid++;
            ready.push(pcb);
            dispatch();
            return true;
        }else {
            delete pcb;
            return false;
        }
    }catch (const bad_alloc&) {
        delete pcb;
        return false;
    }
}

/* [lab4 第二轮：任务2，请你实现createScheduledProcess]
 * 方法定义：
 * bool ProcessManager::createScheduledProcess(const string& name,int sizeBytes,
 *         long long arrivalTime,long long burstTime,int residentPages)
 * 默认参数只写在头文件声明里，不要在这里重复写=-1。
 */
bool ProcessManager::createScheduledProcess(const string &name, int sizeBytes,
    long long arrivalTime, long long burstTime, int residentPages) {
    if (sizeBytes>0 && arrivalTime>=0 && burstTime>0 && !schedulingStarted &&
        !running && ready.empty() && blocked.empty() && finished.empty()) {
        PCB *pcb = nullptr;
        try {
            pcb = new PCB{};
            pcb->pid=nextPid;
            pcb->name=name;
            pcb->sizeBytes=sizeBytes;
            pcb->state=ProcessState::NOT_ARRIVED;
            pcb->memStart=-1;
            pcb->memSize=0;
            pcb->next=nullptr;
            pcb->arrivalTime=arrivalTime;
            pcb->burstTime=burstTime;
            pcb->executedTime=0;
            pcb->finishTime=-1;
        }catch (const bad_alloc&) {
            delete pcb;
            return false;
        }
        if (memory.allocate(*pcb,sizeBytes,residentPages)) {
            pending.pushByArrival(pcb);
            nextPid++;
            return true;
        }else {
            delete pcb;
            return false;
        }
    }else {
        return false;
    }
}

bool ProcessManager::timeOut() {
    if (running!=nullptr) {
        ready.push(running);
        running->state = ProcessState::READY;
        running = ready.pop();
        running->state = ProcessState::RUNNING;
        return true;
    }else {
        return false;
    }
}

bool ProcessManager::blockProcess() {
    if (running!=nullptr) {
        blocked.push(running);
        running->state = ProcessState::BLOCKED;
        running = nullptr;
        dispatch();
        return true;
    }else {
        return false;
    }
}

bool ProcessManager::wakeProcess() {
    if (!blocked.empty()) {
        PCB* p = blocked.pop();
        p->state = ProcessState::READY;
        ready.push(p);

        dispatch();
        return true;
    }else {
        return false;
    }
}

bool ProcessManager::terminateProcess() {
    if (running!=nullptr) {
        if (memory.release(*running)) {
            delete running;
            running = nullptr;
            dispatch();
            return true;
        }else {
            return false;
        }
    }else {
        return false;
    }
}

const PCB* ProcessManager::findProcess(int pid) const {
    if (pid<=0) {
        return nullptr;
    }
    if (running!=nullptr && running->pid==pid) {
        return running;
    }

    const PCB* p=ready.front();
    while (p!=nullptr) {
        if (p->pid==pid) {
            return p;
        }
        p=p->next;
    }

    p=blocked.front();
    while (p!=nullptr) {
        if (p->pid==pid) {
            return p;
        }
        p=p->next;
    }
    return nullptr;
}

bool ProcessManager::accessCurrent(int logicalAddress,bool isWrite,int& physicalAddress) {
    physicalAddress=-1;
    if (running==nullptr) {
        return false;
    }
    return memory.access(*running,logicalAddress,isWrite,physicalAddress);
}

void ProcessManager::showProcess() const {
    cout<<"running:  ";
    if (running!=nullptr) {
        cout<<running->pid<<":"<<running->name;
    }else {
        cout<<"NULL";
    }
    cout<<endl;

    cout<<"ready:    ";
    ready.show();
    cout<<"blocked:  ";
    blocked.show();
}

void ProcessManager::showMemory() const {
    memory.show();
}

bool ProcessManager::showPageTable(int pid) const {
    const PCB* p=findProcess(pid);
    if (p==nullptr) {
        return false;
    }
    memory.showPageTable(*p);
    return true;
}

bool ProcessManager::showStatistics(int pid) const {
    const PCB* p=findProcess(pid);
    if (p==nullptr) {
        return false;
    }
    memory.showStatistics(*p);
    return true;
}
