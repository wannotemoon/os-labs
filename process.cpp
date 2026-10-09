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
