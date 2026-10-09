#include "memory.h"

#include <iostream>
#include <new>
#include <stdexcept>
using namespace std;

MemoryManager::MemoryManager(int memorySizeBytes,int swapSizeBytes,int pageSizeBytes)
    : memoryBitmap(1),swapBitmap(1) {
    if (pageSizeBytes<=0 || (pageSizeBytes & (pageSizeBytes-1))!=0) {
        throw invalid_argument("Page size must be a positive power of two");
    }
    if (memorySizeBytes<=0 || swapSizeBytes<=0) {
        throw invalid_argument("Memory size and swap size must be positive");
    }
    if (memorySizeBytes%pageSizeBytes!=0 || swapSizeBytes%pageSizeBytes!=0) {
        throw invalid_argument("Memory size and swap size must be multiples of page size");
    }

    this->pageSizeBytes=pageSizeBytes;
    frameCount=memorySizeBytes/pageSizeBytes;
    swapBlockCount=swapSizeBytes/pageSizeBytes;

    memoryBitmap=Bitmap(frameCount);
    swapBitmap=Bitmap(swapBlockCount);

    offsetBits=0;
    int n=pageSizeBytes;
    while (n>1) {
        n=n/2;
        offsetBits++;
    }
}

int MemoryManager::calculatePageCount(int sizeBytes) const {
    if (sizeBytes<=0) {
        return 0;
    }
    return (sizeBytes-1)/pageSizeBytes+1;
}

bool MemoryManager::allocate(PCB& pcb,int sizeBytes,int residentPages) {
    if (pcb.pid<=0 || sizeBytes<=0) {
        return false;
    }
    if (!pcb.pageTable.empty() || !pcb.fifoPages.empty()) {
        return false;
    }

    int count=calculatePageCount(sizeBytes);
    if (residentPages==-1) {
        if (count<3) {
            residentPages=count;
        }else {
            residentPages=3;
        }
    }
    if (residentPages<=0 || residentPages>count) {
        return false;
    }

    try {
        vector<int> frames;
        vector<int> swaps;
        if (!memoryBitmap.findFreeBlocks(residentPages,frames)) {
            return false;
        }
        if (!swapBitmap.findFreeBlocks(count,swaps)) {
            return false;
        }

        vector<PageTableEntry> table(count);
        queue<int> fifo;
        for (int i=0;i<count;i++) {
            table[i].swapNo=swaps[i];
            if (i<residentPages) {
                table[i].present=true;
                table[i].frameNo=frames[i];
                fifo.push(i);
            }
        }

        for (int i=0;i<residentPages;i++) {
            memoryBitmap.setUsed(frames[i],true);
        }
        for (int i=0;i<count;i++) {
            swapBitmap.setUsed(swaps[i],true);
        }

        pcb.pageTable.swap(table);
        pcb.fifoPages.swap(fifo);
        pcb.sizeBytes=sizeBytes;
        pcb.residentLimit=residentPages;
        pcb.accessCount=0;
        pcb.pageFaultCount=0;
        pcb.replacementCount=0;
        return true;
    }catch (const bad_alloc&) {
        return false;
    }
}

bool MemoryManager::release(PCB& pcb) {
    if (pcb.pageTable.empty()) {
        return false;
    }

    for (int i=0;i<(int)pcb.pageTable.size();i++) {
        const PageTableEntry& p=pcb.pageTable[i];
        if (p.swapNo<0 || p.swapNo>=swapBlockCount) {
            return false;
        }
        if (!swapBitmap.isUsed(p.swapNo)) {
            return false;
        }
        if (p.present) {
            if (p.frameNo<0 || p.frameNo>=frameCount) {
                return false;
            }
            if (!memoryBitmap.isUsed(p.frameNo)) {
                return false;
            }
        }
    }

    for (int i=0;i<(int)pcb.pageTable.size();i++) {
        PageTableEntry& p=pcb.pageTable[i];
        if (p.present) {
            memoryBitmap.setUsed(p.frameNo,false);
        }
        swapBitmap.setUsed(p.swapNo,false);
    }

    pcb.pageTable.clear();
    while (!pcb.fifoPages.empty()) {
        pcb.fifoPages.pop();
    }
    pcb.sizeBytes=0;
    pcb.residentLimit=0;
    return true;
}

