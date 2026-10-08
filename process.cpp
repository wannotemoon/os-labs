//
// Created by 28794 on 2026/9/3.
//

#include <iostream>
using namespace std;

#include "process.h"

ProcessManager::ProcessManager(int memorySize) : memory(memorySize) {
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
        memory.release(running->pid);
        delete running;
        running = nullptr;
    }
    while (ready.front()!=nullptr) {
        memory.release(ready.front()->pid);
        delete ready.pop();
    }
    while (blocked.front()!=nullptr) {
        memory.release(blocked.front()->pid);
        delete blocked.pop();
    }

}

bool ProcessManager::createProcess(const string &name,int size) {
    if (size<=0) {
        return false;
    }
    int a=0,b = 0;
    if (memory.allocate(nextPid,size,a,b)) {
        PCB* pcb = new PCB{nextPid++,name,ProcessState::READY,a,b,nullptr};
        ready.push(pcb);
        dispatch();
        return true;
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
        if (memory.release(running->pid)) {
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
    cout<<"memory:   ";
    memory.show();

}
