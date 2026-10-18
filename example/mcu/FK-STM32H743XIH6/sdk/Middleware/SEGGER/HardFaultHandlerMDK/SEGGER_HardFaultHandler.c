/** *******************************************************************
 *                SEGGER Microcontroller GmbH & Co. KG                *
 *                        The Embedded Experts                        *
 **********************************************************************
 *                                                                    *
 *       (c) 1995 - 2017 SEGGER Microcontroller GmbH & Co. KG         *
 *                                                                    *
 *       Internet: segger.com  Support: support_embos@segger.com      *
 *                                                                    *
 **********************************************************************
 *                                                                    *
 *       embOS * Real time operating system for microcontrollers      *
 *                                                                    *
 *       Please note:                                                 *
 *                                                                    *
 *       Knowledge of this file may under no circumstances            *
 *       be used to write a similar product or a real-time            *
 *       operating system for in-house use.                           *
 *                                                                    *
 *       Thank you for your fairness !                                *
 *                                                                    *
 **********************************************************************
 *                                                                    *
 *       OS version: 4.32                                             *
 *                                                                    *
 **********************************************************************

----------------------------------------------------------------------
File    : SEGGER_HardFaultHandler.c
Purpose : Generic SEGGER HardFault handler for Cortex-M
Literature:
  [1]  Analyzing HardFaults on Cortex-M CPUs (https://www.segger.com/downloads/appnotes/AN00016_AnalyzingHardFaultsOnCortexM.pdf)

Additional information:
  This HardFault handler enables user-friendly analysis of hard faults
  in debug configurations.
  If a release configuration requires a HardFault handler,
  a specific HardFault handler should be included instead,
  which for example issues a reset or lits an error LED.
--------  END-OF-HEADER  ---------------------------------------------
 */
#include "bsp_uart.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/** *******************************************************************
 *
 *       Defines
 *
 **********************************************************************
 */
#define SYSHND_CTRL (*(volatile unsigned int *)(0xE000ED24u))    // System Handler Control and State Register
#define NVIC_MFSR   (*(volatile unsigned char *)(0xE000ED28u))   // Memory Management Fault Status Register
#define NVIC_BFSR   (*(volatile unsigned char *)(0xE000ED29u))   // Bus Fault Status Register
#define NVIC_UFSR   (*(volatile unsigned short *)(0xE000ED2Au))  // Usage Fault Status Register
#define NVIC_HFSR   (*(volatile unsigned int *)(0xE000ED2Cu))    // Hard Fault Status Register
#define NVIC_DFSR   (*(volatile unsigned int *)(0xE000ED30u))    // Debug Fault Status Register
#define NVIC_BFAR   (*(volatile unsigned int *)(0xE000ED38u))    // Bus Fault Manage Address Register
#define NVIC_AFSR   (*(volatile unsigned int *)(0xE000ED3Cu))    // Auxiliary Fault Status Register

#ifndef DEBUG          // Should be overwritten by project settings
#define DEBUG     (1)  // in debug builds
#define PRINT_COM (COM1)
#endif

#define ERR_INFO "\r\n发生了硬故障中断，进入函数HardFault_Handler，系统终止运行！\r\n"

/** *******************************************************************
 *
 *       Static function prototypes
 *
 **********************************************************************
 */
#ifdef __cplusplus
extern "C" {
#endif
void HardFaultHandler(unsigned int *pStack);
#ifdef __cplusplus
}
#endif

/** *******************************************************************
 *
 *       Static data
 *
 **********************************************************************
 */
#if DEBUG
static volatile unsigned int _Continue;  // Set this variable to 1 to run further
static int BufferIndex = 0;

static struct
{
    struct
    {
        volatile unsigned int r0;   // Register R0
        volatile unsigned int r1;   // Register R1
        volatile unsigned int r2;   // Register R2
        volatile unsigned int r3;   // Register R3
        volatile unsigned int r12;  // Register R12
        volatile unsigned int lr;   // Link register
        volatile unsigned int pc;   // Program counter
        union
        {
            volatile unsigned int byte;
            struct
            {
                unsigned int IPSR : 8;   // Interrupt Program Status register (IPSR)
                unsigned int EPSR : 19;  // Execution Program Status register (EPSR)
                unsigned int APSR : 5;   // Application Program Status register (APSR)
            } bits;
        } psr;  // Program status register.
    } SavedRegs;

