<#
在同一台 Windows 机器上构建、验证并比较 MAGEL StackVM：
MSVC Win32、MSVC x64、GCC x64。默认预热一次、正式运行七次。

从仓库根目录运行：
  ./magel/benchmarks/compare-vm.ps1
  ./magel/benchmarks/compare-vm.ps1 -ExpectedCpu '9700X'

结果写入 tmp/vm-bitness/summary.md 和 results.json；不会删除现有文件。
#>
param(
    [ValidateRange(1, 100)][int]$Repeats = 7,
    [string]$OutputDirectory = 'tmp/vm-bitness',
    [string]$ExpectedCpu = '',
    [string]$ProgramPrefix = '',
    [string]$CMakePath = '',
    [string]$GccPath = '',
    [string]$MakePath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# VM 与单元测试输出的是 UTF-8 中文（"全部通过（N 项断言）"）。控制台代码页不是 UTF-8
# 时（中文 Windows 默认 GBK），Windows PowerShell 会按本地代码页解码原生输出，校验串就
# 匹配不上；先把本进程的捕获编码统一到 UTF-8（已经是 UTF-8 时是空操作）。
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$previous = Get-Location

function Find-Executable([string]$Name, [string]$Override) {
    if ($Override) {
        if (-not (Test-Path -LiteralPath $Override -PathType Leaf)) {
            throw "找不到 $Name：$Override"
        }
        return (Resolve-Path -LiteralPath $Override).Path
    }
    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    if ($Name -eq 'cmake') {
        foreach ($edition in @('Community', 'Enterprise', 'Professional', 'BuildTools')) {
            $candidate = "C:\Program Files\Microsoft Visual Studio\2022\$edition\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        }
    }
    throw "找不到 $Name；请用对应的 Path 参数指定工具位置"
}

function Invoke-Checked([string]$File, [string[]]$Arguments) {
    $output = & $File @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "$File 退出码 $LASTEXITCODE：$($output | Out-String)"
    }
    return ($output | Out-String).Trim()
}

function Get-PeMachine([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $header = [BitConverter]::ToInt32($bytes, 0x3c)
    return [BitConverter]::ToUInt16($bytes, $header + 4)
}

function Parse-Benchmark([string]$Output) {
    $timing = [regex]::Match($Output, 'steps=(\d+) median_ns=(\d+)')
    $rate = [regex]::Match($Output, 'throughput_mips=([0-9.]+)')
    $checksum = [regex]::Match($Output, 'checksum=([^\r\n]+)')
    if (-not $timing.Success -or -not $rate.Success -or -not $checksum.Success) {
        throw "无法读取基准结果：$Output"
    }
    return [pscustomobject]@{
        steps = [uint64]$timing.Groups[1].Value
        median_ns = [uint64]$timing.Groups[2].Value
        median_ms = [math]::Round(([double]$timing.Groups[2].Value / 1000000.0), 3)
        mips = [double]::Parse($rate.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
        checksum = $checksum.Groups[1].Value.Trim()
        pool_peak_bytes = [uint64]([regex]::Match($Output, 'runtime_pool_peak_bytes=(\d+)').Groups[1].Value)
    }
}

try {
    Set-Location -LiteralPath $root
    $out = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
        [IO.Path]::GetFullPath($OutputDirectory)
    } else {
        [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
    }
    New-Item -ItemType Directory -Path $out -Force | Out-Null

    try { $cpu = (Get-CimInstance Win32_Processor -ErrorAction Stop | Select-Object -First 1).Name.Trim() }
    catch { $cpu = (Get-ItemProperty 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim() }
    if ($ExpectedCpu -and $cpu -notmatch [regex]::Escape($ExpectedCpu)) {
        throw "检测到 CPU：$cpu；与期望的 $ExpectedCpu 不符"
    }
    try { $os = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop | Select-Object -First 1 }
    catch { $os = [pscustomobject]@{ Caption='Windows'; Version=[Environment]::OSVersion.Version.ToString() } }
    $cmake = Find-Executable 'cmake' $CMakePath
    $gcc = Find-Executable 'g++' $GccPath
    $make = Find-Executable 'make' $MakePath
    $gccTarget = Invoke-Checked $gcc @('-dumpmachine')
    if ($gccTarget -notmatch 'x86_64') {
        throw "GCC 必须为 64 位工具链，实际目标：$gccTarget"
    }
    $gccVersion = Invoke-Checked $gcc @('-dumpfullversion', '-dumpversion')

    Write-Host "CPU: $cpu"
    Write-Host '构建 MSVC Win32 / x64 与 GCC x64 VM...'
    $x86Dir = Join-Path $out 'win32'
    $x64Dir = Join-Path $out 'x64'
    $gccDir = Join-Path $out 'gcc64'
    $allocatorFlags = ''
    Invoke-Checked $cmake @('-S', 'magel/library/stackvm', '-B', $x86Dir,
                            '-G', 'Visual Studio 17 2022', '-A', 'Win32', "-DCMAKE_CXX_FLAGS=$allocatorFlags") | Out-Null
    Invoke-Checked $cmake @('-S', 'magel/library/stackvm', '-B', $x64Dir,
                            '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_CXX_FLAGS=$allocatorFlags") | Out-Null
    Invoke-Checked $cmake @('-S', 'magel/library/stackvm', '-B', $gccDir,
                            '-G', 'MinGW Makefiles', "-DCMAKE_CXX_COMPILER=$gcc",
                            "-DCMAKE_MAKE_PROGRAM=$make", '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_CXX_FLAGS=$allocatorFlags") | Out-Null
    foreach ($dir in @($x86Dir, $x64Dir)) {
        Invoke-Checked $cmake @('--build', $dir, '--config', 'Release', '--parallel', '4') | Out-Null
    }
    Invoke-Checked $cmake @('--build', $gccDir, '--parallel', '4') | Out-Null

    $compilerFile = Get-ChildItem -LiteralPath (Join-Path $x86Dir 'CMakeFiles') -Recurse -Filter CMakeCXXCompiler.cmake | Select-Object -First 1
    $compilerInfo = Get-Content -Raw -LiteralPath $compilerFile.FullName
    $msvcVersion = [regex]::Match($compilerInfo, 'CMAKE_CXX_COMPILER_VERSION "([^"]+)"').Groups[1].Value

    $binaries = @(
        [pscustomobject]@{ name='MSVC Win32'; dir=$x86Dir; machine=0x014c; compiler="MSVC $msvcVersion" },
        [pscustomobject]@{ name='MSVC x64'; dir=$x64Dir; machine=0x8664; compiler="MSVC $msvcVersion" },
        [pscustomobject]@{ name='GCC x64'; dir=$gccDir; machine=0x8664; compiler="GCC $gccVersion" }
    )
    foreach ($build in $binaries) {
        $release = if ($build.name -like 'MSVC*') { 'Release' } else { '' }
        $build | Add-Member -NotePropertyName bin -NotePropertyValue (Join-Path $build.dir $release)
        $vm = Join-Path $build.bin 'magel-stackvm.exe'
        if ((Get-PeMachine $vm) -ne $build.machine) {
            throw "$($build.name) PE 位数与预期不符：$vm"
        }
    }

    $parser = Join-Path $root 'magel/magel.exe'
    if (-not (Test-Path -LiteralPath $parser -PathType Leaf)) {
        Write-Host '尚无 MAGEL 编译器，先构建根目录工程...'
        & (Join-Path $root 'build.ps1') -NoRust -NoExamples -NoProjects
        if ($LASTEXITCODE -ne 0) { throw 'MAGEL 编译器构建失败' }
    }
    $prefix = if ($ProgramPrefix) { [IO.Path]::GetFullPath($ProgramPrefix) } else { Join-Path $out 'magel-loop' }
    if (-not $ProgramPrefix) {
        Invoke-Checked $parser @('magel/benchmarks/loop.mgl', '--emit', '--svmb', '--out', $prefix) | Out-Null
    }
    $program = "$prefix.svmb"
    $data = "$prefix.data.bin"
    $layout = "$prefix.layout"

    Write-Host '运行单元测试、同一份 MAGEL 字节码与性能基准...'
    $rows = @()
    $expectedAssertions = 0
    foreach ($build in $binaries) {
        $vm = Join-Path $build.bin 'magel-stackvm.exe'
        $test = Join-Path $build.bin 'magel-stackvm-tests.exe'
        $testOutput = Invoke-Checked $test @('magel/library/stackvm/examples')
        $assertions = [regex]::Match($testOutput, '全部通过（(\d+) 项断言）')
        if (-not $assertions.Success) { throw "$($build.name) 单元测试结果异常：$testOutput" }
        $count = [int]$assertions.Groups[1].Value
        if ($expectedAssertions -eq 0) { $expectedAssertions = $count }
        if ($count -ne $expectedAssertions) {
            throw "$($build.name) 断言数 $count 与其他构建的 $expectedAssertions 不同"
        }
        Invoke-Checked $vm @('run', $program, '--data', $data, '--layout', $layout,
                             '--limit', '100000000') | Out-Null
        foreach ($workload in @('mc', 'vm')) {
            if ($workload -eq 'mc') {
                $exe = Join-Path $build.bin 'magel-stackvm-magel-benchmark.exe'
                $result = Parse-Benchmark (Invoke-Checked $exe @($program, $data, $layout, "$Repeats"))
                if ($result.checksum -ne 'asserted') { throw "$($build.name) MAGEL 校验异常" }
            } else {
                $exe = Join-Path $build.bin 'magel-stackvm-benchmark.exe'
                $result = Parse-Benchmark (Invoke-Checked $exe @('1000000', "$Repeats"))
                if ($result.checksum -ne '499999500000') { throw "$($build.name) VM 校验异常" }
            }
            $rows += [pscustomobject]@{
                build=$build.name; workload=$workload; compiler=$build.compiler
                steps=$result.steps; median_ns=$result.median_ns
                median_ms=$result.median_ms; mips=$result.mips
                checksum=$result.checksum; pool_peak_bytes=$result.pool_peak_bytes
            }
            Write-Host ("{0,-11} {1,-2} {2,9:N3} ms  {3,7:N3} MIPS" -f
                        $build.name, $workload, $result.median_ms, $result.mips)
        }
    }

    $vm32 = Join-Path $x86Dir 'Release/magel-stackvm.exe'
    $vm64 = Join-Path $x64Dir 'Release/magel-stackvm.exe'
    $quad = 'magel/library/stackvm/examples/13_heap.quad'
    $svmb32 = Join-Path $out 'heap-x86.svmb'
    $svmb64 = Join-Path $out 'heap-x64.svmb'
    Invoke-Checked $vm32 @('compile', $quad, '--out', $svmb32) | Out-Null
    Invoke-Checked $vm64 @('compile', $quad, '--out', $svmb64) | Out-Null
    $hash32 = (Get-FileHash -LiteralPath $svmb32 -Algorithm SHA256).Hash
    $hash64 = (Get-FileHash -LiteralPath $svmb64 -Algorithm SHA256).Hash
    if ($hash32 -ne $hash64) { throw "32 / 64 位生成的 .svmb 哈希不同：$hash32 / $hash64" }
    $cross64 = Invoke-Checked $vm64 @('run', $svmb32)
    $cross32 = Invoke-Checked $vm32 @('run', $svmb64)
    if ($cross32 -notmatch '42 7 0' -or $cross64 -notmatch '42 7 0') {
        throw "32 / 64 位 VM 交叉执行结果不同：$cross32 / $cross64"
    }

    $report = [pscustomobject]@{
        timestamp_utc=(Get-Date).ToUniversalTime().ToString('o')
        cpu=$cpu; os="$($os.Caption) $($os.Version)"; repeats=$Repeats
        compiler_msvc="MSVC $msvcVersion"; compiler_gcc="GCC $gccVersion"
        optimization='MSVC Release /O2 /DNDEBUG; GCC Release -O3 -DNDEBUG; C++17'
        allocator='C pool, explicit STL allocators, no global new/delete override'
        pool_capacity_bytes=536870912
        magel_program='magel/benchmarks/loop.mgl'
        svmb_sha256=(Get-FileHash -LiteralPath $program -Algorithm SHA256).Hash
        cross_bitness_svmb_sha256=$hash32
        unit_assertions_each=$expectedAssertions
        rows=$rows
    }
    $jsonPath = Join-Path $out 'results.json'
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

    $md = [Collections.Generic.List[string]]::new()
    $md.add('# MAGEL StackVM 位数与编译器性能对比')
    $md.add('')
    $md.add("CPU：$cpu；系统：$($os.Caption) $($os.Version)。")
    $md.add("编译器：MSVC $msvcVersion、GCC $gccVersion；每项预热一次，正式运行 $Repeats 次取中位数。")
    $md.add('MSVC 使用 Release /O2 /DNDEBUG；GCC 使用 Release -O3 -DNDEBUG。')
    $md.add('分配器：C 内存池；默认容量 512 MiB。')
    $md.add('计时只包含 Machine::run，不含 MAGEL 编译、加载或进程启动。')
    $md.add('')
    $md.add('| 构建 | 负载 | 指令数 | 中位数（ms） | MIPS | 校验 |')
    $md.add('| --- | --- | ---: | ---: | ---: | --- |')
    foreach ($row in $rows) {
        $label = if ($row.workload -eq 'mc') { 'MAGEL 源码 loop.mgl' } else { 'VM 整数循环' }
        $md.add(('| {0} | {1} | {2} | {3:N3} | {4:N3} | {5} |' -f
                 $row.build, $label, $row.steps, $row.median_ms, $row.mips, $row.checksum))
    }
    $md.add('')
    $md.add("三种构建均通过 $expectedAssertions 项断言；x86 / x64 生成的堆示例 .svmb SHA-256 相同：$hash32。")
    $md.add('x86 / x64 交叉运行均输出 42 7 0；同一份 MAGEL 字节码在三种构建下通过断言。')
    $md.add('32 位 Windows 必须运行 Win32 VM；C ABI 动态库须与 VM 进程位数相同。')
    $md.add('结果只对应当前机器、构建选项和工作负载，不代表其他 CPU 的性能。')
    $mdPath = Join-Path $out 'summary.md'
    $md | Set-Content -LiteralPath $mdPath -Encoding UTF8
    Write-Host "完成：$mdPath"
    Write-Host "机器可读结果：$jsonPath"
}
finally {
    Set-Location -LiteralPath $previous
}
