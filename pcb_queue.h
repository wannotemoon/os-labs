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

    PCB* front() const;

    bool empty() const;
    int size() const;
    void show() const;
};

#endif //OS_PCB_QUEUE_H
