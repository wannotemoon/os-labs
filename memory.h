//
// Created by 28794 on 2026/9/3.
//

#ifndef OS_MEMORY_H
#define OS_MEMORY_H

struct MemoryBlock {
    int start;
    int length;

    int pid; //-1:none

    MemoryBlock* prev;
    MemoryBlock* next;
};

class MemoryManager {
private:
    int totalSize;
    MemoryBlock* head;

    static const int MIN = 2;
    void merge(MemoryBlock* block);
public:
    MemoryManager(int size);
    ~MemoryManager();

    bool allocate(int pid,int requestSize,int& start,int& actualSize);

    bool release(int pid);

    int getFreeSize() const;

    void show() const;
};

#endif //OS_MEMORY_H
