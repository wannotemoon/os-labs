# 实验二设计记录

## 当前阶段

已确定主要类的职责，开始建立接口。
尚未实现分页分配、地址转换和缺页处理。
当前选择 FIFO，暂不实现 LRU；最终算法验收范围以教师要求为准。

## 类与数据归属

ProcessManager：管理进程生命周期和状态切换，调用 MemoryManager。
PCBQueue：继续管理就绪、阻塞进程，不用于页面置换。

PCB：保存进程身份、状态、逻辑大小、页表、FIFO 页号队列和访问统计。
PageTableEntry：保存一页的存在位、内存块号、置换块号和修改位。

MemoryManager：组织存储分配、回收、地址转换和缺页处理。
Bitmap：管理块的占用状态，不关心进程和页面。

MemoryManager 持有两个独立的 Bitmap 对象，分别管理内存和置换空间。
各进程的页表保存在自己的 PCB 中，由 MemoryManager 完成存储相关修改。
PCB 的创建和删除由 ProcessManager 负责。

## 请求分页策略：本项目约定

进程总页数为 N，驻留页数配额为 R。
默认 R = min(3, N)，允许创建时指定合法配额。
采用固定驻留数量、局部置换，不在运行过程中自动增加配额。

每个页面分配一个固定置换块，因此创建时申请 N 个置换块和 R 个内存块。
模型中，各置换块保存相应页面的有效初始副本，再装入前 R 页。
页面调入后保留自己的置换块；修改过的页面换出前先写回。
进程退出时释放当前占用的内存块和全部固定置换块。

资源分配先检查并准备，资源齐全后再提交；失败不能遗留部分分配。

## FIFO 维护规则

每个 PCB 保存自己的驻留页号队列，不保存物理块号。
初始页面按装入顺序进入队列。
访问命中时不调整队列。
需要置换时选择队头页，换出后移除队头，新调入页进入队尾。
页表始终按页号索引，不因置换顺序变化而重新排列。

## 实现顺序

先建立主要类型和方法接口，再逐个实现与验证。
先完成基础分页，再接入置换空间和 FIFO。
每个经过检查的阶段单独提交，并推送到 lab2-paging 分支。

## 阶段记录：扩展 PCB 数据结构

在 pcb.h 中加入 PageTableEntry，保存存在位、内存块号、
置换块号和修改位。

PCB 新增逻辑字节数、固定驻留配额、完整页表、
FIFO 驻留页号队列和访问统计。

页号由页表下标表示，总页数由页表长度表示。
FIFO 队列只保存驻留页号，不保存物理块号。

为了保持旧创建代码可用，原有字段及其顺序暂时不变，
新增字段追加在 next 后，并设置默认初始状态。
memStart、memSize 将在分页管理接入时删除。

当前尚未接入分页分配与访问，新增字段尚不参与旧程序运行。

本地验证结果：正常

## MemoryManager 目标接口：尚未接入运行代码
```
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

    // 根据逻辑字节数计算所需页数
    int calculatePageCount(int sizeBytes) const;

    // 处理合法页面的缺页，按 FIFO 完成局部置换
    void handlePageFault(PCB& pcb, int pageNo);

public:
    // 三个参数的单位均为字节
    MemoryManager(int memorySizeBytes = 64 * 1024,
                  int swapSizeBytes = 128 * 1024,
                  int pageSizeBytes = 1024);

    // residentPages 为 -1 时，采用默认驻留页数
    bool allocate(PCB& pcb, int sizeBytes, int residentPages = -1);

    // 释放该进程的内存块和置换块，但不删除 PCB
    bool release(PCB& pcb);

    // 完成一次地址访问，包含必要的缺页处理
    bool access(PCB& pcb,
                int logicalAddress,
                bool isWrite,
                int& physicalAddress);

    // 返回空闲空间的字节数
    int getFreeSize() const;
    int getFreeSwapSize() const;

    // 显示两张位示图及空闲空间情况
    void show() const;

    // 显示指定进程的页表及 FIFO 顺序
    void showPageTable(const PCB& pcb) const;

    // 显示指定进程的访问次数、缺页次数、置换次数和缺页率
    void showStatistics(const PCB& pcb) const;
};

#endif //OS_MEMORY_H
```