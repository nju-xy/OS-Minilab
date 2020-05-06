#include <game.h>
#include <klib.h>

#define min(a, b) (a < b ? a : b)
#define max(a, b) (a > b ? a : b)
#define LEFT 2
#define UP 1
#define RIGHT 4
#define DOWN 3
#define BLOOD 5
#define DIVIDE 2
const int FPS = 10;
const int DELAY1 = 3;
const int DELAY2 = 2;
const int CD = 100;
const int CD2 = 25;
uint32_t pixels[805 * 605];
int w, h;
void end();
void init_screen();
void splash();
void new_read_key();
void game_progress();
void draw_p(int x, int y, int face, int color);

struct Player{
    int x, y, x_before, y_before;
    int alive;
    int face;
    int cd;
}p1, p2;

struct Ability{
    int x, y;
    int delay;
    int face;
}a1, a2;

void init_game() {
    printf("这是一个双人游戏,每个角色可以上下左右移动,普通攻击以及特殊技能.\n普通攻击随时可以释放,特殊技能只有在第二行的怒气满之后才能释放. 每个人初始血量都是5.\n操作方法:玩家1:WASD移动,F普通攻击(周围3*3方块),G特殊技能短暂无敌.\n玩家2:上下左右移动,右SHIFT普通攻击(线性攻击),右回车朝着面向方向闪现一小段距离.\n");
    p1.x = p1.x_before = 0;
    p1.y = p1.y_before = DIVIDE;
    p2.x = p2.x_before = w / SIDE - 1;
    p2.y = p2.y_before = h / SIDE - 1;
    p1.alive = p2.alive = BLOOD;
    a1.delay = -1;
    p1.face = 4;
    p2.face = 2;
    p1.cd = 0;
    p2.cd = 0;
}

int main() {
  // Operating system is a C program    
  int next_frame = 0;
  _ioe_init();
  init_screen();
  init_game();
  while (1) {
    while (uptime() < next_frame) ; // 等待一帧的到来
    new_read_key(); // 处理键盘事件
    game_progress();          // 处理一帧游戏逻辑，更新物体的位置等
    splash();          // 重新绘制屏幕
    next_frame += 1000 / FPS; // 计算下一帧的时间
    if(!p1.alive || !p2.alive)
        break;
  }
  end();
  return 0;
}


