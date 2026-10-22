#ifndef OS_DISK_H
#define OS_DISK_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
using namespace std;

class DiskManager {
public:
    static constexpr int BLOCK_SIZE=1024;
    static constexpr int DEFAULT_BLOCK_COUNT=64;
    static constexpr int ROOT_BLOCK=0;
    static constexpr uint16_t EMPTY_BLOCK=0x0000;
    static constexpr uint16_t LAST_BLOCK=0xFFFF;

private:
    int blockCount;
    vector<uint16_t> fat;
    fstream diskFile;
    string lastError;

    // 仅由openDisk在确认目标不存在后调用，建立FAT和数据区。
    bool createDisk(const string& path);

    // 从文件偏移0读取blockCount*2字节，按小端解码并校验。
    // 全部成功才更新成员fat；读取或校验失败保留原fat，不修改磁盘。
    bool loadFat();

    // 将成员fat按小端编码写入文件开头，只覆盖FAT区域并flush。
    // 未打开、缓存长度不符、定位或写入失败均返回false并记录lastError。
    bool saveFat();

public:
    // 只准备FAT缓存并保留根目录块，不创建或打开文件。
    // 块数须为8至1024之间的2的幂，否则抛出invalid_argument。
    explicit DiskManager(int blockCount=DEFAULT_BLOCK_COUNT);

    // 不存在则创建，存在则校验并打开；不覆盖已有的非法虚盘。
    bool openDisk(const string& path);

    // 成功读取完整1024字节；失败清空buffer。块号范围为[0,blockCount)。
    bool readBlock(int blockNo,vector<char>& buffer);

    // buffer必须恰好1024字节；只有写入并同步成功才返回true。
    bool writeBlock(int blockNo,const vector<char>& buffer);

    // 从整文件偏移开始，只读显示原始字节；到文件末尾时缩短显示长度。
    bool showInfo(int byteOffset,int byteCount=256);

    // count须为正；失败firstBlock=-1，空间不足不留下部分分配。
    bool allocateBlocks(int count,int& firstBlock);

    // 输出完整块链；-1表示空链。失败清空blocks并记录原因。
    bool getChain(int firstBlock,vector<int>& blocks);

    // 目录扩展预留：向合法非空链追加count个块，空间不足保持原链。
    bool extendChain(int firstBlock,int count);

    // 回收整条合法链；空链(-1)成功，不允许回收根目录链。
    bool freeChain(int firstBlock);

    // 只统计FAT缓存中值为EMPTY_BLOCK的表项；不会创建或读写文件。
    int getFreeBlockCount() const;

    // 额外辅助显示，与讲义的info原始字节查看不同。
    void showFat() const;

    // bool方法失败时记录原因，成功时清除旧错误。
    const string& getLastError() const;
};

#endif //OS_DISK_H
