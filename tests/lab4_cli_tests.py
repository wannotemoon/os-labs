"""实验四真实菜单回归：数值、输入恢复、批次隔离、极值和独立逐时刻模型。"""
from decimal import Decimal, getcontext
from pathlib import Path
import random
import re
import subprocess
import sys
import unicodedata

getcontext().prec = 70
executable = Path(sys.argv[1]).resolve()
passed = scenarios = 0
maximum = 2**63 - 1


def check(condition, description):
    global passed
    if not condition:
        raise AssertionError(description)
    passed += 1


def run(lines):
    global scenarios
    result = subprocess.run(
        [str(executable)], input=("\n".join(map(str, lines)) + "\n").encode("utf-8"),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=executable.parent, timeout=40,
    )
    check(result.returncode == 0 and not result.stderr,
          f"unexpected exit {result.returncode}: {result.stderr!r}")
    scenarios += 1
    return result.stdout.decode("utf-8", errors="strict").replace("\r\n", "\n")


# job为(name, size, arrival, burst, resident)，对应菜单的一行五项。
def batch(algorithm, jobs, quantum=2):
    return [algorithm, *([quantum] if algorithm == 3 else []), len(jobs),
            *(" ".join(map(str, job)) for job in jobs)]


def summaries(output):
    result = []
    for text in output.split("-------- 调度统计（按完成顺序） --------")[1:]:
        rows = []
        for match in re.finditer(r"^(\d+)[ \t]+(\S+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+(\d+)[ \t]+([\d.eE+-]+)$", text, re.M):
            pid, name, arrival, burst, finish, turnaround, weighted = match.groups()
            rows.append((int(pid), name, int(arrival), int(burst), int(finish), int(turnaround), Decimal(weighted)))
        count = re.search(r"已完成进程数：(\d+)", text)
        average = re.search(r"平均周转时间：([\d.eE+-]+)", text)
        weighted = re.search(r"平均带权周转时间：([\d.eE+-]+)", text)
        result.append((rows, int(count[1]) if count else None,
                       Decimal(average[1]) if average else None,
                       Decimal(weighted[1]) if weighted else None))
    return result


def verify(summary, jobs, finish):
    rows, count, average, average_weighted = summary
    check(len(rows) == count == len(finish), "statistics count only completed processes")
    check([row[0] for row in rows] == sorted(finish, key=finish.get), "statistics follow completion order")
    turns, weights = [], []
    for row in rows:
        pid, name, arrival, burst, end, turn, weight = row
        job = jobs[pid - 1]
        expected_turn = finish[pid] - job[2]
        expected_weight = Decimal(expected_turn) / job[3]
        check((name, arrival, burst, end, turn) == (job[0], job[2], job[3], finish[pid], expected_turn),
              f"per-process times for PID {pid}: {row}")
        check(abs(weight - expected_weight) <= Decimal("0.00051"), "weighted turnaround uses floating division")
        turns.append(Decimal(expected_turn))
        weights.append(expected_weight)
    check(abs(average - sum(turns) / count) <= Decimal("0.00051"), "mean turnaround")
    check(abs(average_weighted - sum(weights) / count) <= Decimal("0.00051"), "mean of individual weighted turnarounds")


jobs = [("A", 1024, 0, 3, 1), ("B", 1024, 2, 1, 1)]
output = run([13, *batch(1, jobs), *batch(2, jobs), *batch(3, jobs), 0, 0])
stats = summaries(output)
check(len(stats) == 3 and output.count("本批调度完成") == 3, "three algorithms run as separate batches")
for summary, finish in zip(stats, [{1: 3, 2: 4}, {1: 3, 2: 4}, {1: 4, 2: 3}]):
    verify(summary, jobs, finish)
check("程序结束" in output, "submenu returns to main menu")

# 按终端显示列而非UTF-8字节检查对齐；覆盖中文、长名称和19位时间。
alignment_jobs = [("A", 1024, 0, 2, 1), ("中文进程", 1024, 1, 3, 1),
                  ("LongProcessName_123456789", 1024, 0, 1, 1), ("Éclair", 1024, maximum - 1, 1, 1)]
aligned = run([13, *batch(1, alignment_jobs), 0, 0])
verify(summaries(aligned)[0], alignment_jobs, {1: 2, 2: 6, 3: 3, 4: maximum})
table = aligned.split("-------- 调度统计（按完成顺序） --------")[1].split("已完成进程数：")[0]
table_lines = [line for line in table.splitlines() if line.strip()]


def terminal_width(text):
    return sum(0 if unicodedata.combining(char) else 2 if unicodedata.east_asian_width(char) in "WF" else 1
               for char in text)


positions = None
for line in table_lines:
    check("\t" not in line, "table padding uses spaces instead of tab stops")
    starts = [terminal_width(line[:match.start()]) for match in re.finditer(r"\S+", line)]
    check(len(starts) == 7, "all seven columns remain distinct")
    if positions is None:
        positions = starts
    check(starts == positions, "headers and data start at identical terminal columns")

