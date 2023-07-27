#include <varvara.h>

// Writing a variadic function in chibicc-uxn:
// 1. Put ... at the end of the signature.
// 2. Declare `va_list ap;`
// 3. Call `va_start(ap, last_param);`
// 4. In your function, invoke `va_arg(ap, int)` to get the next var-arg.

// If the function is called with more arguments than the implementation
// consumes, the stack will be messed up. For example, `sum_until_zero(1, 0, 1)`
// will probably crash the program. Use variadic functions with care.

int sum_until_zero(int x, ...) {
  va_list ap;
  va_start(ap, x);

  for (;;) {
    int y = va_arg(ap, int);
    if (y == 0)
      return x;
    x += y;
  }
}

void main(void) {
  putchar('0' + sum_until_zero(1, 2, 3, -1, 1, 0));
  putchar('\n');
  exit(0);
}
