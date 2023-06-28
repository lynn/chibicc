#include "atari8_uf1.c"
#include <uxn.h>

char spr_circle[8] = {0b00111100, 0b01111110, 0b11111111, 0b11111111,
                      0b11111111, 0b11111111, 0b01111110, 0b00111100};

char spr_quarter[8] = {0b00000111, 0b00011111, 0b00111111, 0b01111111,
                       0b01111111, 0b11111111, 0b11111111, 0b11111111};

// char spr_bullet[8] = {0b00000000, 0b00011000, 0b00100100, 0b01011010,
//                       0b01011010, 0b00100100, 0b00011000, 0b00000000};

char spr_bullet[8] = {0b00111100, 0b01000010, 0b10000001, 0b10000001,
                      0b10000001, 0b10000001, 0b01000010, 0b00111100};

char spr_marker[8] = {0b00011000, 0b00111100, 0b01111110, 0, 0, 0, 0, 0};

char SIN_TABLE[64] = {
    0,   3,   6,   9,   12,  15,  18,  21,  24,  28,  31,  34,  37,
    40,  43,  46,  48,  51,  54,  57,  60,  63,  65,  68,  71,  73,
    76,  78,  81,  83,  85,  88,  90,  92,  94,  96,  98,  100, 102,
    104, 106, 108, 109, 111, 112, 114, 115, 117, 118, 119, 120, 121,
    122, 123, 124, 124, 125, 126, 126, 127, 127, 127, 127, 127,
};
char sin(char a) {
  return SIN_TABLE[(a & 0x40 ? ~a : a) & 0x3f] * (a & 0x80 ? -1 : 1);
}
char cos(char a) { return sin(a + 0x40); }

#define SUBPIXELS 16
#define WIDTH 200
#define HEIGHT 300

unsigned time = 0;
unsigned player_x = (WIDTH / 2 - 4) * SUBPIXELS;
unsigned player_y = (HEIGHT - 50) * SUBPIXELS;
unsigned player_alive = 1;

unsigned enemy_x = (WIDTH / 2 - 8) * SUBPIXELS;
unsigned enemy_y = 50 * SUBPIXELS;
unsigned enemy_goal_x = (WIDTH / 2 - 8) * SUBPIXELS;
unsigned enemy_goal_y = 50 * SUBPIXELS;

unsigned enemy_flash = 0;
unsigned enemy_health = 100;

typedef struct {
  unsigned x;
  unsigned y;
  int dx;
  int dy;
  int ddy;
  char glyph;
} Particle;

#define MAX_PLAYER_BULLETS 20
Particle player_bullets[MAX_PLAYER_BULLETS];
unsigned num_player_bullets = 0;

#define MAX_ENEMY_BULLETS 256
Particle enemy_bullets[MAX_ENEMY_BULLETS];
unsigned num_enemy_bullets = 0;

void update_player(void) {
  if (!player_alive)
    return;

  int speed = controller_button() & ButtonShift ? SUBPIXELS : 2 * SUBPIXELS;
  if (controller_button() & ButtonRight &&
      player_x < (WIDTH - 8 - 2) * SUBPIXELS)
    player_x += speed;
  if (controller_button() & ButtonLeft && player_x > 2 * SUBPIXELS)
    player_x -= speed;
  if (controller_button() & ButtonDown &&
      player_y < (HEIGHT - 8 - 2) * SUBPIXELS)
    player_y += speed;
  if (controller_button() & ButtonUp && player_y > 2 * SUBPIXELS)
    player_y -= speed;
  if (enemy_health && time % 8 == 0 &&
      num_player_bullets < MAX_PLAYER_BULLETS) {
    player_bullets[num_player_bullets].x = player_x;
    player_bullets[num_player_bullets].y = player_y;
    player_bullets[num_player_bullets].dx = 0;
    player_bullets[num_player_bullets].dy = -100;
    player_bullets[num_player_bullets].ddy = -2;
    player_bullets[num_player_bullets].glyph = '^';
    ++num_player_bullets;
  }
}