# 非法整数、记录字段、资源参数都能在当前题目重输；失败不消费PID。
output = run([
    13, "x", "1 extra", "1.5", 9, 3,
    0, -1, maximum + 1, 2,
    0, 65, "2.5", "2 extra", 2,
    "", "bad", "A 0 0 3 1", "A 1024 -1 3 1", "A 1024 0 0 1", "A 1024 0 3 0",
    "A 2147483648 0 3 1", "A 1024 0.5 3 1", "A 1024 0 3 1",
    "B 1024 2 1 1 extra", "B 1024 2 1 1", 0, 0,
])
check(output.count("进程创建失败：") == 4 and "输入无效" in output, "invalid input recovers without starting a bad task")
verify(summaries(output)[0], jobs, {1: 4, 2: 3})

# 取消时析构本批对象，既能释放全部物理页，也能释放预留交换块。
for size, resident in [(65536, 64), (131072, 1)]:
    fresh = [("fresh", size, 0, 1, resident)]
    output = run([13, 1, 2, f"full {size} 0 1 {resident}", "blocked 1024 0 1 1", 0,
                  *batch(1, fresh), 0, 0])
    check("进程创建失败：" in output and "已取消本批调度" in output, "resource-full batch can be cancelled")
    check(len(summaries(output)) == 1, "cancelled tasks are not reported as completed")
    verify(summaries(output)[0], fresh, {1: 1})

full_batch = [(f"P{i}", 1024, 0, 1, 1) for i in range(1, 65)]
output = run([13, *batch(1, full_batch), 0, 0])
verify(summaries(output)[0], full_batch, {i: i for i in range(1, 65)})

# 周转量之和超过long long，单个进程时间和总CPU时间仍合法。
large = [("large-A", 1024, 0, maximum // 2, 1), ("large-B", 1024, 0, maximum // 2, 1)]
output = run([13, *batch(1, large), 0, 0])
verify(summaries(output)[0], large, {1: maximum // 2, 2: 2 * (maximum // 2)})

late = [("late", 1024, maximum - 1, 1, 1)]
output = run([13, *batch(3, late, maximum), 0, 0])
verify(summaries(output)[0], late, {1: maximum})

partial = [("done", 1024, 0, 1, 1), ("overflow", 1024, 1, maximum, 1)]
output = run([13, *batch(1, partial), 0, 0])
check("本批未全部完成" in output and "以下只统计已完成进程" in output, "partial results are explicitly labelled")
verify(summaries(output)[0], partial, {1: 1})

impossible = [("overflow", 1024, maximum, 1, 1)]
output = run([13, *batch(1, impossible), 0, 0])
rows, count, average, weighted = summaries(output)[0]
check(not rows and count is None and average is None and weighted is None, "empty completion has no invented mean or division by zero")
check("暂无已完成进程" in output and not re.search(r"\b(nan|inf)\b", output, re.I), "empty statistics message")

# 原主菜单的进程和FIFO统计跨实验四入口保留，cout精度也不能被新表格污染。
output = run([1, "manual", 5000, 3, 13, *batch(3, jobs), 0,
              8, 9, 0, "N", 9, 1024, "N", 9, 3072, "N", 11, 1, 5, 7, 0])
check("running:  1:manual" in output and "总容量 65536，空闲 62464" in output, "manual batch is isolated from scheduled batches")
check("缺页率：33.3333%" in output, "statistics restores cout formatting")
check("总容量 65536，空闲 65536" in output and "当前进程已终止" in output, "manual resources still release normally")

for lines in [[13], [13, 3], [13, 3, 2], [13, 1, 1], [13, 1, 2, "A 1024 0 1 1"],
              [13, 1, 1, "A 1024 0 1 1"], [13, 1, 2, 0]]:
    run(lines)
    check(True, "EOF at each new input position terminates cleanly")


def tick_model(jobs, algorithm, quantum):
    remaining = [job[3] for job in jobs]
    admitted = set()
    ready, finish = [], {}
    clock, active, used = 0, None, 0

    def admit():
        for index, job in enumerate(jobs):
            if index not in admitted and job[2] == clock:
                admitted.add(index)
                ready.append(index)

    while len(finish) < len(jobs):
        admit()
        if active is None and ready:
            position = min(range(len(ready)), key=lambda i: jobs[ready[i]][3]) if algorithm == 2 else 0
            active, used = ready.pop(position), 0
        if active is None:
            clock += 1
            continue
        remaining[active] -= 1
        clock += 1
        used += 1
        admit()
        if remaining[active] == 0:
            finish[active + 1] = clock
            active = None
        elif algorithm == 3 and used == quantum:
            ready.append(active)
            active = None
    return finish


generator = random.Random(20261105)
lines, cases = [13], []
for trial in range(20):
    sample = [(f"J{i}", 1024, generator.randrange(9), generator.randrange(1, 8), 1)
              for i in range(generator.randrange(1, 7))]
    quantum = generator.randrange(1, 5)
    for algorithm in [1, 2, 3]:
        lines.extend(batch(algorithm, sample, quantum))
        cases.append((sample, tick_model(sample, algorithm, quantum)))
output = run([*lines, 0, 0])
stats = summaries(output)
check(len(stats) == len(cases) == 60, "all model batches produce statistics")
for summary, (sample, finish) in zip(stats, cases):
    verify(summary, sample, finish)

print(f"Passed: {passed} Failed: 0; CLI scenarios: {scenarios}; independent model batches: {len(cases)}")
