// See LICENSE for license details.

#include <stdint.h>
#include "mtrap.h"
#include "uart.h"

volatile uint32_t *uart_base_ptr;

void uart_init(uint64_t uart) {
  uart_base_ptr = (uint32_t *)(uart|0x1000);
  // set 0x0080 to UART.LCR to enable DLL and DLM write
  // configure baud rate
  *(uart_base_ptr + UART_LCR) = 0x0080;

  // System clock 25 MHz, 115200 baud rate
  // divisor = clk_freq / (16 * Baud)
  *(uart_base_ptr + UART_DLL) = 25*1000*1000u / (16u * 115200u) % 0x100u;
  *(uart_base_ptr + UART_DLM) = 25*1000*1000u / (16u * 115200u) >> 8;

  // 8-bit data, 1-bit odd parity
  *(uart_base_ptr + UART_LCR) = 0x000Bu;

  // Enable read IRQ
  *(uart_base_ptr + UART_IER) = 0x0001u;

}

void uart_send(uint8_t data) {
  // wait until THR empty
  while(! (*(uart_base_ptr + UART_LSR) & 0x40u));
  *(uart_base_ptr + UART_THR) = data;
}

void uart_send_string(const char *str) {
  while(*str != 0) {
    while(! (*(uart_base_ptr + UART_LSR) & 0x40u));
    *(uart_base_ptr + UART_THR) = *(str++);
  }
}

void uart_send_buf(const char *buf, const int32_t len) {
  int32_t i;
  for(i=0; i<len; i++) {
    while(! (*(uart_base_ptr + UART_LSR) & 0x40u));
    *(uart_base_ptr + UART_THR) = *(buf + i);
  }
}

uint8_t uart_recv() {
  // wait until RBR has data
  while(! (*(uart_base_ptr + UART_LSR) & 0x01u));

  return *(uart_base_ptr + UART_RBR);

}

// IRQ triggered read
uint8_t uart_read_irq() {
  return *(uart_base_ptr + UART_RBR);
}

// check uart IRQ for read
uint8_t uart_check_read_irq() {
  return (*(uart_base_ptr + UART_LSR) & 0x01u);
}

// enable uart read IRQ
void uart_enable_read_irq() {
  *(uart_base_ptr + UART_IER) = 0x0001u;
}

// disable uart read IRQ
void uart_disable_read_irq() {
  *(uart_base_ptr + UART_IER) = 0x0000u;
}

uint64_t err = 0, ddr = 0, rom = 0, bram = 0, intc = 0, spi = 0, uart = 0, clin = 0, dumh = 0;

void query_uart(uintptr_t fdt)
{
  uint64_t unknown = 0;
  char *unknownstr, *config = (char *)0x10000;
  for (int i = 128; i < 4096; i++)
    {
      char ch = config[i] & 0x7f; 
      if (ch == '@')
        {
          int j = i+1;
          unsigned addr = 0;
          while ((config[j] >= '0' && config[j] <= '9') || (config[j] >= 'a' && config[j] <= 'f'))
            {
              int hex = config[j] - '0';
              if (hex > 9) hex = config[j] - 'a' + 10;
              addr = (addr << 4) | hex;
              j++;
            }
          j = i - 1;
          while ((config[j] >= 'a' && config[j] <= 'z') || config[j] == '-')
            j--;
          if ((++j < i) && addr)
            {
              uint32_t label = (config[j]<<24) | (config[j+1]<<16) | (config[j+2]<<8) |(config[j+3]);
              switch (label)
                {
                case 'memo':
                  ddr = addr;
                  break;
                case 'bram':
                  bram = addr;
                  break;
                case 'clin':
                  clin = addr;
                  break;
                case 'dumh':
                  dumh = addr;
                  break;
                case 'erro':
                  err = addr;
                  break;
                case 'inte':
                  intc = addr;
                  break;
                case 'rom@':
                  rom = addr;
                  break;
                case 'seri':
                  uart = addr;
                  break;
                case 'spi@':
                  spi = addr;
                  break;
                default:
                  unknown = addr;
                  unknownstr = config+j;
                  break;
                }
            }
        }
    }
  if (uart)
    {
      uart_init(uart);
      printm("Serial controller start 0x%x\n", uart);
      if (dumh) printm("Dummy host start 0x%x\n", dumh);
      if (err) printm("Error device start 0x%x\n", err);
      if (rom) printm("ROM start 0x%x\n", rom);
      if (bram) printm("Block RAM start 0x%x\n", bram);
      if (ddr) printm("DDR memory start 0x%x\n", ddr);
      if (intc) printm("Interrupt controller start 0x%x\n", intc);
      if (spi) printm("SPI controller start 0x%x\n", spi);
      if (unknown)
        printm("Unknown %s, start = 0x%x\n", unknownstr, unknown);
    }
}
