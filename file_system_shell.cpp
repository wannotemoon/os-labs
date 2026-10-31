#include "file_system_shell.h"
#include "file_system.h"
#include <cctype>
#include <charconv>
#include <iostream>
#include <sstream>
#include <system_error>

namespace {
bool parseInteger(const string& text,int& value) {
    // [讲解] from_chars不抛异常；既检查越界，也检查是否完整消费了字符串。
    // 因此2000abc、1.5、超长整数不会被当成合法的文件大小或偏移。
    if (text.empty()) {
        return false;
    }
    auto result=from_chars(text.data(),text.data()+text.size(),value);
    return result.ec==errc{} && result.ptr==text.data()+text.size();
}
}

void runFileSystem() {
    cout<<"虚盘路径（默认:os-lab3.disk）：";
    string path;
    if (!getline(cin,path)) {
        return;
    }
    if (path.empty()) {
        path="os-lab3.disk";
    }
    cout<<"数据块数8/16/32/64/128/256/512/1024（默认:64）：";
    string line;
    if (!getline(cin,line)) {
        return;
    }
    int blocks=DiskManager::DEFAULT_BLOCK_COUNT;
    if (!line.empty()) {
        istringstream input(line);
        string token,extra;
        if (!(input>>token) || (input>>extra) || !parseInteger(token,blocks)) {
            cout<<"错误：块数必须是完整的十进制整数"<<endl;
            return;
        }
    }
    // 容量不合法时构造函数会抛异常；只退出本实验入口，不结束原进程实验。
    try {
        FileSystem system(blocks);
        if (!system.open(path)) {
            cout<<"错误："<<system.getLastError()<<endl;
            return;
        }
        cout<<"MD name | CD [name/../\\] | RD name | MK name bytes | DEL name | DIR"<<endl;
        cout<<"info offset 查看虚盘字节；EXIT 返回主菜单。名称区分大小写，最多8个UTF-8字节，不含空白。"<<endl;
        while (true) {
            cout<<system.getCurrentPath()<<"> "<<flush;
            if (!getline(cin,line)) {
                return;
            }
            istringstream input(line);
            vector<string> tokens;
            string token;
            while (input>>token) {
                tokens.push_back(token);
            }
            if (tokens.empty()) {
                continue;
            }
            string command=tokens[0];
            for (char& ch : command) {
                ch=static_cast<char>(toupper(static_cast<unsigned char>(ch)));
            }
            bool success=false;
            int number=0;
            // [讲解] 每次解析一整行，参数错误不会把残留输入带到下一条命令。
            // 只转换关键字，不改文件名；不调用Windows命令，不操作电脑上的真实目录。
            if (command=="EXIT" && tokens.size()==1) {
                return;
            } else if (command=="MD" && tokens.size()==2) {
                success=system.makeDirectory(tokens[1]);
            } else if (command=="CD" && tokens.size()<=2) {
                success=tokens.size()==1 || system.changeDirectory(tokens[1]);
                if (success) {
                    cout<<system.getCurrentPath()<<endl;
                }
            } else if (command=="RD" && tokens.size()==2) {
                success=system.removeDirectory(tokens[1]);
            } else if (command=="MK" && tokens.size()==3 && parseInteger(tokens[2],number)) {
                success=system.makeFile(tokens[1],number);
            } else if (command=="DEL" && tokens.size()==2) {
                success=system.deleteFile(tokens[1]);
            } else if (command=="DIR" && tokens.size()==1) {
                success=system.listDirectory();
            } else if (command=="INFO" && tokens.size()==2 && parseInteger(tokens[1],number)) {
                success=system.showDiskInfo(number);
            } else {
                cout<<"错误：未知命令或参数格式不正确"<<endl;
                continue;
            }
            if (!success) {
                cout<<"错误："<<system.getLastError()<<endl;
            }
        }
    } catch (const exception& error) {
        cout<<"文件系统错误："<<error.what()<<endl;
    }
}