void MemoryManager::handlePageFault(PCB& pcb,int pageNo) {
    if (pcb.fifoPages.empty()) {
        throw logic_error("FIFO queue is empty");
    }

    int oldNo=pcb.fifoPages.front();
    PageTableEntry& oldPage=pcb.pageTable.at(oldNo);
    PageTableEntry& newPage=pcb.pageTable.at(pageNo);
    if (!oldPage.present || newPage.present) {
        throw logic_error("Page table and FIFO queue do not match");
    }
    int frame=oldPage.frameNo;

    pcb.fifoPages.push(pageNo);

    cout<<pageNo<<"号页不在内存，置换块号为 "<<newPage.swapNo<<endl;
    cout<<"FIFO 选中 "<<oldNo<<"号页，内存块号为 "<<frame
        <<"，修改位为 "<<oldPage.modified
        <<"，置换块号为 "<<oldPage.swapNo<<endl;

    if (oldPage.modified) {
        cout<<"[模拟] 将内存块 "<<frame
            <<" 写回置换块 "<<oldPage.swapNo<<endl;
    }else {
        cout<<"原页面未修改，不需要写回"<<endl;
    }
    cout<<"[模拟] 将置换块 "<<newPage.swapNo
        <<" 调入内存块 "<<frame<<endl;

    oldPage.present=false;
    oldPage.frameNo=-1;
    oldPage.modified=false;

    newPage.present=true;
    newPage.frameNo=frame;
    newPage.modified=false;

    pcb.fifoPages.pop();
    pcb.replacementCount++;
}

bool MemoryManager::access(PCB& pcb,int logicalAddress,bool isWrite,int& physicalAddress) {
    physicalAddress=-1;
    if (pcb.pageTable.empty() || logicalAddress<0 || logicalAddress>=pcb.sizeBytes) {
        return false;
    }

    int pageNo=logicalAddress>>offsetBits;
    int offset=logicalAddress & (pageSizeBytes-1);
    if (pageNo>=(int)pcb.pageTable.size()) {
        return false;
    }

    cout<<"页号："<<pageNo<<"，页内偏移："<<offset<<endl;
    if (!pcb.pageTable[pageNo].present) {
        handlePageFault(pcb,pageNo);
        pcb.pageFaultCount++;
    }else {
        cout<<"页面已在内存，内存块号为 "<<pcb.pageTable[pageNo].frameNo<<endl;
    }

    if (isWrite) {
        pcb.pageTable[pageNo].modified=true;
    }
    physicalAddress=pcb.pageTable[pageNo].frameNo*pageSizeBytes+offset;
    pcb.accessCount++;
    return true;
}

int MemoryManager::getFreeSize() const {
    return memoryBitmap.getFreeCount()*pageSizeBytes;
}

int MemoryManager::getFreeSwapSize() const {
    return swapBitmap.getFreeCount()*pageSizeBytes;
}

void MemoryManager::show() const {
    cout<<"页面大小："<<pageSizeBytes<<" 字节"<<endl;
    cout<<"memory: 总容量 "<<frameCount*pageSizeBytes
        <<"，空闲 "<<getFreeSize()<<" 字节"<<endl;
    memoryBitmap.show();

    cout<<"swap: 总容量 "<<swapBlockCount*pageSizeBytes
        <<"，空闲 "<<getFreeSwapSize()<<" 字节"<<endl;
    swapBitmap.show();
}

void MemoryManager::showPageTable(const PCB& pcb) const {
    cout<<"进程 "<<pcb.pid<<":"<<pcb.name<<endl;
    cout<<"逻辑大小："<<pcb.sizeBytes<<" 字节，总页数："<<pcb.pageTable.size()
        <<"，驻留配额："<<pcb.residentLimit<<endl;
    cout<<"页号\t存在位\t内存块\t置换块\t修改位"<<endl;

    for (int i=0;i<(int)pcb.pageTable.size();i++) {
        const PageTableEntry& p=pcb.pageTable[i];
        cout<<i<<"\t"<<p.present<<"\t"<<p.frameNo<<"\t"
            <<p.swapNo<<"\t"<<p.modified<<endl;
    }

    cout<<"FIFO: ";
    queue<int> q=pcb.fifoPages;
    while (!q.empty()) {
        cout<<q.front();
        q.pop();
        if (!q.empty()) {
            cout<<" -> ";
        }
    }
    cout<<endl;
}

void MemoryManager::showStatistics(const PCB& pcb) const {
    double rate=0;
    if (pcb.accessCount!=0) {
        rate=(double)pcb.pageFaultCount/pcb.accessCount*100;
    }
    cout<<"进程 "<<pcb.pid<<":"<<pcb.name<<endl;
    cout<<"访问次数："<<pcb.accessCount<<endl;
    cout<<"缺页次数："<<pcb.pageFaultCount<<endl;
    cout<<"置换次数："<<pcb.replacementCount<<endl;
    cout<<"缺页率："<<rate<<"%"<<endl;
}
