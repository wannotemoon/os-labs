"""真实可执行程序的输入/输出回归；只在新建测试目录中创建虚盘。"""
import pathlib
import re
import subprocess
import sys
import tempfile


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    parent = pathlib.Path(sys.argv[2]).resolve()
    parent.mkdir(parents=True, exist_ok=True)
    folder = pathlib.Path(tempfile.mkdtemp(prefix="cli-", dir=parent))
    passed = 0

    def check(condition, description):
        nonlocal passed
        if not condition:
            raise AssertionError(description)
        passed += 1

    def run(lines):
        result = subprocess.run(
            [str(executable)], input="\n".join(lines).encode("utf-8"),
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=folder, timeout=30,
        )
        check(result.returncode == 0, f"unexpected exit: {result.returncode}, {result.stderr!r}")
        return result.stdout.decode("utf-8", errors="strict").replace("\r\n", "\n")

    disk = folder / "lecture.disk"
    output = run([
        "12", str(disk), "8", "md test", "mK a.c 2000", "DiR", "cD test", "CD",
        "mk zero 0", "cd ..", "rd test", "cd test", "del zero", "cd \\", "rd test",
        "info 0", "exit", "8", "0", "",
    ])
    check(output.count("错误：") == 1 and "directory is not empty" in output, "lecture error semantics")
    check("<DIR>" in output and re.search(r"2000\s+a\.c", output), "lecture DIR")
    check("\\test\\" in output, "CD shows path")
    check("00000000  FF FF 00 00 03 00 FF FF" in output, "info shows expected lecture FAT")
    check("创建成功" not in output and "删除成功" not in output, "filesystem success stays quiet")
    check("总容量 65536，空闲 65536" in output and "程序结束" in output, "EXIT returns to original menu")
    check(disk.stat().st_size == 8208, "lecture exact disk length")
    before = disk.read_bytes()
    output = run(["12", str(disk), "8", "DIR", "EXIT", "0", ""])
    check("<DIR>" not in output and re.search(r"2000\s+a\.c", output), "process restart preserves active entries")
    check(disk.read_bytes() == before, "restart does not reformat")

    syntax = folder / "syntax.disk"
    commands = [
        "MD MiXeD", "cD MiXeD", "mK a 0", "CD \\",
        "RD MiXeD", "CD mixed", "mk a 12x", "mk a 2147483648", "mk a -1",
        "mk a 1.2", "mk a 1 extra", "md", "rd", "del", "dir extra",
        "info", "info -1", "info 8208", "info 2147483648", "info 0 2", "exit extra",
        "CD a/b", "mk a b 0", "unknown", "", "   ",
        "CD MiXeD", "DEL a", "CD ..", "RD MiXeD", "MK Ab 0", "MK ab 0", "DIR", "EXIT",
    ]
    output = run(["12", str(syntax), "8", *commands, "0", ""])
    check(output.count("错误：") == 20, "every malformed command rejects once and recovers")
    check(re.search(r"\sAb\n", output) and re.search(r"\sab\n", output), "names preserve case")
    check("MiXeD" in output and "程序结束" in output, "mixed-case command keywords work")

    for index, value in enumerate(["x", "7", "0", "1025", "64 extra", "2147483647", "-1", "9" * 60]):
        target = folder / f"bad-capacity-{index}.disk"
        output = run(["12", str(target), value, "8", "0", ""])
        check("错误" in output and not target.exists(), f"invalid capacity rejected before creation: {value}")
        check("总容量 65536，空闲 65536" in output, "startup error returns to original menu")

    output = run(["12", str(disk), "64", "0", ""])
    check("disk file size mismatch" in output and disk.read_bytes() == before, "capacity mismatch preserves disk")
    output = run(["12", str(folder), "8", "0", ""])
    check("not a regular file" in output, "directory path rejected")
    output = run(["12", str(folder / "absent" / "disk"), "8", "0", ""])
    check("错误" in output and not (folder / "absent").exists(), "missing parent rejected without creation")

    # 每个读输入的位置遇到EOF都应正常返回，不能死循环。
    for index, lines in enumerate([
        [], ["12", ""], ["12", str(folder / "eof-cap.disk"), ""],
        ["12", str(folder / "eof-shell.disk"), "8", ""],
        ["12", str(folder / "eof-command.disk"), "8", "MK"],
        ["12", str(folder / "eof-dir.disk"), "8", "MD d", "CD d"],
    ]):
        run(lines)
        check(True, f"EOF case {index}")

    output = run(["12", "", "", "MK saved 0", "EXIT", "12", "", "", "DIR", "EXIT", "0", ""])
    check((folder / "os-lab3.disk").stat().st_size == 65664, "default path/default capacity")
    check(re.search(r"\ssaved\n", output), "default disk reopens in same program")

    output = run([
        "1", "proc", "5000", "3", "9", "3072", "Y", "12", str(folder / "coexist.disk"),
        "8", "MD test", "MK a.c 2000", "EXIT", "8", "9", "0", "N", "5", "8", "0", "",
    ])
    check("进程创建成功" in output and "逻辑地址 3072 对应的物理地址" in output, "paging still works before filesystem")
    check("逻辑地址 0 对应的物理地址" in output and "当前进程已终止" in output, "paging still works after filesystem")
    check("总容量 65536，空闲 65536" in output and "总容量 131072，空闲 131072" in output, "process resources reclaimed")
    print(f"Passed: {passed} Failed: 0")


if __name__ == "__main__":
    main()
