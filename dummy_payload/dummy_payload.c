#include <stdint.h>
#include "eth.h"
#include "sbi.h"
#include "mtrap.h"
#include "encoding.h"

asm (".globl _start\n\
  _start: la sp, stack\n\
  j entry\n\
  .pushsection .rodata\n\
  .align 4\n\
  .skip 4096\n\
  stack:\n\
  .popsection");

// #define VERBOSE

static int eth_read(int pingpong, size_t addr)
{
  size_t paddr = (pingpong&1 ? XEL_BUFFER_OFFSET : 0) | addr;
  uint32_t ret = sbi_eth_read(paddr);
#ifdef VERBOSE
  printf("eth_read(%x); returned %x\n", paddr, ret);
#endif  
  return ret;
}

static void eth_write(int pingpong, size_t addr, int data, int strb)
{
  size_t paddr = (pingpong&1 ? XEL_BUFFER_OFFSET : 0) | addr;
#ifdef VERBOSE
  printf("eth_write(%x,%x);\n", paddr, data);
#endif  
  sbi_eth_write(paddr, data);
#ifdef VERBOSE
  printf("eth_write completed\n");
#endif  
}

extern void trap_vector(void);

void entry()
{
  int cnt = 0;
  const char* message =
"This is bbl's dummy ethernet payload. "__TIMESTAMP__".\n";
  while (*message)
    sbi_console_putchar(*message++);
  /*
  set_csr(sie, 0x2UL);
  write_csr(sscratch, 0);
  write_csr(stvec, &trap_vector);
  */
  eth_init(sbi_console_putchar, eth_read, eth_write);
  while (1)
    {
    wfi();
    if (++cnt % 5000000 == 0)
      printf("wait for interrupt completed %d\n", cnt);
    }
  eth_loop();
}
