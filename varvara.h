#include <uxn.h>

void exit(char status);

typedef struct {
    char operation; // 1 = copy
    int length;
    int src_page;
    void* src_addr;
    int dst_page;
    void* dst_addr;
} Expansion;

// https://wiki.xxiivv.com/site/varvara.html
#define expansion(ptr) deo2(ptr, 0x02)
#define friend(func) deo2(func, 0x04)
#define set_palette(r, g, b) (deo2(r, 0x08), deo2(g, 0x0a), deo2(b, 0x0c))
#define debug() deo(0x01, 0x0e)

#define console_read() dei(0x12)
#define console_type() dei(0x17)
#define console_write(c) deo(c, 0x18) // stdout
#define console_error(c) deo(c, 0x19) // stderr
#define getchar console_read
#define putchar console_write

#define set_screen_size(width, height) (deo2(width, 0x22), deo2(height, 0x24))
#define screen_width() dei2(0x22)
#define screen_height() dei2(0x24)
#define set_screen_auto(a) deo(a, 0x26)
#define set_screen_x(x) deo2(x, 0x28)
#define set_screen_y(y) deo2(y, 0x2a)
#define set_screen_xy(x,y) (set_screen_x(x), set_screen_y(y))
#define screen_x() dei2(0x28)
#define screen_y() dei2(0x2a)
#define set_screen_addr(a) deo2(a, 0x2c)
#define draw_pixel(a) deo(a, 0x2e)
#define draw_sprite(a) deo(a, 0x2f)

#define audio_position(ch, a) dei2(0x32 + 0x10*ch)
#define audio_output(ch) dei(a, 0x34 + 0x10*ch)
#define set_audio_adsr(ch, adsr) deo2(adsr, 0x38 + 0x10*ch)
#define set_audio_length(ch, length) deo2(length, 0x3a + 0x10*ch)
#define set_audio_addr(ch, addr) deo2(addr, 0x3c + 0x10*ch)
#define set_audio_volume(ch, volume) deo(volume, 0x3e + 0x10*ch)
#define play_audio(ch, pitch) deo(pitch, 0x3f + 0x10*ch)

#define controller_button() dei(0x82)
#define controller_key() dei(0x83)

#define mouse_x() dei2(0x92)
#define mouse_y() dei2(0x94)
#define mouse_state() dei(0x96)
#define mouse_scrollx() dei2(0x9a)
#define mouse_scrolly() dei2(0x9c)

/// Read up to n bytes from file "name" into addr, then return bytes read.
#define file_read(name, n, addr) (deo2(name, 0xa8), deo2(n, 0xaa), deo2(addr, 0xac), dei2(0xa2))
#define _file_write(name, n, addr, append) (deo(append, 0xa7), deo2(name, 0xa8), deo2(n, 0xaa), deo2(addr, 0xae), dei2(0xa2))
/// Write n bytes from addr into file "name", then return bytes written.
#define file_write(name, n, addr) _file_write(name, n, addr, 0)
#define file_append(name, n, addr) _file_write(name, n, addr, 1)
#define file_delete(name) (deo2(name, 0xa8), deo(1, 0xa6))

#define datetime_year() dei2(0xc0)
#define datetime_month() dei(0xc2)
#define datetime_day() dei(0xc3)
#define datetime_hour() dei(0xc4)
#define datetime_minute() dei(0xc5)
#define datetime_second() dei(0xc6)
#define datetime_dotw() dei(0xc7)
#define datetime_doty() dei2(0xc8)
#define datetime_isdst() dei(0xca)

// Pixel values (| with color number)
// (Layer + operation + corner)
#define BgDot 0x00
#define BgFillBR 0x80
#define BgFillBL 0x90
#define BgFillTR 0xa0
#define BgFillTL 0xb0
#define FgDot 0x40
#define FgFillBR 0xc0
#define FgFillBL 0xd0
#define FgFillTR 0xe0
#define FgFillTL 0xf0

// Sprite values (| with blend number)
// (Layer + bit depth + flip axes)
#define Bg1 0x00
#define Bg1X 0x10
#define Bg1Y 0x20
#define Bg1XY 0x30
#define Fg1 0x40
#define Fg1X 0x50
#define Fg1Y 0x60
#define Fg1XY 0x70
#define Bg2 0x80
#define Bg2X 0x90
#define Bg2Y 0xa0
#define Bg2XY 0xb0
#define Fg2 0xc0
#define Fg2X 0xd0
#define Fg2Y 0xe0
#define Fg2XY 0xf0

// Auto values
#define Auto1 0x00
#define Auto1x 0x01
#define Auto1y 0x02
#define Auto1a 0x04
#define Auto1ax 0x05
#define Auto1ay 0x06
#define Auto2 0x10
#define Auto2x 0x11
#define Auto2y 0x12
#define Auto2a 0x14
#define Auto2ax 0x15
#define Auto2ay 0x16

// Button values
#define ButtonCtrl 0x01
#define ButtonA 0x01
#define ButtonAlt 0x02
#define ButtonB 0x02
#define ButtonShift 0x04
#define ButtonSelect 0x04
#define ButtonStart 0x08
#define ButtonHome 0x08
#define ButtonUp 0x10
#define ButtonDown 0x20
#define ButtonLeft 0x40
#define ButtonRight 0x80
