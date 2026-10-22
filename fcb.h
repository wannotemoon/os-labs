#ifndef OS_FCB_H
#define OS_FCB_H

#include <string>
using namespace std;

// 内存中的目录项；磁盘上另按固定32字节编码，不能直接写入整个对象。
struct FCB {
    static constexpr int DISK_SIZE=32;
    static constexpr int NAME_SIZE=8;
    static constexpr int DATETIME_SIZE=15;

    string name;
    int size=0;            // 文件：逻辑字节数；目录：已分配空间的字节数
    int firstBlock=-1;     // 空文件为-1；根目录为0
    char type=0;          // 0：未使用或已删除，1：文件，2：目录
    string datetime;      // yyyymmdd hhmmss
};

#endif //OS_FCB_H
