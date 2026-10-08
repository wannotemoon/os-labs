#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

#include "process.h"

using namespace std;

int main() {

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    ProcessManager manager(64);

    int choice;
    string name;
    int size;

    cout<<"-------- 进程控制实验 --------"<<endl;

    while (true) {
        cout<<endl;
        cout<<"1. 创建进程"<<endl;
        cout<<"2. 时间片到"<<endl;
        cout<<"3. 阻塞当前进程"<<endl;
        cout<<"4. 唤醒阻塞进程"<<endl;
        cout<<"5. 终止当前进程"<<endl;
        cout<<"6. 查看进程状态"<<endl;
        cout<<"7. 查看内存状态"<<endl;
        cout<<"8. 查看全部状态"<<endl;
        cout<<"0. 退出"<<endl;
        cout<<"请选择：";

        cin>>choice;
        cout<<endl;

        switch (choice) {
        case 1:
            cout<<"请输入进程名称：";
            cin>>name;
            cout<<"请输入进程大小(KB)：";
            cin>>size;

            if (manager.createProcess(name, size)) {
                cout<<"进程创建成功"<<endl;
            } else {
                cout<<"进程创建失败"<<endl;
            }

            manager.showProcess();
            manager.showMemory();
            break;

        case 2:
            if (manager.timeOut()) {
                cout<<"时间片到并且已经重新调度"<<endl;
            } else {
                cout<<"当前无运行进程"<<endl;
            }

            manager.showProcess();
            break;

        case 3:
            if (manager.blockProcess()) {
                cout<<"当前进程已阻塞"<<endl;
            } else {
                cout<<"当前无运行进程"<<endl;
            }

            manager.showProcess();
            break;

        case 4:
            if (manager.wakeProcess()) {
                cout <<"已唤醒一个阻塞进程"<< endl;
            } else {
                cout<<"阻塞队列为空，唤醒失败"<< endl;
            }

            manager.showProcess();
            break;

        case 5:
            if (manager.terminateProcess()) {
                cout <<"当前进程已终止"<< endl;
            } else {
                cout <<"无可终止的运行进程"<< endl;
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
        case 0:
            cout<<"程序结束"<<endl;
            return 0;
        default:
            cout<<"输入无效"<<endl;
            break;
        }
    }
}