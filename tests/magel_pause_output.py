"""在真实伪终端中验证解释执行和 AOT 的输出先于 pause 提示。"""
from pathlib import Path
import ctypes
import os
import re
import subprocess
import sys
import tempfile
import threading
import time

ROOT=Path(__file__).resolve().parents[1]
COMPILER=ROOT/('magel.exe' if os.name=='nt' else 'magel')
PROMPT='按回车继续'.encode()


def verify_output(data):
    """回车前必须已显示正文，且正文在提示之前、没有重复输出。"""
    text=data.decode('utf-8',errors='replace')
    text=re.sub(r'\x1b\](?:[^\x07\x1b]|\x1b(?!\\))*(?:\x07|\x1b\\)','',text)
    text=re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',text)
    assert '1 1' in text and '按回车继续' in text,text
    assert text.index('1 1')<text.index('按回车继续'),text
    assert text.count('1 1')==1,text


def windows_terminal(command,cwd):
    """通过 ConPTY 提供真正的终端输入，在发送回车前核对可见输出。"""
    from ctypes import wintypes as w
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    # 测试进程脱离继承的 PowerShell 控制台，子进程只连接本次 ConPTY。
    kernel.FreeConsole()
    handle=w.HANDLE
    class Coord(ctypes.Structure):
        _fields_=[('x',ctypes.c_short),('y',ctypes.c_short)]
    class Startup(ctypes.Structure):
        _fields_=[('cb',w.DWORD),('reserved',w.LPWSTR),('desktop',w.LPWSTR),('title',w.LPWSTR),
            ('x',w.DWORD),('y',w.DWORD),('width',w.DWORD),('height',w.DWORD),
            ('charsX',w.DWORD),('charsY',w.DWORD),('fill',w.DWORD),('flags',w.DWORD),
            ('show',w.WORD),('reservedSize',w.WORD),('reservedBytes',ctypes.c_void_p),
            ('input',handle),('output',handle),('error',handle)]
    class StartupEx(ctypes.Structure):
        _fields_=[('startup',Startup),('attributes',ctypes.c_void_p)]
    class Process(ctypes.Structure):
        _fields_=[('process',handle),('thread',handle),('processId',w.DWORD),('threadId',w.DWORD)]
    kernel.CreatePipe.argtypes=[ctypes.POINTER(handle),ctypes.POINTER(handle),ctypes.c_void_p,w.DWORD]
    kernel.CreatePseudoConsole.argtypes=[Coord,handle,handle,w.DWORD,ctypes.POINTER(handle)]
    kernel.CreatePseudoConsole.restype=ctypes.c_long
    kernel.InitializeProcThreadAttributeList.argtypes=[ctypes.c_void_p,w.DWORD,w.DWORD,ctypes.POINTER(ctypes.c_size_t)]
    kernel.UpdateProcThreadAttribute.argtypes=[ctypes.c_void_p,w.DWORD,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_void_p]
    kernel.CreateProcessW.argtypes=[w.LPCWSTR,w.LPWSTR,ctypes.c_void_p,ctypes.c_void_p,w.BOOL,w.DWORD,ctypes.c_void_p,w.LPCWSTR,ctypes.c_void_p,ctypes.POINTER(Process)]
    kernel.ReadFile.argtypes=[handle,ctypes.c_void_p,w.DWORD,ctypes.POINTER(w.DWORD),ctypes.c_void_p]
    kernel.WriteFile.argtypes=kernel.ReadFile.argtypes
    kernel.CloseHandle.argtypes=[handle]
    kernel.ClosePseudoConsole.argtypes=[handle]
    kernel.WaitForSingleObject.argtypes=[handle,w.DWORD]
    kernel.GetExitCodeProcess.argtypes=[handle,ctypes.POINTER(w.DWORD)]
    kernel.TerminateProcess.argtypes=[handle,w.UINT]
    kernel.DeleteProcThreadAttributeList.argtypes=[ctypes.c_void_p]
    incoming,outgoing=handle(),handle();reading,writing=handle(),handle();console=handle();process=Process()
    attributes=None;captured=bytearray();reader=None
    def checked(result):
        if not result:raise ctypes.WinError(ctypes.get_last_error())
    try:
        checked(kernel.CreatePipe(ctypes.byref(incoming),ctypes.byref(outgoing),None,0))
        checked(kernel.CreatePipe(ctypes.byref(reading),ctypes.byref(writing),None,0))
        status=kernel.CreatePseudoConsole(Coord(120,25),incoming,writing,0,ctypes.byref(console))
        assert status==0,hex(status & 0xffffffff)
        kernel.CloseHandle(incoming);incoming=handle();kernel.CloseHandle(writing);writing=handle()
        size=ctypes.c_size_t()
        kernel.InitializeProcThreadAttributeList(None,1,0,ctypes.byref(size))
        attributes=ctypes.create_string_buffer(size.value)
        checked(kernel.InitializeProcThreadAttributeList(attributes,1,0,ctypes.byref(size)))
        checked(kernel.UpdateProcThreadAttribute(attributes,0,0x00020016,console,size_of_handle:=ctypes.sizeof(handle),None,None))
        startup=StartupEx();startup.startup.cb=ctypes.sizeof(startup);startup.attributes=ctypes.addressof(attributes)
        startup.startup.flags=0x00000100
        # 显式打开伪控制台的标准句柄，避免父测试进程的重定向句柄被 CRT 继承。
        bridge=('import ctypes,subprocess,sys; '
                'ctypes.windll.kernel32.SetConsoleOutputCP(65001); '
                'ctypes.windll.kernel32.SetConsoleCP(65001); '
                'si=open("CONIN$","rb",buffering=0); '
                'so=open("CONOUT$","wb",buffering=0); '
                'sys.exit(subprocess.call(sys.argv[1:],stdin=si,stdout=so,stderr=so))')
        line=ctypes.create_unicode_buffer(subprocess.list2cmdline([sys.executable,'-c',bridge,*[str(x) for x in command]]))
        checked(kernel.CreateProcessW(None,line,None,None,False,0x00080000,None,str(cwd),ctypes.byref(startup),ctypes.byref(process)))
        def read():
            buffer=ctypes.create_string_buffer(4096);count=w.DWORD()
            while kernel.ReadFile(reading,buffer,len(buffer),ctypes.byref(count),None) and count.value:
                captured.extend(buffer.raw[:count.value])
        reader=threading.Thread(target=read,daemon=True);reader.start()
        deadline=time.monotonic()+30
        while PROMPT not in captured and time.monotonic()<deadline:time.sleep(0.02)
        verify_output(bytes(captured))
        count=w.DWORD();checked(kernel.WriteFile(outgoing,b'\r',1,ctypes.byref(count),None))
        assert kernel.WaitForSingleObject(process.process,10000)==0,bytes(captured)
        code=w.DWORD();checked(kernel.GetExitCodeProcess(process.process,ctypes.byref(code)))
        assert code.value==0,bytes(captured)
    finally:
        if process.process:
            if kernel.WaitForSingleObject(process.process,0)!=0:kernel.TerminateProcess(process.process,1)
            kernel.CloseHandle(process.process)
        if process.thread:kernel.CloseHandle(process.thread)
        if console:kernel.ClosePseudoConsole(console)
        if attributes is not None:kernel.DeleteProcThreadAttributeList(attributes)
        for item in (incoming,outgoing,reading,writing):
            if item:kernel.CloseHandle(item)
        if reader:reader.join(timeout=2)


