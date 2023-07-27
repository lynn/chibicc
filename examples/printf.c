#include <varvara.h>

#define args char *format
#define func_name printf
#define put(c) putchar(c)
#include "printf_impl.c"
#undef args
#undef func_name
#undef put

#define args char *buf, char *format
#define func_name sprintf
#define put(c) (*buf++ = (c))
#include "printf_impl.c"
#undef args
#undef func_name
#undef put

void main() {
  printf("hello %d %x %2x\n", 100, 100, 100);
  printf("hello %s!\n", "world");
  printf("hell%c worl%c\n", 111, 100);
  char test[64];
  sprintf(test, "%d", 1234);
  exit(0);
}
