//
// Created by 28794 on 2026/10/22.
//

#include "disk.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <algorithm>
#include <iomanip>
#include <sstream>
// showInfo使用iostream输出、iomanip排版；显式包含所需头文件，不依赖间接包含。
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

/* [第三轮：创建与打开完整虚盘]
 * 本轮原计划由你实现；按你这次的要求，由助手补全这两个方法，供你对照阅读。
 * saveFat/loadFat已经负责FAT字节转换，这里复用它们，不再写一套编码逻辑。
 *
 * [先看布局]
 * FAT字节数 = blockCount * 2。
 * 数据区字节数 = blockCount * BLOCK_SIZE。
 * 文件总字节数 = 上面两项相加。
 * 8块时是16 + 8192 = 8208字节；默认64块时是128 + 65536 = 65664字节。
 * 新磁盘的FAT只有根目录块为LAST_BLOCK，其他表项均为EMPTY_BLOCK；数据区全部为0。
 * 因此根目录块已经分配，但内部目录项全为空，不需要在这里写入test或a.c。
 *
 * [会用到的接口]
 * 本文件已包含<filesystem>和<system_error>，用于路径检查与错误码。
 * filesystem::exists(path, ec)区分路径是否存在；ec非空表示查询出错。
 * 注意：查询出错不能当作“文件不存在”，更不能因此尝试覆盖创建。
 * filesystem::is_regular_file(path, ec)用于拒绝把目录等对象当成虚盘文件。
 * 对每次查询都检查ec，错误时设置lastError并返回false。
 *
 * [文件流打开模式]
 * ios::in：读取；ios::out：写入；ios::binary：原始字节；ios::trunc：清空原文件。
 * 用|组合这些标志。打开已有虚盘只使用in、out、binary，不能带trunc或app。
 * 新建分支确认路径不存在后，才使用包含trunc的创建模式。
 * 使用成员diskFile.open(...)，不要打开另一个局部流，却让saveFat操作仍未打开的成员流。
 *
 * [本轮调用规则]
 * openDisk是对外入口；确认不存在后才调用createDisk。
 * 同一个对象已经打开虚盘时，先返回false并说明原因，保留原流和fat，不自动切换文件。
 * 成功后保留成员流打开，后续块读写复用它；不要成功后马上close。
 * 失败则关闭本次打开的流，留下lastError；下次重试前清除旧流状态。
 * 已有文件内容始终保留；新文件写入中途失败可能留下未完成文件，不自动删除或覆盖重试。
 */

/* [任务1] bool DiskManager::createDisk(const string& path)
 * [目标] 仅创建一个全新的完整虚盘，成功后可继续用成员流读写。
 *
 * [建议步骤]
 * 1. 拒绝空路径、成员流已打开、路径已存在或存在性查询出错的情况。
 *    即便调用方已检查过，创建前仍再确认一次；已有普通文件也不能清空重建。
 * 2. 为需要修改的成员fat保留一份原缓存，以便后续I/O失败时恢复。
 * 3. 以读写、二进制、创建模式打开成员diskFile，检查打开结果。
 * 4. 准备“文件总字节数”个值为0的char元素，写入完整文件并flush，逐步检查流状态。
 *    这里可以一次准备全文件大小的vector<char>，本实验最大约1MB。
 *    注意0字节是字符'\0'或数值0，不是字符'0'；后者的字节值是0x30。
 * 5. 将成员fat重置为blockCount个EMPTY_BLOCK，再保留ROOT_BLOCK为LAST_BLOCK。
 *    调用已有saveFat，把这张新表写回文件开头；不能沿用旧虚盘缓存的块分配状态。
 * 6. 全部成功才清空lastError、返回true；成员流保持打开。
 *
 * [失败处理]
 * - 打开后发生写入、flush或saveFat失败：恢复原fat，关闭本次打开的成员流，保留失败原因。
 * - 先检查全零写入成功，再调用saveFat；后者会clear旧状态，不能让它掩盖之前的写入失败。
 * - 恢复内存fat不代表新文件的部分写入已经撤销；不自动删除留下的未完成文件。
 * - 这里的fat是文件系统FAT，不使用实验二的页框、页表或置换位示图。
 */
