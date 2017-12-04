#include "mcall.h"

  .section ".text.init"
  .globl _start
_start:
        li a1, 0x88000000
        add sp, a1, -8

        la a0, str
        call puts
        li      a7, SBI_SHUTDOWN
        ecall
  .globl puts
puts:
        li      a7, SBI_CONSOLE_PUTCHAR
        mv      s0,a0
        lbu     a0,0(s0)
        beqz    a0,2f
3:     
        addi    s0,s0,1
        ecall
        lbu     a0,0(s0)
        bnez    a0,3b
2:      
        ret
	.globl putchar
putchar:	
        mv      a0,s0
        ecall
        ret

str:
  .asciz "This is bbl's dummy_payload\n"
