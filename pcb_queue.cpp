//
// Created by 28794 on 2026/9/3.
//

#include "pcb_queue.h"

#include <iostream>
using namespace std;

PCBQueue::PCBQueue() {
    head = nullptr;
    tail = nullptr;
    count=0;
}

void PCBQueue::push(PCB *pcb) {
    if (pcb == nullptr) {
        return;
    }
    pcb->next = nullptr;

    if (head == nullptr) {
        head = pcb;
        tail = pcb;
    }else {
        tail->next = pcb;
        tail = pcb;
    }
    count++;
}

PCB* PCBQueue::pop() {
    if (head == nullptr) {
        return nullptr;
    }else {
        PCB* p = head;
        head = head->next;

        p->next = nullptr; //断后
        count--;

        //易错点
        if (head == nullptr) {
            tail = nullptr;
        }
        return p;
    }

}

/* [lab4 第一轮：请你实现任务1]
 * 方法定义：void PCBQueue::pushByArrival(PCB* p)
 *
 * [职责] 向“待到达队列”按arrivalTime升序插入，不是让进程立即运行。
 * 调用者保证原队列已经有序，p是未入队的有效节点；时间是否合法由上层检查。
 * p==nullptr时直接结束，head、tail、count均不变。
 */
void PCBQueue::pushByArrival(PCB *p) {
    if (p == nullptr) {
        return;
    }
    if (head == nullptr) {
        head = p;
        tail = p;
        p->next = nullptr;
    }else {
        if (p->arrivalTime<head->arrivalTime) {
            p->next = head;
            head = p;
        }else {
            PCB* tmp = head;
            PCB* pre = head;
            while (tmp!=nullptr && tmp->arrivalTime<=p->arrivalTime) {
                pre = tmp;
                tmp=tmp->next;
            }
            p->next = tmp;
            pre->next = p;
            if (tmp==nullptr) {
                tail = p;
            }
        }
    }
    count++;
}

/* [lab4 第一轮：请你实现任务2]
 * 方法定义：PCB* PCBQueue::popShortest()
 *
 * [职责] 从已有的就绪队列摘出服务时间最短的节点，其他节点的相对顺序不变。
 * 不要直接调用pop()，因为最短节点可能位于中间或末尾。
 */
PCB *PCBQueue::popShortest() {
    if (head == nullptr) {
        return nullptr;
    }
    PCB* tmp = head->next;
    PCB* pre = head;
    PCB* result = head;
    PCB* result_pre = nullptr;
    while (tmp != nullptr) {
        if (tmp->burstTime<result->burstTime) {
            result_pre = pre;
            result=tmp;
        }
        pre = tmp;
        tmp = tmp->next;
    }
    if (result==head) {
        head = result->next;
        if (head==nullptr) {
            tail = nullptr;
        }
    }else if (result==tail) {
        result_pre->next = nullptr;
        tail = result_pre;
    }else {
        result_pre->next = result->next;
    }
    result->next = nullptr;
    count--;
    return result;
}

PCB* PCBQueue::front() const {
    return head;
}

bool PCBQueue::empty() const {
    return count==0;
    //return head ==nullptr;
}

int PCBQueue::size() const {
    return count;
}

void PCBQueue::show() const {
    PCB* p = head;
    while (p != nullptr) {
        cout<<p->pid<<":"<<p->name;
        if (p->next != nullptr) {
            cout<<" -> ";
        }
        p = p->next;
    }
    cout<<endl;
}

