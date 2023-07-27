#include <varvara.h>

#define PADDLE_WIDTH 16
#define PADDLE_HEIGHT 24
#define PADDLE_COLOR (Fg2 | 5)
#define PADDLE_SPEED 1
#define BALL_SIZE 16
#define BALL_COLOR (Fg2 | 5)
#define BALL_POSITIVE_SPEED 1
#define BALL_NEGATIVE_SPEED (-1)
#define CLEAR_COLOR 0x40
#define MARGIN 0x10
#define WALL_MARGIN 16

typedef struct {
  int x;
  int y;
} Paddle;

Paddle left;
Paddle right;

struct {
  int x;
  int y;
  int speed_x;
  int speed_y;
} ball;

char tile_background[8] = {0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88};

char paddle_sprite[3][2][2][8] = {
    {{{0x3f, 0x7f, 0xe7, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3},
      {0x00, 0x00, 0x18, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c}},
     {{0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06}}},
    {{{0xc3, 0xc3, 0xc3, 0xc3, 0xe7, 0xff, 0xff, 0xff},
      {0x3c, 0x3c, 0x3c, 0x3c, 0x18, 0x00, 0x00, 0x00}},
     {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
      {0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06}}},
    {{{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x3f},
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
     {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfc},
      {0x06, 0x06, 0x06, 0x06, 0x06, 0x1e, 0x3c, 0x00}}},
};

char ball_sprite[2][2][2][8] = {
    {{{0x03, 0x0f, 0x1f, 0x39, 0x70, 0x70, 0xf9, 0xff},
      {0x00, 0x00, 0x00, 0x06, 0x0f, 0x0f, 0x06, 0x00}},
     {{0xc0, 0xf0, 0xf8, 0xfc, 0xfe, 0xfe, 0xff, 0xff},
      {0x00, 0x00, 0x00, 0x00, 0x08, 0x0c, 0x06, 0x06}}},
    {{{0xff, 0xff, 0x7f, 0x7f, 0x3f, 0x1f, 0x0f, 0x03},
      {0x00, 0x00, 0x00, 0x00, 0x18, 0x0f, 0x01, 0x00}},
     {{0xff, 0xff, 0xfe, 0xfe, 0xfc, 0xf8, 0xf0, 0xc0},
      {0x06, 0x06, 0x0c, 0x1c, 0x38, 0xf0, 0xc0, 0x00}}},
};

void draw_background(void) {
  set_screen_addr(tile_background);
  int max_y = screen_height() - WALL_MARGIN;
  int max_x = screen_width();
  for (int y = WALL_MARGIN; y < max_y; y += 8) {
    for (int x = 0; x < max_x; x += 8) {
      set_screen_xy(x, y);
      draw_sprite(3);
    }
  }
}

void reset(void) {
  ball.x = (screen_width() - BALL_SIZE) / 2;
  ball.y = (screen_height() - BALL_SIZE) / 2;
}

void update_ball(void) {
  ball.x += ball.speed_x;
  ball.y += ball.speed_y;
  if (ball.y < WALL_MARGIN)
    ball.speed_y = BALL_POSITIVE_SPEED;
  else if (ball.y + BALL_SIZE > screen_height() - WALL_MARGIN)
    ball.speed_y = BALL_NEGATIVE_SPEED;

  // Check left paddle
  if (ball.x < MARGIN + PADDLE_WIDTH) {
    if (ball.y > left.y - BALL_SIZE && ball.y < left.y + PADDLE_HEIGHT) {
      ball.speed_x = BALL_POSITIVE_SPEED;
    } else if (ball.x <= 0) {
      // ++right.score;
      reset();
    }
  }

  // Check right paddle
  else if (ball.x + BALL_SIZE > screen_width() - MARGIN - PADDLE_WIDTH) {
    if (ball.y > right.y - BALL_SIZE && ball.y < right.y + PADDLE_HEIGHT) {
      ball.speed_x = BALL_NEGATIVE_SPEED;
    } else if (ball.x >= screen_width()) {
      // ++left.score;
      reset();
    }
  }
}

void update_paddles(void) {
  char button = controller_button();
  if (button & ButtonUp)
    left.y -= PADDLE_SPEED;
  if (button & ButtonDown)
    left.y += PADDLE_SPEED;
  if (button & ButtonA)
    right.y -= PADDLE_SPEED;
  if (button & ButtonB)
    right.y += PADDLE_SPEED;
}

void draw_ball(char color) {
  set_screen_xy(ball.x, ball.y);
  set_screen_addr(ball_sprite);
  set_screen_auto(Auto2ay);
  draw_sprite(color);
  draw_sprite(color);
}

void draw_paddle(int x, int y, char color) {
  set_screen_xy(x, y);
  set_screen_addr(paddle_sprite);
  set_screen_auto(Auto2ay);
  draw_sprite(color);
  draw_sprite(color);
  draw_sprite(color);
}

void on_screen(void) {
  draw_paddle(left.x, left.y, CLEAR_COLOR);
  draw_paddle(right.x, right.y, CLEAR_COLOR);
  draw_ball(CLEAR_COLOR);
  update_paddles();
  update_ball();
  draw_paddle(left.x, left.y, PADDLE_COLOR);
  draw_paddle(right.x, right.y, PADDLE_COLOR);
  draw_ball(BALL_COLOR);
}

void main(void) {
  set_palette(0x2ce9, 0x01c0, 0x2ce5);
  draw_background();
  left.x = MARGIN;
  right.x = screen_width() - MARGIN - PADDLE_WIDTH;
  left.y = right.y = (screen_height() - PADDLE_HEIGHT) / 2;
  reset();
  ball.speed_x = BALL_NEGATIVE_SPEED;
  ball.speed_y = BALL_POSITIVE_SPEED;
}
