# chibicc-uxn

This is Rui Ueyama's [chibicc@historical/old](https://github.com/rui314/chibicc/tree/historical/old), retargeted to compile C to [Uxntal](https://wiki.xxiivv.com/site/uxntal.html).

Uxntal is the low-level programming language for [Uxn](https://100r.co/site/uxn.html), a virtual machine for writing tools and games that run on almost any hardware. See [awesome-uxn](https://github.com/hundredrabbits/awesome-uxn).

![image](https://github.com/lynn/chibicc/assets/16232127/312c01e7-0a82-43c0-86ef-6215010cd250)

chibicc-uxn is featureful enough to write small [games](examples/danmaku.c) and [demos](example/star.c). We have even managed to port [a classic desktop pet application](https://github.com/hikari-no-yume/oneko-uxn) from X11!

## Usage

Running `make` will build `./chibicc`.

chibicc itself has no preprocessor, but any C compiler's `-E` flag will do. We use `-I.` to include `varvara.h` or just `uxn.h` and `-P` to eliminate `#` lines from the preprocessor output:

```sh
gcc -I. -P -E examples/day3.c -o tmp.c
./chibicc -O1 tmp.c > tmp.tal
uxnasm tmp.tal tmp.rom
uxnemu tmp.rom
```

The `-O1` or `-O` flag enables the optimization pass. If the flag is omitted, this is equivalent to `-O0` (no optimization).

There's a convenient script that just runs the above commands: `./run.sh examples/day3.c` (compile + uxnasm + uxnemu).

For a more complex and visually interesting demo, try `./run.sh examples/star.c`.

See also `make test`, which runs a test suite.

## Supported

- Boring C89 stuff: functions, loops, global and local variables, arrays, pointers, structs, enums.
- `char` (8 bits), `short` (16 bits), `int` (16 bits), `unsigned`.
- Emulation of signed comparisons, signed division and [arithmetic right shift](https://en.wikipedia.org/wiki/Arithmetic_shift).
  - But these are slow, so use `unsigned` if you want fast code.
- [Variadic functions](https://en.cppreference.com/w/c/variadic), in a very limited fashion — see `examples/variadic.c`.
- Simple [peephole optimizations](https://en.wikipedia.org/wiki/Peephole_optimization), like `#0004 MUL2` → `#20 SFT2` or `#0004 0005 ADD2` → `#0009`.

## Not supported

- The preprocessor.
- `long` (32-bit integers), `long long` (64-bit integers), `float` or `double`.
  - I don't really want to add support for them. However, [math32.tal](http://plastic-idolatry.com/erik/nxu/math32.tal) exists.
- [Function pointers](https://en.wikipedia.org/wiki/Function_pointer).
- [Variable-length arrays](https://en.cppreference.com/w/c/language/array#Variable-length_arrays).
- [Bit-fields](https://en.cppreference.com/w/c/language/bit_field).
- Functions that take or return structs.

## Varvara

[Varvara](https://wiki.xxiivv.com/site/varvara.html) is the set of I/O interfaces for Uxn-based tools and games.

[varvara.h](./varvara.h) defines macros for setting the window size, drawing sprites on the screen, playing sounds, etc.

To set up Varvara event handlers, just define any of the following functions:

- `void on_console(void);`
  - Called when a byte is received.
  - Use `console_read()` or `console_type()` to process the event.
- `void on_screen(void);`
  - Called 60 times per second to update the screen.
- `void on_audio1(void);`
  - Called when audio ends on channel 1.
- `void on_audio2(void);`
  - Called when audio ends on channel 2.
- `void on_audio3(void);`
  - Called when audio ends on channel 3.
- `void on_audio4(void);`
  - Called when audio ends on channel 4.
- `void on_controller(void);`
  - Called when a button is pressed or released on the controller, or a keyboard key is pressed.
  - Use `controller_button()` or `controller_key()` to process the event.
- `void on_mouse(void);`
  - Called when the mouse is moved.
  - Use `mouse_x()`, `mouse_y()` and `mouse_state()` to process the event.

If they are defined, the compiled startup code will hook them up to the right devices before calling your `main`.

`void main(int argc, char *argv[])` is supported, but this will add [a huge support routine](routines/argc_argv.tal) to your code, so in some cases a custom `on_console` routine is more efficient. Demo: `./run.sh --cli examples/argc_argv.c foo bar 'foo bar'`.

Note that the program doesn't terminate when it reaches the end of `main`; as such, the return value is ignored. Use `exit()` if you want to exit and set the status code.

## Interfacing with assembly

The variadic intrinsic `asm()` function accepts a number of `int` arguments which are pushed in order, followed by some Uxntal code that should leave one `int` on the stack.

```c
int sum_of_squares(int x, int y) {
  return asm(x, y, "DUP2 MUL2 SWP2 DUP2 MUL2 ADD2");
}
```

This is useful if you just want a little Uxntal idiom in the middle of your C.

However, in this case `sum_of_squares` still contains a lot of unnecessary code (creating a stack frame, and copying `x` and `y` to and from it). We can avoid this by simply writing an Uxntal function in a `.tal` file and linking it with our compiler output:

```c
// code.c
extern int sum_of_squares(int x, int y);
```

```tal
( sum_of_squares.tal )
( Note the underscore at the end of the function name. TODO?: don't mangle extern function calls. )
@sum_of_squares_ ( y* x* -> result* )
  DUP2 MUL2 SWP2 DUP2 MUL2 ADD2
  JMP2r
```

```sh
# build.sh
chibicc code.c > tmp.tal
cat sum_of_squares.tal >> tmp.tal
uxnasm tmp.tal tmp.rom
```

See [examples/mandelbrot_fast.c](./examples/mandelbrot_fast.c).

### Calling convention

To write uxntal that's compatible with chibicc, you need to know how chibicc calls functions.

Arguments are pushed to the stack in reverse order: a C function like `int foo(int x, int y, int z);` corresponds to an uxntal signature like `( z* y* x* -- result* )`.

Arguments are always passed on the working stack as shorts (16-bit). The result is always a short, even if the C type is `void` or `char`. Your uxntal implementation of a void-returning function should leave a 16-bit dummy value on the stack before returning.

Some examples:

| C signature | uxntal signature |
| --- | --- |
| `int f(int a, int b);` | `( b* a* -- result* )` |
| `char g(int a, char b);` | `( b* a* -- result* )` |
| `void h(void);` | `( -- result* )` |
