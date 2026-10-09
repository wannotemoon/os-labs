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

## ProcessManager 目标接口：尚未接入运行代码

```
#ifndef OS_PROCESS_H
#define OS_PROCESS_H

#include <string>
using namespace std;

#include "pcb.h"
#include "pcb_queue.h"
#include "memory.h"

class ProcessManager {
private:
    PCB* running;
    PCBQueue ready;
    PCBQueue blocked;

    MemoryManager memory;

    int nextPid;

    void dispatch();
    void clearProcesses();

    // 根据进程编号查找，只查看，不移除进程
    const PCB* findProcess(int pid) const;

public:
    // 三个容量参数统一使用字节
    ProcessManager(int memorySizeBytes = 64 * 1024,
                   int swapSizeBytes = 128 * 1024,
                   int pageSizeBytes = 1024);

    ~ProcessManager();

    // 创建进程，并为其分配分页存储资源
    bool createProcess(const string& name,
                       int sizeBytes,
                       int residentPages = -1);

    // 原来的进程控制操作
    bool timeOut();
    bool blockProcess();
    bool wakeProcess();
    bool terminateProcess();

    // 访问当前运行进程的逻辑地址
    bool accessCurrent(int logicalAddress,
                       bool isWrite,
                       int& physicalAddress);

    // 原来的状态查看入口
    void showProcess() const;
    void showMemory() const;

    // 根据进程编号查看分页信息
    bool showPageTable(int pid) const;
    bool showStatistics(int pid) const;
};

#endif //OS_PROCESS_H
```

## ProcessManager 接入约定

数据成员保持不变，不重复保存页表、FIFO 队列和访问统计。

createProcess 先建立临时 PCB，再调用 MemoryManager 分配资源。
分配成功后加入就绪队列并推进 nextPid；失败清理临时 PCB。
默认驻留页数的计算由 MemoryManager 负责。

accessCurrent 只选择当前运行进程并转交访问请求。
缺页在本次存储访问内部同步处理，不触发进程状态切换。

findProcess 根据 PID 查找运行、就绪和阻塞中的进程。
查找与显示不得改变队列、页表、FIFO 顺序或访问统计。

终止与退出清理先释放存储资源，再删除 PCB。
时间片到、阻塞、唤醒不重置分页状态。

主菜单在接入分页时统一使用字节单位，并新增地址访问、
按 PID 查看页表和访问统计的入口。

当前只完成接口设计，尚未修改 process.h、process.cpp、main.cpp。

# 实验二 FIFO 接入与检查记录

## 本轮文件

替换 memory.h、memory.cpp、process.h、process.cpp、main.cpp。
新增本记录。沿用当前分支的 pcb.h、pcb_queue.h/cpp、bitmap.h/cpp 和 CMakeLists.txt。
更新包不包含 .git、IDE 配置或构建产物，不是独立的新项目。

## 当前状态

MemoryManager 和 ProcessManager 的分页接口已经接入运行代码。
docs/lab2-design.md 中标注“尚未接入”的段落属于之前的设计阶段；本记录描述这次接入后的状态。
保留原来的时间片、阻塞、唤醒和进程队列逻辑。
PCB 中 memStart、memSize 暂时保留，创建时设置为 -1、0，不参与分页分配和地址转换。

## 本版本约定与范围

默认物理内存 65536 字节，置换空间 131072 字节，页面 1024 字节。
创建输入的进程大小统一使用字节，不再使用 KB。
每页固定分配一个置换块。驻留页也保留置换块。
初始配额为 -1 时采用 min(3, 总页数)；也可手动指定 1 到总页数。
采用固定驻留配额、局部 FIFO 置换；命中不改变 FIFO 顺序。
资源数量不足时，不改变两张位示图或待分配 PCB。
统计每次成功完成的合法访问；非法地址不计数，缺页后继续完成的同一次请求不重复计数。
退出或终止时释放当前驻留物理块和全部固定置换块。

本版只有 FIFO，没有 LRU，不能用“FIFO 与 LRU 均已实现”描述它。
换入、写回仅模拟页表、修改位、页面位置和队列变化。
模型假定各固定置换块代表有效后备副本，代码没有逐字节保存页面内容，没有真实磁盘文件读写。
因此日志使用 [模拟]，测试不包含具体数据内容的保存和恢复。
初始位示图全部空闲，没有启用随机初始占用。
将驻留配额设为总页数可演示全部驻留时的地址转换，但仍会分配固定置换块。

## 生成端验证

环境：Linux，GCC 14.2.0，C++20。
完整菜单程序使用 -Wall -Wextra -Wpedantic -Werror 编译通过。
核心测试使用 AddressSanitizer、UndefinedBehaviorSanitizer 并启用泄漏检测；本次执行未报告错误。
测试包含：

- 位示图跨字节操作、重复设置、越界、查找失败清空输出。
- 构造配置检查、不同页面大小、默认和全驻留配额、最后有效地址。
- FIFO 命中不换序、修改位、固定置换块、地址转换、缺页及置换统计。
- 内存不足、置换块不足、重复分配、失败无部分占用、回收复用。
- 3 个进程的 10000 次固定种子访问，逐次与独立 FIFO 参考模型核对。
- 进程状态切换、按 PID 查找、失败不消耗 PID，以及 100 次带未终止进程的退出清理。
- 完整菜单场景、11 组非法输入和 7 组输入结束场景。

这些是生成端测试结果，不代替用户本地 CLion 检查。
未在 Windows/CLion 中执行；未使用本地 CMake 3.31.6 配置用户要求 CMake 4.2 的项目。
未改动用户 CMake 的最低版本要求。

## 本地手动检查

重新构建并启动新程序，不先创建其他进程。

1. 选择 1，进程名 P1，大小 5000，驻留页数 -1。
2. 选择 10，PID 输入 1。应有 5 行页表，前三页驻留，FIFO 为 0 -> 1 -> 2。
3. 每次选择 9，按下表顺序访问：

| 逻辑地址 | 读写 | 预期物理地址 | 访问后的 FIFO |
|---|---|---|---|
| 0 | Y | 0 | 0 -> 1 -> 2 |
| 3072 | N | 0 | 1 -> 2 -> 3 |
| 1024 | N | 1024 | 1 -> 2 -> 3 |
| 4096 | Y | 1024 | 2 -> 3 -> 4 |

第二次访问应输出把修改过的 0 号页从物理块 0 模拟写回置换块 0，随后调入 3 号页。
第四次访问应换出 1 号页，尽管上一条访问刚读过它；FIFO 不按最近访问顺序置换。

4. 选择 11，PID 输入 1：访问 4 次、缺页 2 次、置换 2 次、缺页率 50%。
5. 再访问逻辑地址 5000，读操作：应拒绝访问，上述计数保持不变。
6. 选择 5 终止，再选择 7：空闲内存 65536 字节，空闲置换空间 131072 字节。

## 用户本地结果

待本地构建和演示后填写，不将生成端测试写成用户已完成的验收。

## 自动测试输出

```text
PASS: bitmap, constructor validation, exact address bounds, default/full residency
PASS: FIFO trace, dirty writeback state, hit order, display non-mutation, statistics
PASS: insufficient frames/swap, allocation rollback, repeat release, block reuse
PASS: 10000 seeded accesses against independent multi-process FIFO model
PASS: process state transitions, PID lookup, PID reuse policy, 100 destructor runs
PASS: actual menu FIFO scenario; 11 invalid-input cases; 7 EOF cases
```