def unix_terminal(command,cwd):
    """通过 POSIX PTY 验证同样的输入前输出顺序。"""
    import pty
    import select
    master,slave=pty.openpty()
    process=subprocess.Popen(command,cwd=cwd,stdin=slave,stdout=slave,stderr=slave)
    os.close(slave);captured=bytearray()
    try:
        deadline=time.monotonic()+30
        while PROMPT not in captured and time.monotonic()<deadline:
            if select.select([master],[],[],0.1)[0]:captured.extend(os.read(master,4096))
        verify_output(bytes(captured));os.write(master,b'\n')
        assert process.wait(timeout=10)==0,bytes(captured)
    finally:
        if process.poll() is None:process.kill();process.wait()
        os.close(master)


def main():
    """编译用户复现代码，两个后端都必须在等待回车前输出 1 1。"""
    terminal=windows_terminal if os.name=='nt' else unix_terminal
    with tempfile.TemporaryDirectory(prefix='magel-pause-') as directory:
        folder=Path(directory);source=folder/'main.mgl'
        source.write_text('int64 main(){String a="1";int64 count=len(a);println("{} {}",a,count);pause();return 0;}',encoding='utf-8')
        terminal([COMPILER,'run',source],folder)
        executable=folder/('pause.exe' if os.name=='nt' else 'pause')
        build=subprocess.run([str(COMPILER),'aot',str(source),'out',str(executable),'opt','0'],capture_output=True,timeout=180)
        assert build.returncode==0,build.stderr.decode('utf-8',errors='replace')
        terminal([executable],folder)
    print('pause 真终端回归通过：解释执行与 AOT 的正文均先于提示，回车后正常退出。')


if __name__=='__main__':main()
