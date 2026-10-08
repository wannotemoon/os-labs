//
// Created by 28794 on 2026/9/3.
//

#include "memory.h"

#include <iostream>
using namespace std;

MemoryManager::MemoryManager(int size) {
    totalSize=size;
    head=new MemoryBlock{0,size,-1,nullptr,nullptr};
}

MemoryManager::~MemoryManager() {
    while (head!=nullptr) {
        MemoryBlock* p = head;
        head=head->next;
        delete p;
    }}

void MemoryManager::merge(MemoryBlock *block) {
    if (block->prev!=nullptr && block->prev->pid==-1) {
        MemoryBlock* p = block->prev;
        p->length+=block->length;
        if (block->next!=nullptr) {
            block->next->prev=p;
        }
        p->next=block->next;
        delete block;
        block=p;
    }

    if (block->next!=nullptr && block->next->pid==-1) {
        MemoryBlock* after = block->next;
        block->length+=after->length;
        if (after->next!=nullptr) {
            after->next->prev=block;
        }
        block->next=after->next;
        delete after;
    }

}

bool MemoryManager::allocate(int pid,int requestSize,int& start,int& actualSize) {

    if (pid<=0 || requestSize<=0) {
        return false;
    }

    MemoryBlock* m=head;
    while (m!=nullptr) {
        if (m->pid!=-1) {
            m=m->next;
            continue;
        }
        if (m->length >= requestSize) {
            int a = m->length - requestSize;
            if (a==0 || (a>0 && a<MIN)) {
                m->pid=pid;

                start=m->start;
                actualSize=m->length;
                return true;
            }else if (a>=MIN) {
                m->pid=pid;
                m->length-=a;
                MemoryBlock* b = new MemoryBlock{m->start+m->length,a,-1,m,m->next};
                if (m->next!=nullptr) {
                    m->next->prev=b;
                }
                m->next=b;

                start=m->start;
                actualSize=m->length;
                return true;
            }
        }
        m=m->next;

    }
    return false;
}

bool MemoryManager::release(int pid) {
    MemoryBlock* m = head;
    while (m!=nullptr) {
        if (m->pid==pid) {
            m->pid=-1;
            merge(m);
            return true;
        }
        m=m->next;
    }
    return false;
}

int MemoryManager::getFreeSize() const {
    MemoryBlock* m = head;
    int sum=0;
    while (m!=nullptr) {
        if (m->pid==-1) {
            sum+=m->length;
        }
        m=m->next;
    }
    return sum;
}

void MemoryManager::show() const {
    MemoryBlock* m = head;
    while (m!=nullptr) {
        cout<<m->start<<"|"<<m->length<<"|"<<m->pid;
        if (m->next!=nullptr) {
            cout<<" -> ";
        }
        m=m->next;
    }
    cout<<endl;
}