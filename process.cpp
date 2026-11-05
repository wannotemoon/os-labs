#include <iostream>
#include <new>
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
 *
 * [参数] sizeBytes和residentPages继续交给已有MemoryManager分配逻辑；
 * arrivalTime是入就绪队列的模拟时刻，burstTime是总服务时间，不是内存大小。
 * 返回true表示本次任务已准备成功；返回false不改变已有任务，也不消耗PID。
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
