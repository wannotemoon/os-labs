#ifndef OS_FILE_SYSTEM_H
#define OS_FILE_SYSTEM_H

#include <string>
#include <vector>
#include "disk.h"
#include "fcb.h"
using namespace std;

class FileSystem {
private:
    // 只保存当前导航路径，目录树本身保存在虚盘中。
    struct DirectoryPosition {
        string name;
        int firstBlock;
    };

    DiskManager disk;
    vector<DirectoryPosition> currentPath;
    string lastError;

    // 校验单个新名称；拒绝空名称、分隔符、保留名称及超过8字节的名称。
    bool isValidName(const string& name);
    string makeDateTime() const;

    // entryIndex为块内0至31号槽位；固定32字节，按小端显式编解码。
    bool readEntry(int blockNo,int entryIndex,FCB& entry);
    bool writeEntry(int blockNo,int entryIndex,const FCB& entry);

public:
    // 只准备管理对象；open成功前不执行目录操作。
    explicit FileSystem(int blockCount=DiskManager::DEFAULT_BLOCK_COUNT);

    // 打开/新建虚盘，成功后当前目录回到根；失败时不进入命令循环。
    bool open(const string& diskPath);

    // MD：在当前目录创建空子目录，初始分配1个块，大小记1024字节。
    bool makeDirectory(const string& name);

    // CD：进入子目录，或用..返回上级、用反斜杠回根；失败路径不变。
    bool changeDirectory(const string& target);

    // RD：只删除当前目录中的空子目录，不删除文件或根目录。
    bool removeDirectory(const string& name);

    // MK：创建指定字节大小的文件；大小可为0，同目录中名称不能重复。
    bool makeFile(const string& name,int sizeBytes);

    // DEL：只删除普通文件，回收全部块并使其目录项失效。
    bool deleteFile(const string& name);

    // DIR：仅输出有效目录项，格式按讲义图4-1；读盘失败返回false。
    bool listDirectory();

    // info：转交DiskManager查看整文件字节，不改变当前目录和磁盘。
    bool showDiskInfo(int byteOffset,int byteCount=256);

    // 返回根路径或带尾部反斜杠的当前路径；尚未open时返回空字符串。
    string getCurrentPath() const;

    // bool方法失败时记录原因，成功时清除旧错误；业务错误由命令入口显示。
    const string& getLastError() const;
};

#endif //OS_FILE_SYSTEM_H