void new_read_key() {
  _DEV_INPUT_KBD_t event = { .keycode = _KEY_NONE };
  #define KEYNAME(key) \
    [_KEY_##key] = #key,
  static const char *key_names[] = {
    _KEYS(KEYNAME)
  };
  _io_read(_DEV_INPUT, _DEVREG_INPUT_KBD, &event, sizeof(event));
  if (event.keycode != _KEY_NONE && event.keydown) {
    puts("Key pressed: ");
    puts(key_names[event.keycode]);
    puts("\n");
    //p1 move
    if(event.keycode == _KEY_A) {
        if(p1.x > 0 && !(p1.x - 1 == p2.x && p1.y == p2.y) && a1.delay <= 0) {
            p1.x_before = p1.x;
            p1.y_before = p1.y;
            p1.x--;
            p1.face = 2;
        }
    }
    else if(event.keycode == _KEY_D) {
        if(p1.x < w / SIDE - 1 && !(p1.x + 1 == p2.x && p1.y == p2.y) && a1.delay <= 0) {
            p1.x_before = p1.x;
            p1.y_before = p1.y;
            p1.x++;
            p1.face = 4;
        }
    }
    else if(event.keycode == _KEY_W) {
        if(p1.y > DIVIDE && !(p1.x == p2.x && p1.y - 1 == p2.y) && a1.delay <= 0) {
            p1.x_before = p1.x;
            p1.y_before = p1.y;
            p1.y--;
            p1.face = 1;
        }
    }
    else if(event.keycode == _KEY_S) {
        if(p1.y < h / SIDE - 1 && !(p1.x == p2.x && p1.y + 1 == p2.y) && a1.delay <= 0) {
            p1.x_before = p1.x;
            p1.y_before = p1.y;
            p1.y++;
            p1.face = 3;
        }
    }

    //p2 move
      if(event.keycode == _KEY_LEFT) {
          if(p2.x > 0 && !(p2.x - 1 == p1.x && p1.y == p2.y) && a2.delay <= 0) {
              p2.x_before = p2.x;
              p2.y_before = p2.y;
              p2.x--;
              p2.face = 2;
          }
      }
      else if(event.keycode == _KEY_RIGHT) {
          if(p2.x < w / SIDE - 1 && !(p2.x + 1 == p1.x && p1.y == p2.y) && a2.delay <= 0) {
              p2.x_before = p2.x;
              p2.y_before = p2.y;
              p2.x++;
              p2.face = 4;
          }
      }
      else if(event.keycode == _KEY_UP) {
          if(p2.y > DIVIDE && !(p1.x == p2.x && p2.y - 1 == p1.y) && a2.delay <= 0) {
              p2.x_before = p2.x;
              p2.y_before = p2.y;
              p2.y--;
              p2.face = 1;
          }
      }
      else if(event.keycode == _KEY_DOWN) {
          if(p2.y < h / SIDE - 1 && !(p1.x == p2.x && p2.y + 1 == p1.y) && a2.delay <= 0) {
              p2.x_before = p2.x;
              p2.y_before = p2.y;
              p2.y++;
              p2.face = 3;
          }
      }

      //p1 ability
      if(event.keycode == _KEY_F && a1.delay <= 0) {
          a1.x = p1.x;
          a1.y = p1.y;
          a1.delay = DELAY1;
      }

      //p2 ability
      if(event.keycode == _KEY_RSHIFT && a1.delay <= 0) {
          a2.x = p2.x;
          a2.y = p2.y;
          a2.face = p2.face;
          a2.delay = DELAY2;
      }

      //p2 special ability: flash
      if(event.keycode == _KEY_RETURN && a2.delay <= 0 && p2.cd == CD2) {
          p2.cd = 0;
          p2.x_before = p2.x;
          p2.y_before = p2.y;
          switch(p2.face) {
              case UP:
                  if(p2.y - 3 == p1.y && p1.x == p2.x)
                      p2.y = p2.y - 2;
                  else
                      p2.y = max(DIVIDE, p2.y - 3);
                  break;
              case DOWN:
                  if(p2.y + 3 == p1.y && p1.x == p2.x)
                      p2.y = p2.y + 2;
                  else
                      p2.y = min(h / SIDE - 1, p2.y + 3);
                  break;
              case LEFT:
                  if(p2.x - 3 == p1.x && p1.y == p2.y)
                      p2.x = p2.x - 2;
                  else
                      p2.x = max(0, p2.x - 3);
                  break;
              case RIGHT:
                  if(p2.x + 3 == p1.x && p1.y == p2.y)
                      p2.x = p2.x + 2;
                  else
                      p2.x = min(h / SIDE - 1, p2.x + 3);
                  break;
          }
      }

      //p1 special ability
      if(event.keycode == _KEY_G && a1.delay <= 0 && p1.cd == CD) {
          p1.cd = 0;
      }

  }
}

void init_screen() {
  _DEV_VIDEO_INFO_t info = {0};
  _io_read(_DEV_VIDEO, _DEVREG_VIDEO_INFO, &info, sizeof(info));
  w = info.width;
  h = info.height;
}

extern char font8x8_basic[128][8];

void new_draw_rect(int x, int y, int w, int h, uint32_t color) {
  //uint32_t pixels[w * h];
  _DEV_VIDEO_FBCTL_t event = {
    .x = x, .y = y, .w = w, .h = h, .sync = 1,
    .pixels = pixels,
  };
  for (int i = 0; i < w * h; i++) {
    pixels[i] = color;
  }
  _io_write(_DEV_VIDEO, _DEVREG_VIDEO_FBCTL, &event, sizeof(event));
}

static inline void draw_character(char ch, int x, int y, int color, uint32_t *pixels) {
    _DEV_VIDEO_FBCTL_t event = {
            .x = x, .y = y, .w = w, .h = h, .sync = 1,
            .pixels = pixels,
    };
    char *p = font8x8_basic[(int)ch];
    for (int i = 0; i < 8; i ++)
        for (int j = 0; j < 8; j ++)
            if ((p[i] >> j) & 1) {
                if (x + j < w && y + i < h)
                    pixels[(y + i) * w + x + j] = color;
            }
    _io_write(_DEV_VIDEO, _DEVREG_VIDEO_FBCTL, &event, sizeof(event));
}

static inline void draw_string(const char *str, int x, int y, int color) {
    //printf("%s\n", str);
    //uint32_t pixels[w * h];
    //memset(pixels, 0, sizeof(pixels));
    while (*str) {
        draw_character(*str++, x, y, color, pixels);
        //printf("%c %d %d\n", *str, x, y);
        if (x + 8 >= w) {
            y += 8; x = 0;
        } else {
            x += 8;
        }
    }
}

int color(int delay) {
    if(delay > 1) return 0xFFE384;
    else if(delay == 1) return 0xFFFF00;
    else return 0x000000;
}
void splash() {
    new_draw_rect(p1.x_before * SIDE, p1.y_before * SIDE, SIDE, SIDE, 0x000000);
    new_draw_rect(p2.x_before * SIDE, p2.y_before * SIDE, SIDE, SIDE, 0x000000);
    for(int i = p1.alive; i < w / SIDE - p2.alive; ++i) {
        new_draw_rect(i * SIDE, 0, SIDE, SIDE, 0x000000);
    }
    for(int i = 0; i < p1.alive; ++i) {
        new_draw_rect(i * SIDE, SIDE / 3, SIDE, SIDE / 3, 0x66ccff);
    }
    for(int i = w / SIDE - 1; i > w / SIDE - 1 - p2.alive; --i) {
        new_draw_rect(i * SIDE, SIDE / 3, SIDE, SIDE / 3, 0x7cfc00);
    }
    for(int i = p1.cd * 5 / CD; i < w / SIDE - p2.cd * 5 / CD2; ++i) {
        new_draw_rect(i * SIDE, SIDE, SIDE, SIDE, 0x000000);
    }
    for(int i = 0; i < p1.cd * 5 / CD; ++i) {
        new_draw_rect(i * SIDE, SIDE * 4 / 3, SIDE, SIDE / 3, 0xff0000);
    }
    for(int i = w / SIDE - 1; i > w / SIDE - 1 - p2.cd * 5 / CD2; --i) {
        new_draw_rect(i * SIDE, SIDE * 4 / 3, SIDE, SIDE / 3, 0xff0000);
    }
    if(a1.delay >= 0) {
        for (int i = max(0, a1.x - 1); i <= min(w / SIDE - 1, a1.x + 1); ++i) {
            for (int j = max(DIVIDE, a1.y - 1); j <= min(h / SIDE - 1, a1.y + 1); ++j) {
                new_draw_rect(i * SIDE, j * SIDE, SIDE, SIDE, color(a1.delay));
            }
        }
    }
    if(a2.delay >= 0) {
        switch(a2.face){
            case UP:
                for (int i = DIVIDE; i < a2.y; ++i) {
                    new_draw_rect(a2.x * SIDE + SIDE / 3, i * SIDE, SIDE / 3, SIDE, color(a2.delay));
                }
                break;
            case LEFT:
                for (int i = 0; i < a2.x; ++i) {
                    new_draw_rect(i * SIDE, a2.y * SIDE + SIDE / 3, SIDE, SIDE / 3, color(a2.delay));
                }
                break;
            case DOWN:
                for (int i = h / SIDE - 1; i > a2.y; --i) {
                    new_draw_rect(a2.x * SIDE + SIDE / 3, i * SIDE, SIDE / 3, SIDE, color(a2.delay));
                }
                break;
            case RIGHT:
                for (int i = w / SIDE - 1; i > a2.x; --i) {
                    new_draw_rect(i * SIDE, a2.y * SIDE + SIDE / 3, SIDE, SIDE / 3, color(a2.delay));
                }
                break;
        }
    }
    if(p1.alive) {
        if(p1.cd < 5)
            draw_p(p1.x, p1.y, p1.face, 0x8A2BE2);
            //new_draw_rect(p1.x * SIDE, p1.y * SIDE, SIDE, SIDE, 0x8A2BE2);
        else
            draw_p(p1.x, p1.y, p1.face, 0x66ccff);
            //new_draw_rect(p1.x * SIDE, p1.y * SIDE, SIDE, SIDE, 0x66ccff);
    }
    if(p2.alive){
        draw_p(p2.x, p2.y, p2.face, 0x7cfc00);
        //new_draw_rect(p2.x * SIDE, p2.y * SIDE, SIDE, SIDE, 0x7CFC00);
    }
}

void end(){
  int next_frame = 0;
  //while (1) {
    //while (uptime() < next_frame) ; // 等待一帧的到来
    if(!p1.alive && !p2.alive) {
        draw_string(" Tie!", 0, SIDE * 2, 0xFFFFFF);
    }
    else if(!p1.alive) {
        draw_string(" Player 2 win!", 0, SIDE * 2, 0x7CFC00);
    }
    else if(!p2.alive) {
        draw_string(" Player 1 win!", 0, SIDE * 2, 0x66ccff);
    }
    next_frame += 1000 / FPS; // 计算下一帧的时间
  //}
  while(1);
}

void game_progress(){
    if(a1.delay >= 0)
        a1.delay --;
    if(a2.delay >= 0)
        a2.delay --;
    if(p1.cd < CD)
        p1.cd ++;
    if(p2.cd < CD2)
        p2.cd ++;
    //damage
    //a1
    if(a1.delay == 0 && a1.x - 1 <= p2.x && p2.x <= a1.x + 1 && a1.y - 1 <= p2.y && p2.y <= a1.y + 1) {
        p2.alive --;
    }
    //a2
    else if(p1.cd >= 5 && a2.delay == 0 && a2.x == p1.x && a2.y < p1.y && a2.face == 3) {
        p1.alive --;
    }
    else if(p1.cd >= 5 && a2.delay == 0 && a2.x == p1.x && a2.y > p1.y && a2.face == 1) {
        p1.alive --;
    }
    else if(p1.cd >= 5 && a2.delay == 0 && a2.y == p1.y && a2.x < p1.x && a2.face == 4) {
        p1.alive --;
    }
    else if(p1.cd >= 5 && a2.delay == 0 && a2.y == p1.y && a2.x > p1.x && a2.face == 2) {
        p1.alive --;
    }
}

void draw_p(int x, int y, int face, int color) {
    int width = SIDE / 5;
    int x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    switch(face) {
        case UP: 
            x1 = width;
            x2 = 3 * width;
            y1 = y2 = width;
            break;
        case DOWN: 
            x1 = width;
            x2 = 3 * width;
            y1 = y2 = 3 * width;
            break;
        case LEFT: 
            y1 = width;
            y2 = 3 * width;
            x1 = x2 = width;
            break;
        case RIGHT: 
            y1 = width;
            y2 = 3 * width;
            x1 = x2 = 3 * width;
            break;
    }
    new_draw_rect(x * SIDE, y * SIDE, SIDE, SIDE, color);
    new_draw_rect(x * SIDE + x1, y * SIDE + y1, width, width, 0);
    new_draw_rect(x * SIDE + x2, y * SIDE + y2, width, width, 0);
}
