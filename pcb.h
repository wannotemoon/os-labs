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
    BLOCKED,
    // lab4：尚未到达不能加入就绪队列；完成后保留PCB供时间统计。
    // 追加在末尾，不改变原READY/RUNNING/BLOCKED的枚举值。
    NOT_ARRIVED,
    FINISHED
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

    // lab4：以下都是模拟时间，不是内存字节数，也不需要真实等待。
    long long arrivalTime=0;
    long long burstTime=0;     // 总共需要的CPU服务时间，调度任务须>0
    long long executedTime=0;  // 累计已执行时间，剩余时间=burstTime-executedTime
    long long finishTime=-1;

};

#endif //OS_PCB_H