bool DiskManager::createDisk(const string &path) {
    if (path.empty()) {
        lastError="path must not be empty";
        return false;
    }
    if (diskFile.is_open()) {
        lastError="disk already open";
        return false;
    }
    error_code ec;
    bool exists=filesystem::exists(path,ec);
    if (ec) {
        lastError="path query fails: "+ec.message();
        return false;
    }
    if (exists) {
        lastError="file exists";
        return false;
    }
    vector<uint16_t> oldFat=fat;
    int totalBytes=blockCount*2+blockCount*BLOCK_SIZE;
    vector<char> zeros(totalBytes,0);
    diskFile.clear(); // 清除上次失败留下的状态，再尝试打开。
    diskFile.open(path,ios::in|ios::out|ios::binary|ios::trunc);
    if (!diskFile) {
        lastError="cannot create disk file";
        return false;
    }

    diskFile.write(zeros.data(),totalBytes);
    if (!diskFile) {
        lastError="cannot initialize disk file";
        diskFile.close();
        return false;
    }
    diskFile.flush();
    if (!diskFile) {
        lastError="cannot flush disk file";
        diskFile.close();
        return false;
    }

    fat.assign(blockCount,EMPTY_BLOCK);
    fat[ROOT_BLOCK]=LAST_BLOCK;
    if (!saveFat()) {
        fat.swap(oldFat);
        diskFile.close();
        return false;
    }
    lastError="";
    return true;
}

/* [任务2] bool DiskManager::openDisk(const string& path)
 * [目标] 不存在时创建，存在时原样加载；只接受符合当前块数配置的完整虚盘。
 *
 * [建议步骤]
 * 1. 拒绝空路径、成员流已打开；检查路径是否存在，查询出错应报告失败。
 * 2. 路径确实不存在时，转交createDisk，并返回其结果。
 * 3. 路径存在时确认它是普通文件；以in、out、binary打开成员流，检查结果。
 *    此分支绝不带trunc，打开失败也不能回退到“创建新文件”。
 * 4. 用seekg(0, ios::end)定位读位置到文件末尾，再用tellg()取得字节位置。
 *    检查定位和查询是否成功；此时的末尾位置就是文件长度。
 *    文件长度必须恰好等于blockCount*2 + blockCount*BLOCK_SIZE。
 * 5. 长度正确后调用loadFat；它会自己定位回开头，并只在成功时更新成员fat。
 *    如果返回false，关闭本次打开的流并保留它的错误原因，不清空或重写这个文件。
 * 6. 加载成功后清空lastError，返回true；成员流保持打开。
 *
 * [几个容易混淆的边界]
 * - loadFat可以只读一段FAT，但openDisk必须拒绝缺少数据区的文件，两者职责不同。
 * - 比预期短或长都应拒绝；不要修改构造时的blockCount去迁就已有文件长度。
 * - 文件长度正确却FAT非法，也要拒绝，不能因加载失败而格式化重建。
 * - 成功加载后应保留磁盘中已有的分配情况，不要又把fat全重置为空闲。
 * - 本轮仅完成文件长度和已有loadFat的表项检查；块链循环检查仍留给getChain。
 *
 * [阅读提示] 重点区分“不存在时创建”与“存在时校验加载”，失败不能互相回退。
 * 测试虚盘先放在被忽略的构建目录，测试时不需要自己改main或移动私有接口。
 */
bool DiskManager::openDisk(const string &path) {
    if (path.empty()) {
        lastError="path must not be empty";
        return false;
    }
    if (diskFile.is_open()) {
        lastError="disk already open";
        return false;
    }
    error_code ec;
    bool exists=filesystem::exists(path,ec);
    if (ec) {
        lastError="path query fails: "+ec.message();
        return false;
    }
    if (!exists) {
        return createDisk(path);
    }

    bool regularFile=filesystem::is_regular_file(path,ec);
    if (ec) {
        lastError="file type query fails: "+ec.message();
        return false;
    }
    if (!regularFile) {
        lastError="path is not a regular file";
        return false;
    }
    diskFile.clear();
    diskFile.open(path,ios::in|ios::out|ios::binary);
    if (!diskFile) {
        lastError="cannot open disk file";
        return false;
    }

    diskFile.seekg(0,ios::end);
    streampos fileSize=diskFile.tellg();
    if (!diskFile || fileSize==streampos(-1)) {
        lastError="cannot read disk file size";
        diskFile.close();
        return false;
    }
    int totalBytes=blockCount*2+blockCount*BLOCK_SIZE;
    if (fileSize!=streampos(totalBytes)) {
        lastError="disk file size mismatch";
        diskFile.close();
        return false;
    }

    if (!loadFat()) {
        diskFile.close();
        return false;
    }
    lastError="";
    return true;
}

