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

