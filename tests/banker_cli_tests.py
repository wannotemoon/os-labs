"""实际菜单与演示脚本回归；资源状态另用穷举安全序列核对。"""
from itertools import permutations
from pathlib import Path
import random
import re
import subprocess
import sys

executable = Path(sys.argv[1]).resolve()
project = Path(__file__).resolve().parents[1]
passed = scenarios = 0


def check(condition, description):
    global passed
    if not condition:
        raise AssertionError(description)
    passed += 1


def run(lines):
    global scenarios
    data = "\n".join(map(str, lines)) + "\n"
    result = subprocess.run([str(executable)], input=data.encode("utf-8"), stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, cwd=executable.parent, timeout=30)
    check(result.returncode == 0 and not result.stderr, f"exit {result.returncode}: {result.stderr!r}")
    scenarios += 1
    return result.stdout.decode("utf-8", errors="strict").replace("\r\n", "\n")


def vector(text):
    return tuple(map(int, text.split()))


def states(output):
    result = []
    for match in re.finditer(r"Available: \[([^\n]*)\]\nPID[^\n]*\n((?:P\d+[^\n]*\n)+)", output):
        rows = []
        for row in re.finditer(r"P(\d+)\s+\[([\d ]*)\]\s+\[([\d ]*)\]\s+\[([\d ]*)\]", match[2]):
            rows.append((int(row[1]), vector(row[2]), vector(row[3]), vector(row[4])))
        result.append((vector(match[1]), tuple(rows)))
    return result


def expected_state(available, maximum, allocation):
    return (tuple(available), tuple((i, tuple(maximum[i]), tuple(allocation[i]),
                                    tuple(a-b for a, b in zip(maximum[i], allocation[i])))
                                   for i in range(len(maximum))))


available = [3, 3, 2]
maximum = [[7, 5, 3], [3, 2, 2], [9, 0, 2], [2, 2, 2], [4, 3, 3]]
allocation = [[0, 1, 0], [2, 0, 0], [3, 0, 2], [2, 1, 1], [0, 0, 2]]
classic = expected_state(available, maximum, allocation)

output = run((project / "samples/banker-demo-input.txt").read_text(encoding="utf-8").splitlines())
check("系统安全，安全序列：P1 -> P3 -> P4 -> P0 -> P2" in output, "classic preset safe sequence")
check("系统不安全" in output and "仅能假想完成的前缀：P0" in output, "preset unsafe prefix is not a complete safe sequence")
check("不等于此刻已经发生死锁" in output, "unsafe and deadlocked are distinguished")
check("Work [3 3 2] + Allocation [2 0 0] = [5 3 2]" in output, "readable safety proof")
check("程序结束" in output, "demonstration script returns to original menu")
for title, message in [("安全请求", "请求获准"), ("超过剩余需求", "超过该进程剩余需求Need"),
                       ("可用资源不足", "请求暂缓"), ("试分配不安全", "试分配后不安全")]:
    block = output.split(f"-------- 演示：{title} --------")[1].split("-------- 演示：")[0]
    tables = states(block)
    check(message in block and len(tables) == 2 and tables[0] == classic, f"fresh baseline for {title}")
    if title == "安全请求":
        changed = [row[:] for row in allocation]
        changed[1] = [3, 0, 2]
        check(tables[1] == expected_state([2, 3, 0], maximum, changed), "approved example commits exact request")
    else:
        check(tables[1] == classic, f"rejected {title} preserves all matrix cells")
check("Work初值=[0 0 2]" in output and "没有进程满足Need<=Work" in output, "unsafe trial diagnosis")

output = run((project / "samples/banker-manual-safe-input.txt").read_text(encoding="utf-8").splitlines())
check(states(output)[0] == classic and states(output)[-1][0] == (2, 3, 0), "manual sample matches preset and commits grant")
check(output.count("系统安全，安全序列：") == 2, "manual sample checks before and after request")

output = run([14, 1, 2, 1, 3, 4, "3 3 0", 3, 1, "1 0 2", 2, 1, 0, 0, 0])
tables = states(output)
check(tables[0] == tables[1] == tables[2] == classic, "checks and unsafe request preserve the live session")
check(tables[-1] == tables[-2] and tables[-1][0] == (2, 3, 0), "approved state persists across subsequent checks")

output = run([14, "x", 9, "1 extra", 4, "x", -1, 21, 1, -1, 11, "1 extra", 1,
              "", -1, "1 2", "1.5", "9" * 70, 1, 1, 0,
              3, -1, 1, 0, -1, "0 0", "1x", 0, 3, 0, "q", 1, 0, 0, 0])
