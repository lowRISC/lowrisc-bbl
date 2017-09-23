#include "mtrap.h"
#include "mcall.h"
#include "atomic.h"
#include "bits.h"
#include "dev_map.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef DEV_MAP__io_ext_host__BASE
volatile uint64_t *tohost = (uint64_t *)DEV_MAP__io_ext_host__BASE;
#endif

void __attribute__((noreturn)) bad_trap()
{
  die("machine mode: unhandleable trap %d @ %p", read_csr(mcause), read_csr(mepc));
}

static uintptr_t mcall_hart_id()
{
  return read_const_csr(mhartid);
}

// WS need change
static uintptr_t mcall_console_putchar(uint8_t data)
{
  enum {UART_THR=0x0u,UART_LSR=0x5u};
  volatile uint32_t *uart_base_ptr = (uint32_t *)(DEV_MAP__io_ext_uart__BASE | 0x1000);
  // wait until THR empty
  while(! (*(uart_base_ptr + UART_LSR) & 0x40u));
  *(uart_base_ptr + UART_THR) = data;
  return 0;
}

/* This code exits and returns to the background before the supervisor user interrupt
   is triggered. So it will loop endlessly if interrupts are still enabled. DUH */

void external_interrupt()
{
  enum {UART_IER=0x4u,
	MACHI_OFFSET=0x0804,
	MACHI_IRQ_EN=0x00400000,
	SPI_GIE=0x1C};
  volatile uint32_t *uart_base = (uint32_t *)(DEV_MAP__io_ext_uart__BASE | 0x1000);
  volatile uint32_t *eth_base = (uint32_t *)(DEV_MAP__io_ext_eth__BASE);
  volatile uint32_t *spi_base = (uint32_t *)(DEV_MAP__io_ext_spi__BASE);
  uart_base[UART_IER>>2] = 0x0000u;
  eth_base[MACHI_OFFSET>>2] &= ~MACHI_IRQ_EN;
  spi_base[SPI_GIE>>2] &= ~0x80000000;
  set_csr(mip, MIP_SSIP);
}

uintptr_t timer_interrupt()
{
  // just send the timer interrupt to the supervisor
  clear_csr(mie, MIP_MTIP);
  set_csr(mip, MIP_STIP);

  return 0;
}

static uintptr_t mcall_htif_syscall(uintptr_t magic_mem)
{
  //do_tohost_fromhost(0, 0, magic_mem); //fesvr syscall
  return 0;
}

// WS need change
void poweroff()
{
  while (1) {
#ifdef DEV_MAP__io_ext_host__BASE
    *tohost = 1;
#endif
  }
}

void putstring(const char* s)
{
  while (*s)
    mcall_console_putchar(*s++);
}

void printm(const char* s, ...)
{
  char buf[256];
  va_list vl;

  va_start(vl, s);
  vsnprintf(buf, sizeof buf, s, vl);
  va_end(vl);

  putstring(buf);
}

static void send_ipi(uintptr_t recipient, int event)
{
  if ((atomic_or(&OTHER_HLS(recipient)->mipi_pending, event) & event) == 0) {
    mb();
    *OTHER_HLS(recipient)->ipi = 1;
  }
}

static uintptr_t mcall_send_ipi(uintptr_t recipient)
{
  if (recipient >= num_harts)
    return -1;

  send_ipi(recipient, IPI_SOFT);
  return 0;
}

static uintptr_t mcall_shutdown()
{
  poweroff();
}

static uintptr_t mcall_set_timer(uint64_t when)
{
  *HLS()->timecmp = when;
  clear_csr(mip, MIP_STIP);
  set_csr(mie, MIP_MTIP);
  return 0;
}

void software_interrupt()
{
  clear_csr(mip, MIP_MSIP);
  mb();
  int ipi_pending = atomic_swap(&HLS()->mipi_pending, 0);

  if (ipi_pending & IPI_SOFT) {
    HLS()->sipi_pending = 1;
    set_csr(mip, MIP_SSIP);
  }

  if (ipi_pending & IPI_FENCE_I)
    asm volatile ("fence.i");

  if (ipi_pending & IPI_SFENCE_VM)
    asm volatile ("sfence.vm");
}

