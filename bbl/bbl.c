#include "bbl.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include "config.h"
#include "fdt.h"
#include "uart.h"
#include <string.h>

void dramtest(void);
int dhry_main (void);
int micropython_main(void);

void test(void)
{
  printm("enter d for dhrystone test, m for dramtest, p for python shell, x to exit\r\n");
  do {
    uart_send('>');
    uint8_t ch = uart_recv();
    printm("%c\n", ch);
    switch(ch)
      {
      case 'd': dhry_main(); break;
      case 'm': dramtest(); break;
      case 'p': micropython_main(); break;
      case 'x': poweroff(); break;
      default : printm("%c: Unknown command\n", ch); break;
      }
  } while (1);
}

void boot_loader(uintptr_t dtb)
{
  print_logo();
  mb();
  test();
}
