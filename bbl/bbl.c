#include "bbl.h"
#include "mtrap.h"
#include "atomic.h"
#include "vm.h"
#include "bits.h"
#include "config.h"
#include "fdt.h"
#include <string.h>

static const void* entry_point;

static uintptr_t dtb_output()
{
  extern char _payload_end;
  uintptr_t end = (uintptr_t) &_payload_end;
  return (end + MEGAPAGE_SIZE - 1) / MEGAPAGE_SIZE * MEGAPAGE_SIZE;
}

#if 0
static void filter_dtb(uintptr_t source)
{
  uintptr_t dest = dtb_output();
  uint32_t size = fdt_size(source);
  memcpy((void*)dest, (void*)source, size);

  // Remove information from the chained FDT
  filter_harts(dest, DISABLED_HART_MASK);
  filter_plic(dest);
  filter_compat(dest, "riscv,clint0");
  filter_compat(dest, "riscv,debug-013");
}
#endif

void boot_other_hart(uintptr_t dtb)
{
  const void* entry;
  do {
    entry = entry_point;
    mb();
  } while (!entry);
  printm("Entry point %p\n", entry);
  enter_supervisor_mode(entry, read_csr(mhartid), dtb_output());
}

void boot_loader(uintptr_t dtb)
{
  extern char _payload_start;
#if 0
  filter_dtb(dtb);
#else
  printm("Skipping filter_dtb\n");
#endif  
#ifdef PK_ENABLE_LOGO
  print_logo();
#endif
  mb();
  entry_point = &_payload_start;
  boot_other_hart(dtb);
}
