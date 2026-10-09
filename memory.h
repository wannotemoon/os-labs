#ifndef OS_MEMORY_H
#define OS_MEMORY_H

#include "bitmap.h"
#include "pcb.h"

class MemoryManager {
private:
    int pageSizeBytes;
    int frameCount;
    int swapBlockCount;
    int offsetBits;

    Bitmap memoryBitmap;
    Bitmap swapBitmap;

    int calculatePageCount(int sizeBytes) const;
    void handlePageFault(PCB& pcb,int pageNo);

public:
    MemoryManager(int memorySizeBytes=64*1024,
                  int swapSizeBytes=128*1024,
                  int pageSizeBytes=1024);

    bool allocate(PCB& pcb,int sizeBytes,int residentPages=-1);
    bool release(PCB& pcb);

    bool access(PCB& pcb,int logicalAddress,bool isWrite,int& physicalAddress);

    int getFreeSize() const;
    int getFreeSwapSize() const;

    void show() const;
    void showPageTable(const PCB& pcb) const;
    void showStatistics(const PCB& pcb) const;
};

#endif
