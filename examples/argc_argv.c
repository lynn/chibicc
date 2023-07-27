#include <varvara.h>

void print_int(unsigned char i) {
	putchar('0' + ((i / 10) % 10));
	putchar('0' + (i % 10));
}
void print_string(char *s) {
	for (; *s; s++) putchar(*s);
}

void main(int argc, char *argv[]) {
	print_string("argc: ");
	print_int(argc);
	putchar('\n');

	for (int i = 0; i < argc; i++) {
	    print_string("arg ");
		print_int(i);
		print_string(": ");
		print_string(argv[i]);
		putchar('\n');
	}

	exit(0);
}