check("输入无效" in output and "请求获准" in output and "已取消本次请求" in output, "malformed input and q recover in place")
check(states(output)[0] == states(output)[-1] == expected_state([1], [[1]], [[0]]), "zero request and cancelled input keep state")

for lines in [[14, 4, 0, 0, 0], [14, 4, 1, 0, 0, 0],
              [14, 4, 1, 1, "q", 0, 0], [14, 4, 1, 1, 1, "q", 0, 0],
              [14, 4, 1, 1, 1, 1, "q", 0, 0]]:
    output = run(lines)
    check("系统安全" not in output and "本次手动载入未完成" in output, "cancel setup returns without loading partial state")

for values in [(0, 1, 0), (1, 0, 1), (2**63-1, 1, 1)]:
    output = run([14, 4, 1, 1, *values, 0, 0])
    check("数据不合法：" in output and "系统安全" not in output, "bad total/claim/allocation rejected")

output = run([14, 4, 1, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0])
check("系统安全，安全序列：P0" in output and "请求获准" in output, "zero-resource vector is data, not menu cancellation")
output = run([14, 4, 1, 1, 0, 2**63-1, 2**63-1, 2, 0, 0, 0])
check("系统安全" in output and "[9223372036854775807]" in output, "maximum total is accepted without overflow")

output = run([1, "manual", 1024, 1, 14, 1, 0, 0, 8,
              13, 1, 1, "scheduled 1024 0 1 1", 0, 8, 5, 7, 0])
check(output.count("running:  1:manual") >= 2 and "平均周转时间：1.000" in output, "banker coexists with manual and CPU scheduling menus")
check("总容量 65536，空闲 65536" in output and "程序结束" in output, "original resources remain releasable")

for lines in [[14], [14, 1], [14, 3], [14, 4], [14, 4, 1], [14, 4, 1, 1],
              [14, 4, 1, 1, 1], [14, 4, 1, 1, 1, 1], [14, 1, 3], [14, 1, 3, 0]]:
    run(lines)
    check(True, "EOF terminates each new read location")


def brute_safe(available, maximum, allocation):
    for order in permutations(range(len(maximum))):
        work = available[:]
        for i in order:
            if any(maximum[i][j] - allocation[i][j] > work[j] for j in range(len(work))):
                break
            work = [work[j] + allocation[i][j] for j in range(len(work))]
        else:
            return True
    return False


generator = random.Random(20261106)
for trial in range(20):
    n, m = generator.randrange(1, 5), generator.randrange(1, 4)
    available = [generator.randrange(3) for _ in range(m)]
    allocation = [[generator.randrange(3) for _ in range(m)] for _ in range(n)]
    total = [available[j] + sum(row[j] for row in allocation) for j in range(m)]
    maximum = [[generator.randrange(allocation[i][j], total[j]+1) for j in range(m)] for i in range(n)]
    pid = generator.randrange(n)
    need = [maximum[pid][j] - allocation[pid][j] for j in range(m)]
    request = [generator.randrange(value+2) for value in need]
    lines = [14, 4, n, m, " ".join(map(str, available))]
    for claim, assigned in zip(maximum, allocation):
        lines += [" ".join(map(str, claim)), " ".join(map(str, assigned))]
    output = run([*lines, 3, pid, " ".join(map(str, request)), 1, 0, 0, 0])
    tables = states(output)
    before = expected_state(available, maximum, allocation)
    check(tables[0] == before and ("系统安全，安全序列：" in output) == brute_safe(available, maximum, allocation),
          "manual safety agrees with exhaustive reference")
    changed = [row[:] for row in allocation]
    after_available = [available[j] - request[j] for j in range(m)]
    for j in range(m):
        changed[pid][j] += request[j]
    if any(request[j] > need[j] for j in range(m)):
        expected_message = "超过该进程剩余需求Need"
        expected = before
    elif any(request[j] > available[j] for j in range(m)):
        expected_message = "请求暂缓"
        expected = before
    elif brute_safe(after_available, maximum, changed):
        expected_message = "请求获准"
        expected = expected_state(after_available, maximum, changed)
    else:
        expected_message = "试分配后不安全"
        expected = before
    check(expected_message in output and tables[1] == tables[2] == expected, "request result and persisted matrices match reference")

print(f"Passed: {passed} Failed: 0; CLI scenarios: {scenarios}; exhaustive model sessions: 20")