    union
    {
        volatile unsigned int byte;
        struct
        {
            unsigned int MEMFAULTACT : 1;  // Read as 1 if memory management fault is active
            unsigned int BUSFAULTACT : 1;  // Read as 1 if bus fault exception is active
            unsigned int UnusedBits1 : 1;
            unsigned int USGFAULTACT : 1;  // Read as 1 if usage fault exception is active
            unsigned int UnusedBits2 : 3;
            unsigned int SVCALLACT : 1;   // Read as 1 if SVC exception is active
            unsigned int MONITORACT : 1;  // Read as 1 if debug monitor exception is active
            unsigned int UnusedBits3 : 1;
            unsigned int PENDSVACT : 1;       // Read as 1 if PendSV exception is active
            unsigned int SYSTICKACT : 1;      // Read as 1 if SYSTICK exception is active
            unsigned int USGFAULTPENDED : 1;  // Usage fault pended; usage fault started but was replaced by a higher-priority exception
            unsigned int MEMFAULTPENDED : 1;  // Memory management fault pended; memory management fault started but was replaced by a higher-priority exception
            unsigned int BUSFAULTPENDED : 1;  // Bus fault pended; bus fault handler was started but was replaced by a higher-priority exception
            unsigned int SVCALLPENDED : 1;    // SVC pended; SVC was started but was replaced by a higher-priority exception
            unsigned int MEMFAULTENA : 1;     // Memory management fault handler enable
            unsigned int BUSFAULTENA : 1;     // Bus fault handler enable
            unsigned int USGFAULTENA : 1;     // Usage fault handler enable
        } bits;
    } syshndctrl;  // System Handler Control and State Register (0xE000ED24)

    union
    {
        volatile unsigned char byte;
        struct
        {
            unsigned char IACCVIOL : 1;  // Instruction access violation
            unsigned char DACCVIOL : 1;  // Data access violation
            unsigned char UnusedBits : 1;
            unsigned char MUNSTKERR : 1;  // Unstacking error
            unsigned char MSTKERR : 1;    // Stacking error
            unsigned char UnusedBits2 : 2;
            unsigned char MMARVALID : 1;  // Indicates the MMAR is valid
        } bits;
    } mfsr;  // Memory Management Fault Status Register (0xE000ED28)

    union
    {
        volatile unsigned int byte;
        struct
        {
            unsigned int IBUSERR : 1;     // Instruction access violation
            unsigned int PRECISERR : 1;   // Precise data access violation
            unsigned int IMPREISERR : 1;  // Imprecise data access violation
            unsigned int UNSTKERR : 1;    // Unstacking error
            unsigned int STKERR : 1;      // Stacking error
            unsigned int UnusedBits : 2;
            unsigned int BFARVALID : 1;  // Indicates BFAR is valid
        } bits;
    } bfsr;                      // Bus Fault Status Register (0xE000ED29)
    volatile unsigned int bfar;  // Bus Fault Manage Address Register (0xE000ED38)

    union
    {
        volatile unsigned short byte;
        struct
        {
            unsigned short UNDEFINSTR : 1;  // Attempts to execute an undefined instruction
            unsigned short INVSTATE : 1;    // Attempts to switch to an invalid state (e.g., ARM)
            unsigned short INVPC : 1;       // Attempts to do an exception with a bad value in the EXC_RETURN number
            unsigned short NOCP : 1;        // Attempts to execute a coprocessor instruction
            unsigned short UnusedBits : 4;
            unsigned short UNALIGNED : 1;  // Indicates that an unaligned access fault has taken place
            unsigned short DIVBYZERO : 1;  // Indicates a divide by zero has taken place (can be set only if DIV_0_TRP is set)
        } bits;
    } ufsr;  // Usage Fault Status Register (0xE000ED2A)

    union
    {
        volatile unsigned int byte;
        struct
        {
            unsigned int UnusedBits : 1;
            unsigned int VECTBL : 1;  // Indicates hard fault is caused by failed vector fetch
            unsigned int UnusedBits2 : 28;
            unsigned int FORCED : 1;    // Indicates hard fault is taken because of bus fault/memory management fault/usage fault
            unsigned int DEBUGEVT : 1;  // Indicates hard fault is triggered by debug event
        } bits;
    } hfsr;  // Hard Fault Status Register (0xE000ED2C)

    union
    {
        volatile unsigned int byte;
        struct
        {
            unsigned int HALTED : 1;    // Halt requested in NVIC
            unsigned int BKPT : 1;      // BKPT instruction executed
            unsigned int DWTTRAP : 1;   // DWT match occurred
            unsigned int VCATCH : 1;    // Vector fetch occurred
            unsigned int EXTERNAL : 1;  // EDBGRQ signal asserted
        } bits;
    } dfsr;  // Debug Fault Status Register (0xE000ED30)

    volatile unsigned int afsr;  // Auxiliary Fault Status Register (0xE000ED3C), Vendor controlled (optional)
} HardFaultRegs;
#endif

