// This example shows how to link with existing Uxntal code.
//
// To compile:
//
//     # cd ~/blah/chibicc
//     ./compile.sh examples/mandelbrot_fast.c > tmp.tal
//     cat examples/mandelbrot_mul.tal >> tmp.tal
//     uxnasm tmp.tal tmp.rom
//     uxnemu tmp.rom

#include <varvara.h>

// Multiply two 8.8 fixed point numbers.
// Defined in mandelbrot_mul.tal
extern int mul(unsigned x, unsigned y);

int x0, y0, x, y, i, xx, yy, tmp;

void main(void) {
  set_palette(0x08df, 0x12bf, 0x549d);
  set_screen_size(320, 288);
  for (x0 = -256; x0 < 64; ++x0) {
    for (y0 = -144; y0 <= 0; ++y0) {
      x = 0;
      y = 0;
      for (i = 0; i < 25; ++i) {
        if ((xx = mul(x, x)) + (yy = mul(y, y)) > 4 * 256)
          break;
        tmp = xx - yy + x0 * 2;
        y = 2 * mul(x, y) + y0 * 2;
        x = tmp;
      }
      tmp = (unsigned)(i + (x0 + y0 & 1)) / 8;
      set_screen_xy(x0 + 256, y0 + 144);
      draw_pixel(tmp);
      set_screen_y(144 - y0);
      draw_pixel(tmp);
    }
  }
}
