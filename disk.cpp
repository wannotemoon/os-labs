//
// Created by 28794 on 2026/10/22.
//

#include "disk.h"
#include <iostream>
#include <stdexcept>
// 当前没有输入输出操作，<iostream>暂时用不到；不要依赖其他头文件间接提供异常类型。
using namespace std;

DiskManager::DiskManager(int blockCount) {
    int bc=blockCount;
    if (bc<8 || bc>1024) {
        throw invalid_argument("blockCount must be 8~1024");
    }
    while (bc%2==0) {
        bc/=2;
    }
    if (bc==1) {
        this->blockCount = blockCount;
        fat.assign(blockCount, EMPTY_BLOCK);
        fat[0]=LAST_BLOCK;
        //uint16_t是什么单位？
        /* [讲解：uint16_t]
         * 它是<cstdint>提供的“无符号16位整数类型”，本身不是容量单位。
         * 取值为0至65535，在当前平台占2字节；因此FAT16的每个表项占2字节。
         * FAT[i]表示第i个磁盘块的状态：0x0000为空闲，0xFFFF为链尾，
         * 其他合法块号表示下一块在哪里。这些值不是文件大小。
         * 每个数据块的容量另由BLOCK_SIZE=1024字节规定。
         * 例如8个FAT表项占16字节，8个数据块则占8192字节。
         */
    }else {
        throw invalid_argument("blockCount must be 2的幂次");
    }
}

int DiskManager::getFreeBlockCount() const {
    int count=0;
    for (uint16_t f : fat) {
        if (f==EMPTY_BLOCK) {
            count++;
        }
    }
    return count;
}
