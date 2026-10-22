//
// Created by 28794 on 2026/10/22.
//

#include "disk.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
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

/* [第二轮：先理解数据流，再实现下面两个方法]
 * 上一轮的fat只是内存里的vector。这一轮让它与虚盘文件中的FAT区域对应：
 * saveFat：成员fat -> 按小端排列的字节 -> diskFile文件开头。
 * loadFat：diskFile文件开头 -> 读取字节 -> 临时FAT -> 校验成功后更新成员fat。
 *
 * [讲解：范围] 每个FAT表项占2字节，本轮只读写blockCount*2字节。
 * 例如8块时处理文件偏移0至15；不要把后面的8个1024字节数据块也写掉。
 * 这两个方法不接收路径、不打开文件，不负责创建根目录或数据区。
 * 它们以后由openDisk/createDisk调用；没有打开文件时返回false并记录原因。
 *
 * [讲解：小端格式] 低8位先存，高8位后存。
 * FAT表项0x0003存成两个字节03 00；0xFFFF存成FF FF。
 * 取低字节可用“值 & 0xFF”，取高字节可用“(值 >> 8) & 0xFF”。
 * 还原数值则是“低字节 | (高字节 << 8)”。
 * 不能把vector对象本身写盘；本项目也不用直接写uint16_t数组来依赖本机字节序。
 *
 * [讲解：文件流] diskFile是类成员fstream，用它完成二进制读写。
 * is_open()检查是否已打开；clear()清除上次读到文件末尾等状态，不会清空文件。
 * seekp(0, ios::beg)把写位置移到文件开头；seekg(0, ios::beg)移动读位置。
 * write(字节数组地址, 字节数)写入原始字节；read(字节数组地址, 字节数)读取。
 * vector<char>的data()可提供缓冲区地址，但须先准备好足够数量的元素。
 * 不用<<或>>处理这些字节，它们会按文本格式输出或跳过空白。
 */

/* [任务1] bool DiskManager::saveFat()
 * [目标] 将当前成员fat完整保存到已经打开的虚盘文件开头。
 *
 * [建议顺序]
 * 1. 检查diskFile已打开，fat的元素数量等于blockCount。
 *    不满足时设置lastError并返回false；在检查前不要写入文件。
 * 2. 准备blockCount*2字节的vector<char>缓冲区。
 *    每个fat表项对应连续两个字节，按低字节、高字节顺序填入。
 * 3. 清除旧的流状态，定位写位置到文件开头，并检查定位是否成功。
 * 4. 写入整段FAT字节，再调用flush()把流的缓冲数据提交给底层。
 *    检查写入和flush结果；失败时设置lastError并返回false。
 * 5. 全部成功后清空lastError，返回true。
 *
 * flush()也不代表完成断电安全保证，本实验暂不设计这类机制。
 */
bool DiskManager::saveFat() {
    if (!diskFile.is_open()) {
        lastError="file open fails";
        return false;
    }
    if (fat.size()!=static_cast<size_t>(blockCount)) {
        lastError="block count mismatch";
        return false;
    }
    vector<char> bf(blockCount*2);
    int loc=0;
    for (uint16_t f:fat) {
        bf[loc++]=f & 0xff;
        bf[loc++]=(f >> 8) & 0xff;
    }
    diskFile.clear();
    diskFile.seekp (0, ios::beg);
    /* [讲解：get / put]
     * g来自get（读取、输入），p来自put（写入、输出）。
     * seek是定位，所以seekg定位读位置，seekp定位写位置；tellg/tellp则查询位置。
     * ios::beg表示以文件开头为基准，0表示相对开头偏移0字节。
     */
    if (!diskFile) {
        lastError="I/O fails";
        return false;
    }
    diskFile.write(bf.data(), blockCount*2);
    //为什么直接写bf不代表初始地址，一定要用.data()?
    /* [讲解：vector对象和它管理的数组]
     * bf是一个vector对象，内部管理元素所在的存储区、长度、容量等信息。
     * 它不是C风格数组，不能自动变成首元素指针；&bf也是对象地址，不是元素地址。
     * bf.data()返回连续元素区的首地址，write需要的正是这个地址和要写的字节数。
     * 你这里使用bf.data()是对的；后面loadFat的read也应指向元素区。
     */

    if (!diskFile) {
        lastError="I/O fails";
        return false;
    }

    diskFile.flush();
    /* [讲解：flush]
     * 文件流可能先把write的数据放在缓冲区里。flush要求把待输出数据提交到底层。
     * 它不是清空文件，也不是清除错误标志；错误状态由clear处理。
     * 部分写入错误可能到flush时才体现，所以写入和flush之后都要检查状态。
     * 本实验只要求确认流操作成功，flush本身并不保证突然断电时数据也一定保住。
     */
    //整个文件操作包围上try catch吗，然后有错就设置lastError?
    /* [讲解：先检查流状态，不用靠try/catch代替]
     * 当前diskFile没有启用exceptions异常掩码，普通I/O失败通常只设置failbit/badbit，
     * 不会自动抛出异常。因此只包try/catch仍可能漏掉定位、写入或flush失败。
     */

    if (!diskFile) {
        lastError="I/O fails";
        return false;
    }

    lastError="";
    return true;
}

/* [任务2] bool DiskManager::loadFat()
 * [目标] 从已经打开的虚盘文件开头恢复FAT；失败时保留原成员fat。
 *
 * [建议顺序]
 * 1. 检查diskFile已打开；准备blockCount*2字节的读取缓冲区。
 * 2. 清除旧流状态，定位读位置到文件开头，读取完整FAT区域。
 *    检查定位和读取结果；gcount()返回最近一次read实际读取的字节数。
 *    文件过短或其他读取失败时，设置lastError并返回false。
 * 3. 将字节解码到一个临时vector<uint16_t>，不要边读边覆盖成员fat。
 *    char在当前工具链可能带符号，FF可能读成负数；参与移位与组合前，
 *    先把读取到的char转换成unsigned char，再作为无符号字节使用。
 * 4. 检查临时表项：只能是EMPTY_BLOCK、LAST_BLOCK，或1至blockCount-1的下一块号。
 *    根目录的ROOT_BLOCK表项不能是EMPTY_BLOCK，根目录块始终保留。
 *    注意：根目录以后可能扩展，不能把“根表项必须永远是LAST_BLOCK”写死。
 * 5. 全部读取并校验通过后，再一次性用临时表替换成员fat；清空lastError，返回true。
 */
bool DiskManager::loadFat() {
    if (!diskFile.is_open()) {
        lastError="file open fails";
        return false;
    }
    diskFile.clear();
    diskFile.seekg(0, ios::beg);

    if (!diskFile) {
        lastError="I/O fails";
        return false;
    }

    vector<char> bf(blockCount*2);
    vector<uint16_t> fatt;
    diskFile.read((char*)bf.data(), blockCount*2);

    if (diskFile.gcount()!=blockCount*2) {
        lastError="file read error";
        return false;
    }

    if (!diskFile) {
        lastError="I/O fails";
        return false;
    }

    for (size_t i=0;i<bf.size();i+=2) {
        fatt.push_back((unsigned char)bf[i] | (unsigned char)bf[i+1] << 8);
    }
    if (fatt[0]==EMPTY_BLOCK) {
        lastError="root block not found";
        return false;
    }else {
        for (uint16_t f:fatt) {
            if (!(f==EMPTY_BLOCK || f==LAST_BLOCK || (f>=1 && f<blockCount))) {
                lastError="block mismatch";
                return false;
            }
        }
        fat=fatt;
        lastError="";
        return true;
    }

}