/* [第四轮：按块读写数据区]
 * 前面已经能新建、打开虚盘；现在要能读出或写入其中“指定的一整块”。
 * 本轮只写readBlock和writeBlock，不改main，不实现目录命令或分配算法。
 *
 * [讲解：参数里的块号不是整个文件的字节偏移]
 * 文件布局：[blockCount*2字节FAT][0号数据块][1号数据块]……
 * 某数据块的起始偏移 = blockCount*2 + blockNo*BLOCK_SIZE。
 * 沿用讲义8块案例：0号块从16开始，1号块从1040开始，7号块从7184开始。
 * 默认64块：0号块从128开始。不能省略前面的FAT长度，否则会覆盖分配表。
 * 注意：先检查块号合法，再计算偏移，避免对负数或极大值先做乘法。
 *
 * [职责边界]
 * - 使用已打开的成员diskFile；不接收路径，不重新open，不新建或关闭文件。
 * - 块号有效范围为0到blockCount-1，根目录0号块也允许读写。
 * - 这是原始块读写，不根据fat[blockNo]是否空闲来拒绝操作。
 *   后面分配、清零和目录保存都会复用它；哪些块可用于某个文件由上层决定。
 * - 不改变fat，不调用saveFat；这里只处理数据区的一块原始字节。
 * - 失败设置lastError；成功清空旧错误。不要在方法内部打印提示。
 * - 操作前clear旧流状态，再seek定位；失败后保留流打开，便于下次重试。
 */

/* [任务1] bool DiskManager::readBlock(int blockNo, vector<char>& buffer)
 * [参数与结果]
 * blockNo是要读的数据块号；buffer是输出参数，可以由本方法改变大小和内容。
 * true：buffer恰好包含这一块的1024字节；false：buffer必须为空，不能留下旧内容或半块数据。
 *
 * [建议步骤]
 * 1. 先清空buffer，再检查成员流已打开、块号在范围内；任何失败都设置错误并返回false。
 * 2. 清除流的旧错误状态，用seekg定位到上述“块起始偏移”，检查定位是否成功。
 * 3. 给vector准备BLOCK_SIZE个元素后，再用data()接收read读取的字节。
 *    resize会增加元素数量；reserve只准备容量，不增加元素，不能用来代替resize。
 * 4. read之后检查gcount是否恰好等于BLOCK_SIZE，并检查流状态。
 *    不足一整块也算失败，需要再次清空buffer；不能把末尾的半块当成成功。
 * 5. 完整读取才清空lastError并返回true；流仍然打开，fat与文件都不变。
 *
 * [复用你已写过的知识]
 * 与loadFat类似地定位、读取和检查，但这里读的是数据区，不做FAT解码。
 * read的长度是BLOCK_SIZE，不是sizeof(BLOCK_SIZE)，也不是sizeof(buffer)。
 *
 * [待你实现] 在这段注释下面补本方法定义；先完成读取，再做写入。
 */
bool DiskManager::readBlock(int blockNo, vector<char> &buffer) {
    buffer.clear();
    if (!diskFile.is_open()) {
        lastError="disk is not opened";
        return false;
    }
    if (!(blockNo>=0 && blockNo<blockCount)) {
        lastError="block out of range";
        return false;
    }
    int loc = blockCount * 2 + blockNo * BLOCK_SIZE;
    diskFile.clear();
    diskFile.seekg(loc,ios::beg);
    if (!diskFile) {
        lastError="cannot seek disk file";
        return false;
    }

    buffer.resize(BLOCK_SIZE);
    diskFile.read(buffer.data(),BLOCK_SIZE);
    if (!diskFile) {
        lastError="cannot read disk file";
        buffer.clear();
        return false;
    }
    if (diskFile.gcount()!=BLOCK_SIZE) {
        lastError="disk file size mismatch";
        buffer.clear();
        return false;
    }

    lastError="";
    return true;
}

/* [任务2] bool DiskManager::writeBlock(int blockNo, const vector<char>& buffer)
 * [参数与结果]
 * buffer是只读输入参数，必须已有恰好1024个字节；本方法不能resize、清空或修改它。
 * true表示定位、整块write和flush全部成功；任何一步失败都返回false并设置错误。
 *
 * [建议步骤]
 * 1. 检查流已打开、块号合法、buffer.size()恰好等于BLOCK_SIZE。
 *    所有参数检查都放在写入前；长度不对就拒绝，不补0、不截断、不自动跨块写入。
 *    长度比较注意size_t与int类型，可沿用saveFat中的显式类型转换。
 * 2. 清除旧流状态，用seekp定位到该数据块起始处，检查定位结果。
 * 3. 用buffer.data()写BLOCK_SIZE个字节，检查write结果，再flush并检查结果。
 * 4. 全部成功才清空lastError、返回true；不关闭流，不修改输入buffer和fat。
 *
 * [讲解：get位置与put位置]
 * 读取前seekg，写入前seekp。每次显式定位，不假设上次操作结束的位置正好可用。
 * 这里写原始字节，不能用<<或把buffer当成以'\0'结尾的字符串。
 * 参数错误可以保证不写文件；实际写入中途出错却可能已经写了一部分，不能声称自动回滚。
 *
 * [待你实现] 在这段注释下面补本方法定义；这轮不要继续实现其他方法。
 * 完成后我会测试：首/尾块、二进制字节往返、非法块号、错误长度、短读清空、
 * 失败后重试、读写交替，以及FAT区域和其他数据块是否始终保持不变。
 */
