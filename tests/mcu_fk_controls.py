"""用模拟 BSP 编译真实板端桥接层，覆盖实体及串口退出和菜单按键隔离。"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / 'example/mcu/FK-STM32H743XIH6'

STUB = r"""
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef enum {KEY_MODE,KEY_ENTER,KEY_UP,KEY_DOWN} KeyEnum_T;
typedef enum {KEY_EVENT_CLICK=1,KEY_EVENT_DOUBLE_CLICK=2,KEY_EVENT_LONG_PRESS=4,KEY_EVENT_REPEAT=8,KEY_EVENT_ALL=255} KeyEvent_T;
#define KEY_VOLATGE_PRESSED 1
#define COM1 1
#define MP_RAM_EX 0
typedef struct {int unused;} FIFO_T;
typedef struct {FIFO_T RxFIFO;} UartBlock_T;
typedef struct {uint16_t Width,Height;} Lcd_T;
typedef struct {uint32_t CYCCNT;} Dwt_T;
extern Lcd_T g_LCD;
extern Dwt_T testDwt;
#define DWT (&testDwt)
extern uint32_t SystemCoreClock;
void UART_SendArrayImm(unsigned port,uint8_t *bytes,uint16_t count);
UartBlock_T *UART_FindBlockByPort(unsigned port);
unsigned FIFO_ReadBuffer(FIFO_T *fifo,void *buffer,unsigned count);
void LCD_Fill(unsigned x,unsigned y,unsigned w,unsigned h,unsigned color);
void LCD_ShowChar(unsigned x,unsigned y,unsigned ch,unsigned fg,unsigned bg);
int Key_GetVoltage(KeyEnum_T key);
int Key_IsClicked(KeyEnum_T key);
int Key_HasEvent(KeyEnum_T key,KeyEvent_T mask);
void Key_ClearEvent(KeyEnum_T key,KeyEvent_T mask);
void Key_ClearAll(void);
void Key_SetEvent(KeyEnum_T key,KeyEvent_T event);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t value);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t milliseconds);
void *Memory_Malloc(unsigned pool,uint32_t bytes);
#define __DMB() ((void)0)
#define __DSB() ((void)0)
#define SCB_CleanDCache_by_Addr(ptr,bytes) ((void)(ptr),(void)(bytes))
"""

DRIVER = r"""
#include "bsp_system.h"
#include "board_io.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
Lcd_T g_LCD={320,240};
Dwt_T testDwt;
uint32_t SystemCoreClock=480000000;
static uint32_t tick,irq;
static unsigned down[4],events[4],rxStart,rxEnd;
static unsigned char rx[4096];
static UartBlock_T port;
static char output[4096];
static unsigned outputLength;
void UART_SendArrayImm(unsigned p,uint8_t *s,uint16_t n){assert(p==1);assert(outputLength+n<sizeof(output));memcpy(output+outputLength,s,n);outputLength+=n;output[outputLength]=0;}
UartBlock_T *UART_FindBlockByPort(unsigned p){return p==1?&port:NULL;}
unsigned FIFO_ReadBuffer(FIFO_T *f,void *buffer,unsigned n){(void)f;unsigned count=rxEnd-rxStart;if(count>n)count=n;memcpy(buffer,rx+rxStart,count);rxStart+=count;return count;}
void LCD_Fill(unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){(void)a;(void)b;(void)c;(void)d;(void)e;}
void LCD_ShowChar(unsigned a,unsigned b,unsigned c,unsigned d,unsigned e){(void)a;(void)b;(void)c;(void)d;(void)e;}
int Key_GetVoltage(KeyEnum_T key){return down[key];}
int Key_IsClicked(KeyEnum_T key){int found=events[key]&1;events[key]&=~1u;return found!=0;}
int Key_HasEvent(KeyEnum_T key,KeyEvent_T mask){int found=events[key]&mask;events[key]&=~mask;return found!=0;}
void Key_ClearEvent(KeyEnum_T key,KeyEvent_T mask){events[key]&=~mask;}
void Key_ClearAll(void){memset(events,0,sizeof(events));}
void Key_SetEvent(KeyEnum_T key,KeyEvent_T event){events[key]|=event;}
uint32_t __get_PRIMASK(void){return irq;}
void __disable_irq(void){irq=1;}
void __set_PRIMASK(uint32_t value){irq=value;}
uint32_t HAL_GetTick(void){return tick;}
void HAL_Delay(uint32_t ms){tick+=ms;}
void *Memory_Malloc(unsigned pool,uint32_t bytes){(void)pool;return malloc(bytes);}
static void inject(const char *bytes){size_t n=strlen(bytes);assert(rxEnd+n<sizeof(rx));memcpy(rx+rxEnd,bytes,n);rxEnd+=(unsigned)n;}
int main(void){
    FkResetKeys();
    assert(!FkExitRequested());
    for(unsigned key=0;key<2;++key){
        down[key]=1;assert(!FkExitRequested());tick+=29;assert(!FkExitRequested());tick+=1;assert(FkExitRequested());
        FkResetKeys();assert(!FkMenuReady());tick+=100;assert(!FkMenuReady());down[key]=0;
        assert(!FkMenuReady());events[key]=KEY_EVENT_CLICK;tick+=30;assert(FkMenuReady());assert(!events[key]);assert(!FkKeyClicked(key));
    }
    down[0]=1;assert(!FkExitRequested());tick+=10;down[0]=0;assert(!FkExitRequested());tick+=50;assert(!FkExitRequested());
    tick=0xfffffff0u;down[0]=1;assert(!FkExitRequested());tick+=30;assert(FkExitRequested());down[0]=0;FkResetKeys();
    const char *names[]={"mode","enter"};const char *modes[]={"click","long","double","repeat"};
    for(unsigned key=0;key<2;++key)for(unsigned mode=0;mode<4;++mode){
        char line[64];snprintf(line,sizeof(line),"key %s %s\r\n",names[key],modes[mode]);
        for(unsigned i=0;line[i];++i){char chunk[2]={line[i],0};inject(chunk);FkPollCommands();}
        assert(FkExitRequested());assert(!FkExitRequested());assert(irq==0);
    }
    inject("key up\nkey down\n");FkPollCommands();assert(!FkExitRequested());assert(FkKeyClicked(2)&&FkKeyClicked(3));
    unsigned char input[128];inject("123\r\nhello\nKEY MODE\n");unsigned n=FkSerialRead(input,sizeof(input));assert(n==11&&!memcmp(input,"123\r\nhello\n",11));assert(FkExitRequested());
    inject("key mode invalid\nkey nowhere\nkey mode click extra\n");FkPollCommands();assert(!FkExitRequested());assert(strstr(output,"KEY_ERROR"));
    inject("key mode ");for(unsigned i=0;i<200;++i)inject("x");inject("\n");FkPollCommands();assert(!FkExitRequested());
    inject("key enter\n");FkDelay(10);assert(FkExitRequested());
    irq=1;Key_SetEvent(KEY_MODE,KEY_EVENT_CLICK);assert(FkExitRequested());assert(irq==1);FkResetKeys();assert(irq==1);
    puts("FK controls: physical debounce, bounce, clock wrap, eight virtual exits, serial input, invalid commands, menu release barrier passed.");
}
"""


def main():
    """只替换硬件接口，执行真实 board_io.c 的命令分流及退出逻辑。"""
    with tempfile.TemporaryDirectory(prefix='fk-controls-') as directory:
        work = Path(directory)
        (work / 'bsp_system.h').write_text(STUB, encoding='utf-8')
        (work / 'test.c').write_text(DRIVER, encoding='utf-8')
        executable = work / ('controls.exe' if os.name == 'nt' else 'controls')
        subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', str(work),
                        '-I', str(BOARD), str(BOARD / 'board_io.c'), str(work / 'test.c'),
                        '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
    check_benchmark()


def check_benchmark():
    """执行实际 Magel 测速程序，确认退出轮询、方向键和超时均能正常结束。"""
    source = (BOARD / 'programs/benchmark.mgl').read_text(encoding='utf-8')
    source = source.replace('%include "board.mgl"', '').replace('int64 main()', 'int64 benchmarkMain()')
    mocks = """
