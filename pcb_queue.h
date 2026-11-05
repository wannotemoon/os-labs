//
// Created by 28794 on 2026/9/3.
//

#ifndef OS_PCB_QUEUE_H
#define OS_PCB_QUEUE_H

#include "pcb.h"

class PCBQueue {
private:
    PCB* head;
    PCB* tail;
    int count;

public:
    PCBQueue();

    void push(PCB* p);
    PCB* pop();

    // lab4：队列原先须按arrivalTime非递减排列；插入后仍保持有序。
    // 相同到达时间按插入先后排列；p为空时不改变队列，不检查时间业务合法性。
    // 调用者须保证p不在任何队列中；与push一样，一个PCB只能属于一个队列。
    void pushByArrival(PCB* p);

    // lab4：取出burstTime最小的PCB，相同时取原队列中靠前的那个。
    // 空队列返回nullptr；只摘链，不删除PCB，不改变进程状态或任何时间字段。
    // 供非抢占式SJF使用，不是按剩余时间选择的SRTF。
    PCB* popShortest();

    PCB* front() const;

    bool empty() const;
    int size() const;
    void show() const;
};

#endif //OS_PCB_QUEUE_H