/** *******************************************************************
 *
 *       Static functions
 *
 **********************************************************************
 */

/** *******************************************************************
 *
 *       Global functions
 *
 **********************************************************************
 */

/** *******************************************************************
 *
 *       HardFaultHandler()
 *
 *  Function description
 *    C part of the hard fault handler which is called by the assembler
 *    function HardFault_Handler
 */
void HardFaultHandler(unsigned int *pStack)
{
    //
    // In case we received a hard fault because of a breakpoint instruction, we return.
    // This may happen when using semihosting for printf outputs and no debugger is connected,
    // i.e. when running a "Debug" configuration in release mode.
    //
    if (NVIC_HFSR & (1u << 31))
    {
        NVIC_HFSR |= (1u << 31);  // Reset Hard Fault status
        *(pStack + 6u) += 2u;     // PC is located on stack at SP + 24 bytes. Increment PC by 2 to skip break instruction.
        return;                   // Return to interrupted application
    }
#if DEBUG
    //
    // Read NVIC registers
    //
    HardFaultRegs.syshndctrl.byte = SYSHND_CTRL;  // System Handler Control and State Register
    HardFaultRegs.mfsr.byte = NVIC_MFSR;          // Memory Fault Status Register
    HardFaultRegs.bfsr.byte = NVIC_BFSR;          // Bus Fault Status Register
    HardFaultRegs.bfar = NVIC_BFAR;               // Bus Fault Manage Address Register
    HardFaultRegs.ufsr.byte = NVIC_UFSR;          // Usage Fault Status Register
    HardFaultRegs.hfsr.byte = NVIC_HFSR;          // Hard Fault Status Register
    HardFaultRegs.dfsr.byte = NVIC_DFSR;          // Debug Fault Status Register
    HardFaultRegs.afsr = NVIC_AFSR;               // Auxiliary Fault Status Register

    // Output initial error message
    UART_SendStringImm(PRINT_COM, ERR_INFO);

    // Output saved registers from the stack
    UART_SendStringImm(PRINT_COM, "\r\n=== 栈中保存的寄存器数据 ===\r\n");
    UART_PrintfImm(PRINT_COM, "R0  = 0x%08X\r\n", pStack[0]);
    UART_PrintfImm(PRINT_COM, "R1  = 0x%08X\r\n", pStack[1]);
    UART_PrintfImm(PRINT_COM, "R2  = 0x%08X\r\n", pStack[2]);
    UART_PrintfImm(PRINT_COM, "R3  = 0x%08X\r\n", pStack[3]);
    UART_PrintfImm(PRINT_COM, "R12 = 0x%08X\r\n", pStack[4]);
    UART_PrintfImm(PRINT_COM, "LR  = 0x%08X\r\n", pStack[5]);
    UART_PrintfImm(PRINT_COM, "PC  = 0x%08X\r\n", pStack[6]);
    UART_PrintfImm(PRINT_COM, "PSR = 0x%08X\r\n", pStack[7]);

    // Output fault status registers
    UART_SendStringImm(PRINT_COM, "\r\n=== 故障状态寄存器 ===\r\n");
    UART_PrintfImm(PRINT_COM, "HFSR = 0x%08X", HardFaultRegs.hfsr.byte);
    if (HardFaultRegs.hfsr.bits.FORCED) UART_PrintfImm(PRINT_COM, " (FORCED)");
    if (HardFaultRegs.hfsr.bits.VECTBL) UART_PrintfImm(PRINT_COM, " (VECTBL)");
    UART_PrintfImm(PRINT_COM, "\r\n");

    UART_PrintfImm(PRINT_COM, "CFSR = 0x%04X%02X%02X\r\n",
                   HardFaultRegs.ufsr.byte,
                   HardFaultRegs.bfsr.byte,
                   HardFaultRegs.mfsr.byte);

    UART_PrintfImm(PRINT_COM, "  MFSR = 0x%02X", HardFaultRegs.mfsr.byte);
    if (HardFaultRegs.mfsr.bits.IACCVIOL) UART_PrintfImm(PRINT_COM, " IACCVIOL");
    if (HardFaultRegs.mfsr.bits.DACCVIOL) UART_PrintfImm(PRINT_COM, " DACCVIOL");
    if (HardFaultRegs.mfsr.bits.MUNSTKERR) UART_PrintfImm(PRINT_COM, " MUNSTKERR");
    if (HardFaultRegs.mfsr.bits.MSTKERR) UART_PrintfImm(PRINT_COM, " MSTKERR");
    UART_PrintfImm(PRINT_COM, "\r\n");

    UART_PrintfImm(PRINT_COM, "  BFSR = 0x%02X", HardFaultRegs.bfsr.byte);
    if (HardFaultRegs.bfsr.bits.IBUSERR) UART_PrintfImm(PRINT_COM, " IBUSERR");
    if (HardFaultRegs.bfsr.bits.PRECISERR) UART_PrintfImm(PRINT_COM, " PRECISERR");
    if (HardFaultRegs.bfsr.bits.IMPREISERR) UART_PrintfImm(PRINT_COM, " IMPREISERR");
    if (HardFaultRegs.bfsr.bits.UNSTKERR) UART_PrintfImm(PRINT_COM, " UNSTKERR");
    if (HardFaultRegs.bfsr.bits.STKERR) UART_PrintfImm(PRINT_COM, " STKERR");
    if (HardFaultRegs.bfsr.bits.BFARVALID) UART_PrintfImm(PRINT_COM, " BFARVALID");
    UART_PrintfImm(PRINT_COM, "\r\n");

    UART_PrintfImm(PRINT_COM, "  UFSR = 0x%04X", HardFaultRegs.ufsr.byte);
    if (HardFaultRegs.ufsr.bits.UNDEFINSTR) UART_PrintfImm(PRINT_COM, " UNDEFINSTR");
    if (HardFaultRegs.ufsr.bits.INVSTATE) UART_PrintfImm(PRINT_COM, " INVSTATE");
    if (HardFaultRegs.ufsr.bits.INVPC) UART_PrintfImm(PRINT_COM, " INVPC");
    if (HardFaultRegs.ufsr.bits.NOCP) UART_PrintfImm(PRINT_COM, " NOCP");
    if (HardFaultRegs.ufsr.bits.UNALIGNED) UART_PrintfImm(PRINT_COM, " UNALIGNED");
    if (HardFaultRegs.ufsr.bits.DIVBYZERO) UART_PrintfImm(PRINT_COM, " DIVBYZERO");
    UART_PrintfImm(PRINT_COM, "\r\n");

    if (HardFaultRegs.bfsr.bits.BFARVALID)
    {
        UART_PrintfImm(PRINT_COM, "BFAR = 0x%08X\r\n", HardFaultRegs.bfar);
    }

    UART_PrintfImm(PRINT_COM, "DFSR = 0x%08X", HardFaultRegs.dfsr.byte);
    if (HardFaultRegs.dfsr.bits.HALTED) UART_PrintfImm(PRINT_COM, " HALTED");
    if (HardFaultRegs.dfsr.bits.BKPT) UART_PrintfImm(PRINT_COM, " BKPT");
    if (HardFaultRegs.dfsr.bits.DWTTRAP) UART_PrintfImm(PRINT_COM, " DWTTRAP");
    if (HardFaultRegs.dfsr.bits.VCATCH) UART_PrintfImm(PRINT_COM, " VCATCH");
    if (HardFaultRegs.dfsr.bits.EXTERNAL) UART_PrintfImm(PRINT_COM, " EXTERNAL");
    UART_PrintfImm(PRINT_COM, "\r\n");

    UART_PrintfImm(PRINT_COM, "AFSR = 0x%08X\r\n", HardFaultRegs.afsr);

    // Output SHCSR (System Handler Control and State)
    UART_SendStringImm(PRINT_COM, "\r\n=== 系统句柄控制寄存器 ===\r\n");
    UART_PrintfImm(PRINT_COM, "SHCSR = 0x%08X\r\n", HardFaultRegs.syshndctrl.byte);

    //
    // Save registers to structure for debug inspection
    //
    HardFaultRegs.SavedRegs.r0 = pStack[0];  // Register R0
    HardFaultRegs.SavedRegs.r1 = pStack[1];  // Register R1
    HardFaultRegs.SavedRegs.r2 = pStack[2];  // Register R2

    HardFaultRegs.SavedRegs.r3 = pStack[3];        // Register R3
    HardFaultRegs.SavedRegs.r12 = pStack[4];       // Register R12
    HardFaultRegs.SavedRegs.lr = pStack[5];        // Link register LR
    HardFaultRegs.SavedRegs.pc = pStack[6];        // Program counter PC
    HardFaultRegs.SavedRegs.psr.byte = pStack[7];  // Program status word PSR

    //
    // Halt execution
    // To step out of the HardFaultHandler, change the variable value to != 0.
    //
    _Continue = 0u;
    while (_Continue == 0u)
    {
    }
#else
    //
    // If this module is included in a release configuration, simply stay in the HardFault handler
    //
    (void)pStack;
    do
    {
    } while (1);
#endif
}

/*************************** End of file ****************************/