void update_enemy(void) {
  if (enemy_health == 0)
    return;

  if (player_alive) {
    if (time % 300 == 50) {
      enemy_goal_x = (WIDTH / 4 - 8) * SUBPIXELS;
      enemy_goal_y = 50 * SUBPIXELS;
    }
    if (time % 300 == 150) {
      enemy_goal_x = (WIDTH * 3 / 4 - 8) * SUBPIXELS;
      enemy_goal_y = 50 * SUBPIXELS;
    }
    if (time % 300 == 250) {
      enemy_goal_x = (WIDTH / 2 - 8) * SUBPIXELS;
      enemy_goal_y = 80 * SUBPIXELS;
    }
    if (time % 100 == 70 || time % 100 == 90) {
      for (int i = 0; i < 256 && num_enemy_bullets < MAX_ENEMY_BULLETS;
           i += 8) {
        enemy_bullets[num_enemy_bullets].x = enemy_x;
        enemy_bullets[num_enemy_bullets].y = enemy_y;
        enemy_bullets[num_enemy_bullets].dx = cos(i) * SUBPIXELS / 100;
        enemy_bullets[num_enemy_bullets].dy = sin(i) * SUBPIXELS / 100;
        enemy_bullets[num_enemy_bullets].ddy = 0;
        enemy_bullets[num_enemy_bullets].glyph = (i + time) % 63 + 64;
        ++num_enemy_bullets;
      }
    }
  }
  // Easing
  enemy_x = (enemy_x * 15 + enemy_goal_x) / 16;
  enemy_y = (enemy_y * 15 + enemy_goal_y) / 16;
  if (enemy_flash > 0)
    --enemy_flash;
}

void destroy_bullet(unsigned i, unsigned *num_bullets, Particle *array) {
  --*num_bullets;
  array[i].x = array[*num_bullets].x;
  array[i].y = array[*num_bullets].y;
  array[i].dx = array[*num_bullets].dx;
  array[i].dy = array[*num_bullets].dy;
  array[i].ddy = array[*num_bullets].ddy;
  array[i].glyph = array[*num_bullets].glyph;
}

void player_explode(void) {
  player_alive = 0;
  for (int i = 0; i < 256 && num_player_bullets < MAX_PLAYER_BULLETS; i += 24) {
    player_bullets[num_player_bullets].x = player_x;
    player_bullets[num_player_bullets].y = player_y;
    player_bullets[num_player_bullets].dx = cos(i) * SUBPIXELS / 100;
    player_bullets[num_player_bullets].dy =
        (int)(sin(i) * SUBPIXELS / 100) - 16;
    player_bullets[num_player_bullets].ddy = 1;
    player_bullets[num_player_bullets].glyph = 'X';
    ++num_player_bullets;
  }
}

void update_bullets(void) {
  for (unsigned i = 0; i < num_player_bullets; ++i) {
    player_bullets[i].x += player_bullets[i].dx;
    player_bullets[i].y += player_bullets[i].dy;
    player_bullets[i].dy += player_bullets[i].ddy;
    if (player_bullets[i].x < 2 * SUBPIXELS ||
        player_bullets[i].x > (WIDTH - 8 - 2) * SUBPIXELS ||
        player_bullets[i].y < 2 * SUBPIXELS ||
        player_bullets[i].y > (HEIGHT - 8 - 2) * SUBPIXELS) {
      destroy_bullet(i, &num_player_bullets, player_bullets);
    }
    if (enemy_health && player_bullets[i].x >= enemy_x - 7 * SUBPIXELS &&
        player_bullets[i].x < enemy_x + 16 * SUBPIXELS &&
        player_bullets[i].y >= enemy_y - 7 * SUBPIXELS &&
        player_bullets[i].y < enemy_y + 16 * SUBPIXELS) {
      destroy_bullet(i, &num_player_bullets, player_bullets);
      enemy_flash = 2;
      enemy_health -= 2;
      if (enemy_health == 0) {
        for (int j = 0; j < num_enemy_bullets; j++) {
          enemy_bullets[j].ddy = 2;
        }
      }
    }
  }
  for (unsigned i = 0; i < num_enemy_bullets; ++i) {
    enemy_bullets[i].x += enemy_bullets[i].dx;
    enemy_bullets[i].y += enemy_bullets[i].dy;
    enemy_bullets[i].dy += enemy_bullets[i].ddy;
    if (enemy_bullets[i].x < 2 * SUBPIXELS ||
        enemy_bullets[i].x > (WIDTH - 8 - 2) * SUBPIXELS ||
        enemy_bullets[i].y < 2 * SUBPIXELS ||
        enemy_bullets[i].y > (HEIGHT - 8 - 2) * SUBPIXELS) {
      destroy_bullet(i, &num_enemy_bullets, enemy_bullets);
    }
    if (player_alive && enemy_health &&
        enemy_bullets[i].x >= player_x - 7 * SUBPIXELS &&
        enemy_bullets[i].x < player_x + 8 * SUBPIXELS &&
        enemy_bullets[i].y >= player_y - 7 * SUBPIXELS &&
        enemy_bullets[i].y < player_y + 8 * SUBPIXELS) {
      destroy_bullet(i, &num_enemy_bullets, enemy_bullets);
      player_explode();
    }
  }
}

