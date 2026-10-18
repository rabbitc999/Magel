.syntax unified
.thumb
.global HardFault_Handler
.type HardFault_Handler,%function
HardFault_Handler:
  tst lr, #4
  ite eq
  mrseq r0, msp
  mrsne r0, psp
  b HardFaultHandler
.size HardFault_Handler,.-HardFault_Handler
