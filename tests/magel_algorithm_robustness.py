"""用独立暴力解验证堆链表、BST、DFS/BFS 与 NOIP2015 运输计划，覆盖 VM/AOT。"""
import argparse
from collections import deque
import os
from pathlib import Path
import random
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/algorithm-robustness"


def structures(n, seed):
    """独立模拟随机序列，以 BFS 核对所有起点的距离校验和。"""
    def draw():
        nonlocal seed
        seed = seed * 48271 % 2147483647
        return seed
    values = [draw() % 1000 for _ in range(n)]
    adjacency = [[] for _ in range(n)]
    for _ in range(n * 3):
        a, b = draw() % n, draw() % n
        adjacency[a].append(b)
    checksum = 0
    for start in range(n):
        distance = [-1] * n
        distance[start] = 0
        queue = deque([start])
        while queue:
            vertex = queue.popleft()
            for target in adjacency[vertex]:
                if distance[target] < 0:
                    distance[target] = distance[vertex] + 1
                    queue.append(target)
        checksum += sum((d + 1) * (start + 1) * (i + 1) for i, d in enumerate(distance))
    return f"{sum(values)} {checksum} {n} {len(set(values))}"


def transport(n, edges, queries):
    """枚举被清零的边，对每条查询独立遍历求路径，避免复用待测算法。"""
    adjacency = [[] for _ in range(n)]
    for index, (a, b, weight) in enumerate(edges):
        adjacency[a - 1].append((b - 1, weight, index))
        adjacency[b - 1].append((a - 1, weight, index))
    paths = []
    for a, b in queries:
        stack = [(a - 1, -1, 0, set())]
        while stack:
            vertex, parent, distance, used = stack.pop()
            if vertex == b - 1:
                paths.append((distance, used))
                break
            for child, weight, index in adjacency[vertex]:
                if child != parent:
                    stack.append((child, vertex, distance + weight, used | {index}))
    return min(max((length - (weight if index in used else 0) for length, used in paths), default=0)
               for index, (_, _, weight) in enumerate(edges)) if edges else 0


def encode(n, edges, queries):
    """将树和查询编码成题目输入。"""
    return "\n".join([f"{n} {len(queries)}", *(" ".join(map(str, edge)) for edge in edges),
                      *(f"{a} {b}" for a, b in queries)]) + "\n"


def main():
    """同一组输入分别运行 VM 和 AOT，并在首次不一致时保留输入。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--magel", type=Path, default=ROOT / "magel.exe")
    parser.add_argument("--random-cases", type=int, default=40)
    parser.add_argument("--stress-size", type=int, default=2000)
    parser.add_argument("--skip-aot", action="store_true")
    parser.add_argument("--mcu-host", type=Path, help="额外对照板端宿主（使用相同字节码及输入）")
    args = parser.parse_args()
    compiler = args.magel.resolve()
    work = WORK / compiler.stem
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, MAGEL_LANG="zh")
    env.pop("MAGEL_NO_STDIN_INPUT", None)

    def execute(command, data="", timeout=240):
        result = subprocess.run(list(map(str, command)), input=data, cwd=ROOT, env=env,
                                capture_output=True, encoding="utf-8", errors="replace", timeout=timeout)
        assert result.returncode == 0, (command, result.returncode, result.stdout, result.stderr)
        if args.mcu_host and Path(command[0]).resolve() == args.mcu_host.resolve():
            assert all(not line.strip() or line.startswith("mcu-vm：") for line in result.stderr.splitlines()), (command, result.stderr)
        elif len(command)>1 and command[1]=="svmb":
            assert all(not line.strip() or line.startswith("magel：不带调试节") for line in result.stderr.splitlines()), (command, result.stderr)
        else:
            assert not result.stderr.strip(), (command, result.stderr)
        return result.stdout.strip()

    programs = {}
    for name in ("borrow_algorithms", "noip2015_transport", "idempotent_destruction"):
        source = ROOT / "tests/fixtures" / (name + ".mgl")
        programs[name] = [[compiler, "run", source, "--out", work / name]]
        if not args.skip_aot:
            binary = work / (name + (".exe" if os.name == "nt" else "-aot"))
            execute([compiler, "aot", source, "--out", binary, "--opt-level=1"], timeout=600)
            programs[name].append([binary])
        if args.mcu_host:
            prefix = work / (name + "-mcu")
            execute([compiler, "svmb", source, "--no-reflection", "out", prefix])
            programs[name].append([args.mcu_host.resolve(), Path(str(prefix)+".svmb"),
                                   Path(str(prefix)+".data.bin"), Path(str(prefix)+".layout")])
        print(f"已准备 {name}", flush=True)

    count = 0
    def verify(name, data, expected, commands=None):
        nonlocal count
        (work / "last-input.txt").write_text(data, encoding="utf-8")
        for command in commands or programs[name]:
            actual = execute(command, data, timeout=900)
            assert actual == str(expected), (name, command, data[:2000], expected, actual)
            count += 1

    verify("idempotent_destruction", "", "idempotent destruction ok 7")
    for n, seed in [(0, 1), (1, 1), (2, 42), (12, 42), (32, 0), (64, 7), (128, 2026)]:
        verify("borrow_algorithms", f"{n} {seed}\n", structures(n, seed))
    sample = [(1, 2, 3), (1, 6, 4), (3, 1, 7), (4, 3, 6), (3, 5, 5)]
    verify("noip2015_transport", encode(6, sample, [(3, 6), (2, 5), (4, 5)]), 11)
    rng = random.Random(201512)
    for case in range(args.random_cases):
        n = rng.randint(2, 18)
        edges = [(vertex, (vertex - 1 if case % 4 == 0 else 1 if case % 4 == 1 else rng.randrange(1, vertex)),
                  (0 if case % 7 == 0 else 1000 if case % 7 == 1 else rng.randrange(101)))
                 for vertex in range(2, n + 1)]
        queries = [(rng.randrange(1, n + 1), rng.randrange(1, n + 1)) for _ in range(rng.randint(1, 25))]
        queries += [(1, n), (n, 1), (1, 1)]
        verify("noip2015_transport", encode(n, edges, queries), transport(n, edges, queries))
        if (case + 1) % 10 == 0:
            print(f"随机暴力对照 {case + 1}/{args.random_cases}", flush=True)
    if args.stress_size >= 2:
        n = args.stress_size
        edges = [(v - 1, v, 1000) for v in range(2, n + 1)]
        queries = [(1, n)] * n
        # 完整深链只删除一条边；已知答案无需大规模暴力求解。
        commands = programs["noip2015_transport"][-1:] if n > 10000 else None
        verify("noip2015_transport", encode(n, edges, queries), (n - 2) * 1000, commands)
        print(f"深链压力 n=m={n} 通过", flush=True)
    print(f"算法鲁棒性：{count} 次运行全部通过", flush=True)


if __name__ == "__main__":
    main()