static void send_ipi_many(uintptr_t* pmask, int event)
{
  _Static_assert(MAX_HARTS <= 8 * sizeof(*pmask), "# harts > uintptr_t bits");
  uintptr_t mask = -1;
  if (pmask)
    mask = *pmask;

  // send IPIs to everyone
  for (ssize_t i = num_harts-1; i >= 0; i--)
    if ((mask >> i) & 1)
      send_ipi(i, event);

  // wait until all events have been handled.
  // prevent deadlock while spinning by handling any IPIs from other harts.
  for (ssize_t i = num_harts-1; i >= 0; i--)
    if ((mask >> i) & 1)
      while (OTHER_HLS(i)->mipi_pending & event)
        software_interrupt();
}

static uintptr_t mcall_remote_sfence_vm(uintptr_t* hart_mask, uintptr_t asid)
{
  // ignore the ASID and do a global flush.
  // this allows us to avoid queueing a message.
  send_ipi_many(hart_mask, IPI_SFENCE_VM);
  return 0;
}

static uintptr_t mcall_remote_fence_i(uintptr_t* hart_mask)
{
  send_ipi_many(hart_mask, IPI_FENCE_I);
  return 0;
}

static uintptr_t mcall_config_string_base(void)
{
  /* Potentially prune hidden devices here */
  return *(uint32_t*)CONFIG_STRING_ADDR;
}

static uintptr_t mcall_config_string_size(void)
{
  const char* s = (const char*)mcall_config_string_base();
  return strlen(s)+1;
}

void mcall_trap(uintptr_t* regs, uintptr_t mcause, uintptr_t mepc)
{
  uintptr_t n = regs[17], arg0 = regs[10], arg1 = regs[11], retval;
  switch (n)
  {
    case MCALL_HART_ID:
      retval = mcall_hart_id();
      break;
    case MCALL_CONSOLE_PUTCHAR:
      retval = mcall_console_putchar(arg0);
      break;
    case MCALL_CONSOLE_GETCHAR:
      retval = ' ';
      break;
    case MCALL_HTIF_SYSCALL:
      retval = mcall_htif_syscall(arg0);
      break;
    case MCALL_SEND_IPI:
      retval = mcall_send_ipi(arg0);
      break;
    case MCALL_CLEAR_IPI:
      retval = (0);
      break;
    case MCALL_SHUTDOWN:
      retval = mcall_shutdown();
      break;
    case MCALL_SET_TIMER:
#ifdef __riscv32
      retval = mcall_set_timer(arg0 + ((uint64_t)arg1 << 32));
#else
      retval = mcall_set_timer(arg0);
#endif
      break;
    case MCALL_REMOTE_SFENCE_VM:
      retval = mcall_remote_sfence_vm((uintptr_t*)arg0, arg1);
      break;
    case MCALL_REMOTE_FENCE_I:
      retval = mcall_remote_fence_i((uintptr_t*)arg0);
      break;
    case MCALL_CONFIG_STRING_BASE:
      retval = mcall_config_string_base();
      break;
    case MCALL_CONFIG_STRING_SIZE:
      retval = mcall_config_string_size();
      break;
    default:
      retval = -ENOSYS;
      break;
  }
  regs[10] = retval;
  write_csr(mepc, mepc + 4);
}

void redirect_trap(uintptr_t epc, uintptr_t mstatus)
{
  write_csr(sepc, epc);
  write_csr(scause, read_csr(mcause));
  write_csr(mepc, read_csr(stvec));

  uintptr_t prev_priv = EXTRACT_FIELD(mstatus, MSTATUS_MPP);
  uintptr_t prev_ie = EXTRACT_FIELD(mstatus, MSTATUS_MPIE);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_SPP, prev_priv);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_SPIE, prev_ie);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPP, PRV_S);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPIE, 0);
  write_csr(mstatus, mstatus);

  extern void __redirect_trap();
  return __redirect_trap();
}

static void machine_page_fault(uintptr_t* regs, uintptr_t mepc)
{
  // MPRV=1 iff this trap occurred while emulating an instruction on behalf
  // of a lower privilege level. In that case, a2=epc and a3=mstatus.
  if (read_csr(mstatus) & MSTATUS_MPRV) {
    write_csr(sbadaddr, read_csr(mbadaddr));
    return redirect_trap(regs[12], regs[13]);
  }
  bad_trap();
}

void trap_from_machine_mode(uintptr_t* regs, uintptr_t dummy, uintptr_t mepc)
{
  uintptr_t mcause = read_csr(mcause);

  switch (mcause)
  {
    case CAUSE_FAULT_LOAD:
    case CAUSE_FAULT_STORE:
      return machine_page_fault(regs, mepc);
    default:
      bad_trap();
  }
}