void undraw(void) {
  set_screen_xy(0, 0);
  draw_pixel(FgFillBR);
}

void display_message(char *msg) {
  int len = 0;
  char *p = msg;
  while (*p)
    ++p, ++len;
  set_screen_xy(WIDTH / 2 - len * 4, HEIGHT / 2 - 4);
  set_screen_auto(Auto1x);
  for (p = msg; *p; ++p) {
    set_screen_addr(atari8_uf1 + 256 + *p * 8);
    draw_sprite(Fg1 | 3);
  }
  set_screen_auto(0);
}

void draw(void) {
  for (unsigned i = 0; i < num_player_bullets; ++i) {
    set_screen_xy(player_bullets[i].x / SUBPIXELS,
                  player_bullets[i].y / SUBPIXELS);
    set_screen_addr(atari8_uf1 + 256 + player_bullets[i].glyph * 8);
    draw_sprite(Fg1 | 2);
  }
  for (unsigned i = 0; i < num_enemy_bullets; ++i) {
    set_screen_xy(enemy_bullets[i].x / SUBPIXELS,
                  enemy_bullets[i].y / SUBPIXELS);
    set_screen_addr(atari8_uf1 + 256 + enemy_bullets[i].glyph * 8);
    draw_sprite(Fg1 | (enemy_health ? 1 : 2));
  }

  if (player_alive) {
    set_screen_xy(player_x / SUBPIXELS, player_y / SUBPIXELS);
    set_screen_addr(spr_circle);
    draw_sprite(Fg1 | 2);
  }

  if (enemy_health > 0) {
    set_screen_addr(spr_quarter);
    set_screen_xy(enemy_x / SUBPIXELS, enemy_y / SUBPIXELS);
    int color = enemy_flash ? 3 : 1;
    draw_sprite(Fg1 | color);
    set_screen_xy(enemy_x / SUBPIXELS + 8, enemy_y / SUBPIXELS);
    draw_sprite(Fg1X | color);
    set_screen_xy(enemy_x / SUBPIXELS, enemy_y / SUBPIXELS + 8);
    draw_sprite(Fg1Y | color);
    set_screen_xy(enemy_x / SUBPIXELS + 8, enemy_y / SUBPIXELS + 8);
    draw_sprite(Fg1XY | color);
    set_screen_addr(spr_marker);
    set_screen_xy(enemy_x / SUBPIXELS + 4, HEIGHT - 8);
    draw_sprite(Fg1 | 3);
  } else {
    display_message("You win!");
  }

  if (!player_alive) {
    display_message("Game over");
  }

  set_screen_addr(atari8_uf1 + 256 + ('0' + num_enemy_bullets / 100 % 10) * 8);
  set_screen_xy(2, 2);
  draw_sprite(Fg1 | 3);
  set_screen_addr(atari8_uf1 + 256 + ('0' + num_enemy_bullets / 10 % 10) * 8);
  set_screen_xy(10, 2);
  draw_sprite(Fg1 | 3);
  set_screen_addr(atari8_uf1 + 256 + ('0' + num_enemy_bullets % 10) * 8);
  set_screen_xy(18, 2);
  draw_sprite(Fg1 | 3);
  for (int y = 2; y < 5; y++) {
    set_screen_y(y);
    for (int i = 0; i < 100; i++) {
      set_screen_x(WIDTH / 2 - 50 + i);
      draw_pixel(enemy_health > i ? 1 : 0);
    }
  }
}

void on_screen(void) {
  ++time;
  undraw();
  update_player();
  update_enemy();
  update_bullets();
  draw();
}

void main(void) {
  set_palette(0x0f2f, 0x0acf, 0x40ff);
  set_screen_size(WIDTH, HEIGHT);
}
