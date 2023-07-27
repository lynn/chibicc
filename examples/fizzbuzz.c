// How to run:
//
// $ make
// $ gcc -I. -P -E examples/fizzbuzz.c > tmp.c
// $ ./chibicc tmp.c > tmp.tal
// $ uxnasm tmp.tal tmp.rom
// $ uxncli tmp.rom

#include <varvara.h>

void puts(char *s) {
  for (; *s; ++s)
    putchar(*s);
  putchar('\n');
}

void puts_num(int n) {
  if (n >= 100)
    putchar('0' + n / 100);
  if (n >= 10)
    putchar('0' + n / 10);
  putchar('0' + n % 10);
  putchar('\n');
}

void main(void) {
  for (int n = 1; n < 101; n++) {
    if (n % 3 == 0 && n % 5 == 0)
      puts("FizzBuzz");
    else if (n % 3 == 0)
      puts("Fizz");
    else if (n % 5 == 0)
      puts("Buzz");
    else
      puts_num(n);
  }
  exit(0);
}
