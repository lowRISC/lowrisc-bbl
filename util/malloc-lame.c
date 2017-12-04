#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
void printm(const char* s, ...);

static uint8_t *sbrk = (uint8_t *)0x87000000;

void *malloc(size_t siz)
{
  void *ptr = sbrk;
  printm("malloc(%d) returned %p\n", siz, ptr);
  sbrk += ((siz-1)|7)+1;
  return ptr;
}

void free(void *ptr)
{
  //  my_free(ptr);
}

void *realloc(void *oldptr, size_t siz)
{
  void *ptr = malloc(siz);
  if (ptr)
    memcpy(ptr, oldptr, siz);
  printm("realloc(%p,%d) returned %p\n", oldptr, siz, ptr);
  return ptr;
}

