#include <iostream>
#include <string>
#include <limits>
#include <exception>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "process.h"

using namespace std;

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    try {
        ProcessManager manager;

        int choice;
        string name;
        int size;
        int residentPages;
        int pid;
        int logicalAddress;
        int physicalAddress;
        string writeFlag;

        cout<<"-------- 进程控制与分页存储管理 --------"<<endl;
        cout<<"内存 64KB，置换空间 128KB，页面 1024 字节，FIFO"<<endl;
        cout<<"换入和写回为管理信息模拟，不读写真实磁盘文件"<<endl;

        while (true) {
            cout<<endl;
            cout<<"1. 创建进程"<<endl;
            cout<<"2. 时间片到"<<endl;
            cout<<"3. 阻塞当前进程"<<endl;
            cout<<"4. 唤醒阻塞进程"<<endl;
            cout<<"5. 终止当前进程"<<endl;
            cout<<"6. 查看进程状态"<<endl;
            cout<<"7. 查看内存和置换空间"<<endl;
            cout<<"8. 查看全部状态"<<endl;
            cout<<"9. 访问当前进程的逻辑地址"<<endl;
            cout<<"10. 查看进程页表"<<endl;
            cout<<"11. 查看进程访问统计"<<endl;
            cout<<"0. 退出"<<endl;
            cout<<"请选择：";

            if (!(cin>>choice)) {
                if (cin.eof()) {
                    break;
                }
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(),'\n');
                cout<<"输入无效"<<endl;
                continue;
            }
            cout<<endl;

            switch (choice) {
            case 1:
                cout<<"请输入进程名称：";
                if (!(cin>>name)) {
                    break;
                }
                cout<<"请输入进程大小(字节)：";
                if (!(cin>>size)) {
                    break;
                }
                cout<<"请输入驻留页数(-1表示默认最多3页)：";
                if (!(cin>>residentPages)) {
                    break;
                }

                if (manager.createProcess(name,size,residentPages)) {
                    cout<<"进程创建成功"<<endl;
                }else {
                    cout<<"进程创建失败，请检查大小、驻留页数和空闲空间"<<endl;
                }
                manager.showProcess();
                manager.showMemory();
                break;

            case 2:
                if (manager.timeOut()) {
                    cout<<"时间片到并且已经重新调度"<<endl;
                }else {
                    cout<<"当前无运行进程"<<endl;
                }
                manager.showProcess();
                break;

            case 3:
                if (manager.blockProcess()) {
                    cout<<"当前进程已阻塞"<<endl;
                }else {
                    cout<<"当前无运行进程"<<endl;
                }
                manager.showProcess();
                break;

            case 4:
                if (manager.wakeProcess()) {
                    cout<<"已唤醒一个阻塞进程"<<endl;
                }else {
                    cout<<"阻塞队列为空，唤醒失败"<<endl;
                }
                manager.showProcess();
                break;

            case 5:
                if (manager.terminateProcess()) {
                    cout<<"当前进程已终止"<<endl;
                }else {
                    cout<<"终止失败：当前无运行进程或资源状态异常"<<endl;
                }
                manager.showProcess();
                manager.showMemory();
                break;

            case 6:
                manager.showProcess();
                break;
            case 7:
                manager.showMemory();
                break;
            case 8:
                manager.showProcess();
                manager.showMemory();
                break;

            case 9:
                cout<<"请输入逻辑地址：";
                if (!(cin>>logicalAddress)) {
                    break;
                }
                cout<<"是否为写访问(Y/N)：";
                if (!(cin>>writeFlag)) {
                    break;
                }
                if (writeFlag!="Y" && writeFlag!="y" && writeFlag!="N" && writeFlag!="n") {
                    cout<<"请输入Y或N"<<endl;
                    break;
                }
                if (manager.accessCurrent(logicalAddress,writeFlag=="Y" || writeFlag=="y",physicalAddress)) {
                    cout<<"逻辑地址 "<<logicalAddress<<" 对应的物理地址为 "<<physicalAddress<<endl;
                }else {
                    cout<<"访问失败：当前无运行进程或逻辑地址越界"<<endl;
                }
                break;

            case 10:
                cout<<"请输入进程编号：";
                if (!(cin>>pid)) {
                    break;
                }
                if (!manager.showPageTable(pid)) {
                    cout<<"进程不存在"<<endl;
                }
                break;

            case 11:
                cout<<"请输入进程编号：";
                if (!(cin>>pid)) {
                    break;
                }
                if (!manager.showStatistics(pid)) {
                    cout<<"进程不存在"<<endl;
                }
                break;

            case 0:
                cout<<"程序结束"<<endl;
                return 0;
            default:
                cout<<"输入无效"<<endl;
                break;
            }

            if (!cin) {
                if (cin.eof()) {
                    break;
                }
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(),'\n');
                cout<<"输入无效，请重新选择操作"<<endl;
            }
        }
    }catch (const exception& e) {
        cout<<"程序错误："<<e.what()<<endl;
        return 1;
    }
    return 0;
}