bool DiskManager::writeBlock(int blockNo, const vector<char> &buffer) {
    if (!diskFile.is_open()) {
        lastError="disk is not opened";
        return false;
    }
    if (!(blockNo>=0 && blockNo<blockCount)) {
        lastError="block out of range";
        return false;
    }

    if (buffer.size()!=BLOCK_SIZE) {
        lastError="buffer size must be 1024";
        return false;
    }
    diskFile.clear();
    int loc = blockCount * 2 + blockNo * BLOCK_SIZE;
    diskFile.seekp(loc,ios::beg);
    if (!diskFile) {
        lastError="cannot seek disk file";
        return false;
    }
    diskFile.write(buffer.data(),BLOCK_SIZE);
    /* [讲解：写入与flush后的合并检查]
     * 你现在把检查放在flush之后，仍能发现write或flush留下的错误状态，
     * 因为flush不会清除write已经置上的failbit/badbit；本轮只读流写入测试也能正确拒绝。
     * 若想区分“write失败”和“flush失败”，可以分别检查，但这是错误提示的细化，
     * 不是本次短读输出未清空的原因，不必为此重写整个方法。
     */
    diskFile.flush();
    if (!diskFile) {
        lastError="cannot write disk file";
        return false;
    }

    lastError="";
    return true;
}

/* [第五轮：错误信息读取与讲义info]
 * 已完成的readBlock/writeBlock只操作数据块；本轮开始查看整个虚盘的原始字节。
 * info来自讲义外存方案的第（6）项建议和图4-7，不是新构思的额外命令。
 * 本轮不改main、不接入命令解析、不实现showFat或块分配；先单独完成并验收接口。
 */

/* [任务1] const string& DiskManager::getLastError() const
 * [目标] 让调用者读取这个对象保存的最近一次错误，不再只能在测试中访问私有成员。

 */
const string &DiskManager::getLastError() const {
    return lastError;
}

/* [任务2] bool DiskManager::showInfo(int byteOffset, int byteCount)
 * [讲解：查看的是整个虚盘文件，不是某个数据块]
 * 沿用讲义info 0：从文件偏移0显示最多256字节，FAT也包含在里面。
 * 8块案例：前16字节是FAT，随后才是0号数据块，所以不要再给byteOffset加FAT长度。
 * 默认64块：前128字节是FAT，info 0会显示这些FAT字节和根目录块的前128字节。
 * 本方法不能仅调用readBlock，因为后者跳过了FAT，且只接受整块读取。
 * byteCount=256的默认值已写在disk.h声明中，方法定义不要重复写默认值。
 *
 * [参数与失败处理]
 * - 必须已经打开虚盘，byteOffset非负、严格小于文件长度，byteCount必须大于0。
 * - 先检查再计算；不要先算byteOffset+byteCount，两个极大int相加可能溢出。
 * - 本方法只读，不修改fat或文件，不关闭流；失败记录lastError，成功清空旧错误。
 * - I/O读取失败就返回false，不把半段内容当成功，不在这里直接打印业务错误提示。
 *
 * [建议分成两段：先读取，再排版]
 * 1. 检查打开状态、偏移非负和显示长度为正；clear旧流状态。
 * 2. 与openDisk类似，用seekg到文件末尾、tellg取得实际文件长度，检查流与位置是否有效。
 *    tellg返回的是位置类型，可用streamoff表示字节长度；不要未经检查就缩成int。
 * 3. 确认偏移在文件内，再求“剩余字节数=文件长度-byteOffset”。
 *    实际读取量取byteCount与剩余字节数中较小者，这样靠近末尾能正常缩短。
 *    若用std::min，需<algorithm>且两侧数值类型统一，注意int与streamoff的区别。
 * 4. seekg到byteOffset，检查定位；准备相应长度的vector<char>，read后检查流和gcount。
 *    所需字节完整读到后才输出，避免先显示一部分，随后才发现读取失败。
 * 5. 按下方格式逐行输出；全部完成后清空lastError，返回true。
 *
 * [沿用图4-7的三栏样式，明确本项目的排版约定]
 * 每行最多16个字节：左侧8位补0的十进制绝对偏移，中间大写两位十六进制，右侧ASCII。
 * 假设从偏移0读到FF FF 00 00，右侧对应....；从偏移16读到41 20 42，右侧对应A B。
 * 若byteOffset是17，第一行偏移就显示00000017，不擅自向下对齐到16。
 * 末行不足16字节时，中间缺少的字节位置用空格补齐，让ASCII栏对齐；不要补造00字节。
 * ASCII只显示0x20至0x7E，其余显示点；中文UTF-8字节也按原始值查看，不在此解码。
 *
 * [讲解：十六进制输出的常见陷阱]
 * - 可在本文件显式包含<iomanip>，使用setw/setfill、dec/hex、uppercase等控制格式。
 * - char先转unsigned char再转整数输出，否则可能直接输出字符，或把高位字节显示成负数。
 * - 左侧每行都用十进制偏移；不要让上一行的hex影响下一行的偏移。
 * - setw只影响紧接着的一次输出，每个字节都要考虑宽度；十六进制列用右对齐和补0。
 * - hex、uppercase、对齐方式、fill等会持续影响流；如果直接设置cout，先保存原flags、fill、width，
 *   输出结束后恢复。先完成全部读取检查，再改输出格式，便于避免失败路径漏恢复。
 *   也可在局部ostringstream中排版（需<sstream>），最后输出时仍注意cout原有的width：
 *   用<<输出字符串会消费它，可保存恢复；或用不消费width的cout.write输出已排好的文本。
 */
