// star demo for chibicc-uxn by hikari_no_yume (2023-06-10 ~ 2023-06-14)

#include <varvara.h>

// generated with JS: x="";for(i=0;i<64;i++)x+=((Math.sin(i*Math.PI/128)*128)|0)+",";
char SIN_TABLE[64] = {0,3,6,9,12,15,18,21,24,28,31,34,37,40,43,46,48,51,54,57,60,63,65,68,71,73,76,78,81,83,85,88,90,92,94,96,98,100,102,104,106,108,109,111,112,114,115,117,118,119,120,121,122,123,124,124,125,126,126,127,127,127,127,127,};
char sin(char a)
{
  return SIN_TABLE[(a & 0x40 ? ~a : a) & 0x3f] * (a & 0x80 ? -1 : 1);
}
char cos(char a)
{
  return sin(a + 0x40);
}

// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm#All_cases
void line_low(int x0, int y0, int x1, int y1, char pixel)
{
  int dx = x1 - x0;
  int dy = y1 - y0;
  int yi = 1;
  if (dy < 0) {
    yi = -1;
    dy = -dy;
  }
  int d = (2 * dy) - dx;
  int y = y0;
  for (int x = x0; x <= x1; x++) {
    set_screen_xy(x, y);
    draw_pixel(pixel);
    if (d > 0) {
      y += yi;
      d += 2 * (dy - dx);
    } else {
      d += 2 * dy;
    }
  }
}
void line_high(int x0, int y0, int x1, int y1, char pixel)
{
  int dx = x1 - x0;
  int dy = y1 - y0;
  int xi = 1;
  if (dx < 0) {
    xi = -1;
    dx = -dx;
  }
  int d = (2 * dx) - dy;
  int x = x0;
  for (int y = y0; y <= y1; y++) {
    set_screen_xy(x, y);
    draw_pixel(pixel);
    if (d > 0) {
      x += xi;
      d += 2 * (dx - dy);
    } else {
      d += 2 * dx;
    }
  }
}
void line(int x0, int y0, int x1, int y1, char pixel)
{
  int y1y0 = y1 - y0;
  if (y1y0 < 0) y1y0 = -y1y0;
  int x1x0 = x1 - x0;
  if (x1x0 < 0) x1x0 = -x1x0;
  if (y1y0 < x1x0) {
    if (x0 > x1)
      line_low(x1, y1, x0, y0, pixel);
    else
      line_low(x0, y0, x1, y1, pixel);
  } else {
    if (y0 > y1)
      line_high(x1, y1, x0, y0, pixel);
    else
      line_high(x0, y0, x1, y1, pixel);
  }
}

char SPRITE[8] = {
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b01111111,
};

char t = 0;

void on_screen(void)
{
  t++;

  // clear layer 0
  set_screen_xy(0, 0);
  draw_pixel(BgFillBR);
  // draw star
  _Bool first = 1;
  int last_x;
  int last_y;
  for (unsigned i = 0; i < 11; i++) {
    unsigned char angle = t + (256 * (i % 10)) / 10;
    int x = sin(angle) >> (i & 1);
    int y = cos(angle) >> (i & 1);
    if (first) {
      first = 0;
    } else {
      line(127 + last_x, 127 + last_y, 127 + x, 127 + y, 0x03);
      line(127 + (last_x * 3 >> 2), 127 + (last_y * 3 >> 2), 127 + (x * 3 >> 2), 127 + (y * 3 >> 2), 0x02);
      line(127 + (last_x >> 1), 127 + (last_y >> 1), 127 + (x >> 1), 127 + (y >> 1), 0x01);
    }
    last_x = x;
    last_y = y;
  }
  // draw u
  set_screen_addr(SPRITE);
  set_screen_xy(113, 123);
  draw_sprite(0x01);
  // draw x
  int x = sin(t + 0x20) >> 5;
  int y = cos(t + 0x20) >> 5;
  line(127 - x - 1, 127 - y - 1, 127 + x, 127 + y, 0x03);
  x = sin(t + 0x60) >> 5;
  y = cos(t + 0x60) >> 5;
  line(127 - x - 1, 127 - y - 1, 127 + x, 127 + y, 0x03);
  // draw n
  set_screen_addr(SPRITE);
  set_screen_xy(133, 123);
  draw_sprite(0x31);
}

void main(void)
{
  set_palette(0x0aaf, 0x0ffc, 0x0faa);
  set_screen_size(256, 256);
}
