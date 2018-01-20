// See LICENSE for license details.

#include <stdint.h>
#include <string.h>
#include "mtrap.h"
#include "hid.h"

volatile uint32_t *hid_base_ptr;

void hid_console_putchar(unsigned char ch)
{
  static int addr_int = 4096-256;
  switch(ch)
    {
    case 8: case 127: if (addr_int & 127) hid_base_ptr[HID_VGA+(--addr_int)] = ' '; break;
    case 13: addr_int = addr_int & -128; break;
    case 10: addr_int = (addr_int|127)+1; break;
    default: hid_base_ptr[HID_VGA+addr_int++] = ch;
    }
  if (addr_int >= 4096-128)
    {
      // this is where we scroll
      for (addr_int = 0; addr_int < 4096; addr_int++)
        if (addr_int < 4096-128)
          hid_base_ptr[HID_VGA+addr_int] = hid_base_ptr[HID_VGA+addr_int+128];
        else
          hid_base_ptr[HID_VGA+addr_int] = ' ';
      addr_int = 4096-256;
    }
}

void hid_init(void *base) {
  //  extern volatile uint32_t *sd_base;
  //  sd_base = (uint32_t *)(0x00010000 + (char *)base);
  hid_base_ptr = (uint32_t *)base;
}

void hid_send_irq(uint8_t data)
{

}

void hid_send(uint8_t data)
{
  hid_console_putchar(data);
    *(hid_base_ptr + HID_LED) = data;
}

void hid_send_string(const char *str) {
  while (*str) hid_send(*str++);
}

void hid_send_buf(const char *buf, const int32_t len)
{
  int32_t i;
  for (i=0; i<len; i++) hid_send(buf[i]);
}

uint8_t hid_recv()
{
  return *(hid_base_ptr + HID_DIP);
}

// IRQ triggered read
uint8_t hid_read_irq() {
  return *(hid_base_ptr + HID_DIP);
}

// check hid IRQ for read
uint8_t hid_check_read_irq() {
  return 0;
}

// enable hid read IRQ
void hid_enable_read_irq() {

}

size_t err = 0, eth = 0, ddr = 0, rom = 0, bram = 0, intc = 0, clin = 0, hid = 0;

void query_hid(uintptr_t fdt)
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
          while ((config[j] >= '0' && config[j] <= '9') || (config[j] >= 'a' && config[j] <= 'z') || config[j] == '-')
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
                case 'erro':
                  err = addr;
                  break;
                case 'eth@':
                  eth = addr;
                  break;
                case 'inte':
                  intc = addr;
                  break;
                case 'rom@':
                  rom = addr;
                  break;
                case 'mmio':
                  if (config[j+4] == '@')
                    {
                    bram = addr;
                    }
                  else
                    {
                      hid = addr;
                    }
                  break;
                default:
                  unknown = addr;
                  unknownstr = config+j;
                  break;
                }
            }
        }
    }
  if (hid)
    {
      hid_init((void *)hid);
      printm("MMIO2 (32-bit) start 0x%x\n", hid);
      if (err) printm("Error device start 0x%x\n", err);
      if (rom) printm("ROM start 0x%x\n", rom);
      if (bram) printm("Block RAM start 0x%x\n", bram);
      if (ddr) printm("DDR memory start 0x%x\n", ddr);
      if (intc) printm("Interrupt controller start 0x%x\n", intc);
      if (unknown)
        printm("Unknown %s, start = 0x%x\n", unknownstr, unknown);
    }
}
