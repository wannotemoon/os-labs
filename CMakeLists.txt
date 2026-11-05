cmake_minimum_required(VERSION 4.2)
project(os)

set(CMAKE_CXX_STANDARD 20)

add_executable(os main.cpp
        pcb.h
        pcb_queue.h
        process.h
        memory.h
        pcb_queue.cpp
        memory.cpp
        process.cpp
        bitmap.h
        bitmap.cpp
        fcb.h
        disk.h
        file_system.h
        file_system.cpp
        file_system_shell.h
        file_system_shell.cpp
        disk.cpp
        scheduling_shell.h
        scheduling_shell.cpp
        banker.h
        banker.cpp
        banker_shell.h
        banker_shell.cpp
)

# 可重复运行的正式接口测试；所有虚盘样本生成在构建目录，不改用户的磁盘文件。
include(CTest)
if(BUILD_TESTING)
    add_executable(banker_tests tests/banker_tests.cpp banker.cpp)
    target_compile_options(banker_tests PRIVATE $<$<CXX_COMPILER_ID:GNU>:-ftrapv>)
    add_test(NAME banker-algorithm COMMAND banker_tests)
    add_executable(lab3_tests tests/lab3_tests.cpp disk.cpp file_system.cpp)
    add_test(NAME lab3-filesystem COMMAND lab3_tests "${CMAKE_CURRENT_BINARY_DIR}/lab3-test-data")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME lab3-command-line COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/lab3_cli_tests.py"
                "$<TARGET_FILE:os>" "${CMAKE_CURRENT_BINARY_DIR}/lab3-test-data")
        add_test(NAME lab4-command-line COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/lab4_cli_tests.py" "$<TARGET_FILE:os>")
        add_test(NAME banker-command-line COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/banker_cli_tests.py" "$<TARGET_FILE:os>")
    endif()
endif()
