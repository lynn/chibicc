// Raw interface to uxn's "devices"
void deo(char data, char device);
void deo2(int data, char device);
char dei(char device);
int dei2(char device);

int __builtin_va_arg();
typedef void *va_list;
#define va_start(ap, va)
#define va_end(ap)
#define va_arg(ap, ty) __builtin_va_arg()