int64 elapsed = 0;
int64 polls = 0;
int64 exitAfter = 0;
int64 records = 0;
void lcdClear(int64 color) {}
void lcdFill(int64 x,int64 y,int64 w,int64 h,int64 color) {}
void lcdText(int64 x,int64 y,String text,int64 fg,int64 bg) {}
int64 lcdWidth() { return 320; }
int64 boardClockHz() { return 480000000; }
int64 boardCycles() { return elapsed; }
int64 nativeIntegerLoop(int64 count,int64 seed) { return integerLoop(count,seed); }
void benchRecord(int64 sample,int64 count,int64 vm,int64 native,int64 checksum) {
    assert(count==10000 && checksum==1254053515); records+=1;
}
bool boardExitRequested() { polls+=1; return exitAfter>0 && polls>=exitAfter; }
bool keyClicked(int64 key) { return key==2; }
bool keyPressed(int64 key) { return false; }
void boardDelay(int64 delay) { elapsed+=delay; }
int64 main() {
    exitAfter=2; assert(benchmarkMain()==0 && records==5 && elapsed==10 && polls==2);
    elapsed=0;polls=0;records=0;exitAfter=0;
    assert(benchmarkMain()==0 && records==5 && elapsed==30000 && polls==3000);
    println("BENCH_EXIT_REGRESSION_OK");return 0;
}
"""
    with tempfile.TemporaryDirectory(prefix='fk-benchmark-') as directory:
        work = Path(directory)
        program = work / 'benchmark-test.mgl'
        program.write_text(source + mocks, encoding='utf-8')
        compiler = ROOT / ('magel.exe' if os.name == 'nt' else 'magel')
        result = subprocess.run([str(compiler), 'run', str(program), '--out', str(work / 'out')],
                                cwd=ROOT, capture_output=True, text=True, encoding='utf-8',
                                errors='replace', timeout=120)
        assert result.returncode == 0, (result.stdout[-2000:], result.stderr)
        assert result.stdout.count('BENCH_EXIT key') == 1, result.stdout[-2000:]
        assert 'BENCH_EXIT_REGRESSION_OK' in result.stdout, result.stdout[-2000:]
        print('实际 benchmark.mgl：提前退出、方向键演示及三十秒自动返回通过。')


if __name__ == '__main__':
    main()