bool DiskManager::showInfo(int byteOffset, int byteCount) {
    if (!diskFile.is_open()) {
        lastError="disk is not opened";
        return false;
    }
    if (byteOffset<0) {
        lastError="byte offset must not be negative";
        return false;
    }
    if (byteCount<=0) {
        lastError="byte count must be positive";
        return false;
    }

    // [讲解] tellg得到的是文件内位置；streamoff用于表示这个字节长度，避免缩成int。
    diskFile.clear();
    diskFile.seekg(0,ios::end);
    streamoff fileSize=diskFile.tellg();
    if (!diskFile || fileSize<0) {
        lastError="cannot read disk file size";
        return false;
    }
    if (byteOffset>=fileSize) {
        lastError="byte offset out of range";
        return false;
    }
    // [讲解] 先减出剩余量，再取较小值；不计算可能溢出的byteOffset+byteCount。
    streamoff remaining=fileSize-byteOffset;
    streamsize readCount=min<streamoff>(byteCount,remaining);
    vector<char> buffer(static_cast<size_t>(readCount));
    diskFile.seekg(byteOffset,ios::beg);
    if (!diskFile) {
        lastError="cannot seek disk file";
        return false;
    }
    diskFile.read(buffer.data(),readCount);
    if (!diskFile || diskFile.gcount()!=readCount) {
        lastError="cannot read disk info";
        return false;
    }

    // [讲解] 所需字节完整读到后才开始排版。所有hex、补0和对齐设置只作用于局部流。
    ostringstream output;
    output<<right<<uppercase;
    for (size_t row=0;row<buffer.size();row+=16) {
        // 左侧仍是十进制的“整文件绝对偏移”，每行都重新切回dec。
        output<<dec<<setfill('0')<<setw(8)
              <<(static_cast<streamoff>(byteOffset)+static_cast<streamoff>(row))<<"  ";
        output<<hex;
        for (size_t column=0;column<16;column++) {
            if (row+column<buffer.size()) {
                // char先去掉符号，再转整数，才能得到80、FF而不是字符或负数。
                unsigned char value=static_cast<unsigned char>(buffer[row+column]);
                output<<setw(2)<<static_cast<unsigned int>(value)<<' ';
            } else {
                output<<"   "; // 缺少一个字节就留三个空格，不伪造00，右侧ASCII保持对齐。
            }
        }
        output<<"  ";
        for (size_t column=0;column<16 && row+column<buffer.size();column++) {
            unsigned char value=static_cast<unsigned char>(buffer[row+column]);
            if (value>=0x20 && value<=0x7E) {
                output<<static_cast<char>(value);
            } else {
                output<<'.';
            }
        }
        output<<'\n';
    }

    // [讲解] write原样输出排好的文本，不使用或消耗cout原有的宽度和数字格式。
    string text=output.str();
    cout.write(text.data(),static_cast<streamsize>(text.size()));
    if (!cout) {
        lastError="cannot display disk info";
        return false;
    }
    lastError="";
    return true;
}
