#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include "font.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>

static const int SDL_WAKEUPEVENT = SDL_USEREVENT+1;

#ifndef TARGET_RETROFW
	#define system(x) printf(x); printf("\n")
#endif

#ifndef TARGET_RETROFW
	#define DBG(x) printf("%s:%d %s %s\n", __FILE__, __LINE__, __func__, x);
#else
	#define DBG(x)
#endif
#define IMAGE_PATH		"backdrop.png"
#define GPIO_BASE		0x10010000
#define PAPIN			((0x10010000 - GPIO_BASE) >> 2)
#define PBPIN			((0x10010100 - GPIO_BASE) >> 2)
#define PCPIN			((0x10010200 - GPIO_BASE) >> 2)
#define PDPIN			((0x10010300 - GPIO_BASE) >> 2)
#define PEPIN			((0x10010400 - GPIO_BASE) >> 2)
#define PFPIN			((0x10010500 - GPIO_BASE) >> 2)

#ifndef WIDTH
#define WIDTH  320
#endif
#ifndef HEIGHT
#define HEIGHT 240
#endif

#ifndef ALTERNATE_KEYMAP
#define BTN_NORTH		SDLK_SPACE
#define BTN_EAST		SDLK_LCTRL
#define BTN_SOUTH		SDLK_LALT
#define BTN_WEST		SDLK_LSHIFT
#define BTN_L1			SDLK_TAB
#define BTN_R1			SDLK_BACKSPACE
#define BTN_L2			SDLK_RALT   // definition from miyoo_kbd.c
#define BTN_R2			SDLK_RSHIFT // definition from miyoo_kbd.c
#define BTN_START		SDLK_RETURN
#define BTN_SELECT		SDLK_ESCAPE
#define BTN_BACKLIGHT	SDLK_3
#define BTN_POWER		SDLK_END
#define BTN_UP			SDLK_UP
#define BTN_DOWN		SDLK_DOWN
#define BTN_LEFT		SDLK_LEFT
#define BTN_RIGHT		SDLK_RIGHT
#define BTN_RESET		SDLK_RCTRL
#define GPIO_TV			SDLK_WORLD_0
#define GPIO_MMC		SDLK_WORLD_1
#define GPIO_USB		SDLK_WORLD_2
#define GPIO_PHONES		SDLK_WORLD_3
#endif

//Rectangle dimensions for highlighting components
//      NAME                  X,   Y,   W,   H
#ifndef RECT_BTN_NORTH
#define RECT_BTN_NORTH		  0,   0,   0,   0
#endif
#ifndef RECT_BTN_EAST
#define RECT_BTN_EAST		  0,   0,   0,   0
#endif
#ifndef RECT_BTN_SOUTH
#define RECT_BTN_SOUTH		  0,   0,   0,   0
#endif
#ifndef RECT_BTN_WEST
#define RECT_BTN_WEST		  0,   0,   0,   0
#endif
#ifndef RECT_BTN_L1
#define RECT_BTN_L1			  0,   0,   0,   0
#endif
#ifndef RECT_BTN_L2
#define RECT_BTN_L2			  0,   0,   0,   0
#endif
#ifndef RECT_BTN_R1
#define RECT_BTN_R1			  0,   0,   0,   0
#endif
#ifndef RECT_BTN_R2
#define RECT_BTN_R2			  0,   0,   0,   0
#endif
#ifndef RECT_BTN_START
#define RECT_BTN_START		  0,   0,   0,   0
#endif
#ifndef RECT_BTN_SELECT
#define RECT_BTN_SELECT		  0,   0,   0,   0
#endif
#ifndef RECT_BTN_BACKLIGHT
#define RECT_BTN_BACKLIGHT	  0,   0,   0,   0
#endif
#ifndef RECT_BTN_POWER
#define RECT_BTN_POWER		  0,   0,   0,   0
#endif
#ifndef RECT_BTN_UP
#define RECT_BTN_UP           0,   0,   0,   0
#endif
#ifndef RECT_BTN_DOWN
#define RECT_BTN_DOWN         0,   0,   0,   0
#endif
#ifndef RECT_BTN_LEFT
#define RECT_BTN_LEFT         0,   0,   0,   0
#endif
#ifndef RECT_BTN_RIGHT
#define RECT_BTN_RIGHT        0,   0,   0,   0
#endif
#ifndef RECT_BTN_RESET
#define RECT_BTN_RESET        0,   0,   0,   0
#endif
#ifndef RECT_GPIO_TV
#define RECT_GPIO_TV		  0,   0,   0,   0
#endif
#ifndef RECT_GPIO_MMC
#define RECT_GPIO_MMC		  0,   0,   0,   0
#endif
#ifndef RECT_GPIO_USB
#define RECT_GPIO_USB		  0,   0,   0,   0
#endif
#ifndef RECT_GPIO_PHONES
#define RECT_GPIO_PHONES	  0,   0,   0,   0
#endif

const int	HAlignLeft		= 1,
			HAlignRight		= 2,
			HAlignCenter	= 4,
			VAlignTop		= 8,
			VAlignBottom	= 16,
			VAlignMiddle	= 32;



SDL_RWops *rw;
TTF_Font *font = NULL;
SDL_Surface *screen = NULL;
SDL_Surface* img = NULL;
SDL_Rect bgrect;
SDL_Event event;

SDL_Color txtColor = {200, 200, 220};
SDL_Color titleColor = {200, 200, 0};
SDL_Color subTitleColor = {0, 200, 0};
SDL_Color powerColor = {200, 0, 0};

volatile uint32_t *memregs;
volatile uint8_t memdev = 0;
uint16_t mmcPrev, mmcStatus;
uint16_t udcPrev, udcStatus;
uint16_t tvOutPrev, tvOutStatus;
uint16_t phonesPrev, phonesStatus;

static char buf[1024];
uint32_t *mem;

uint8_t *keys;

extern uint8_t rwfont[];

int draw_text(int x, int y, const char buf[64], SDL_Color txtColor, int align) {
	DBG("");

	SDL_Surface *msg = TTF_RenderText_Blended(font, buf, txtColor);

	if (align & HAlignCenter) {
		x -= msg->w / 2;
	} else if (align & HAlignRight) {
		x -= msg->w;
	}

	if (align & VAlignMiddle) {
		y -= msg->h / 2;
	} else if (align & VAlignTop) {
		y -= msg->h;
	}

	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = msg->w;
	rect.h = msg->h;
	SDL_BlitSurface(msg, NULL, screen, &rect);
	SDL_FreeSurface(msg);
	return msg->w;
}

void draw_background(const char buf[64]) {
	DBG("");
	bgrect.w = img->w;
	bgrect.h = img->h;
	bgrect.x = IMAGE_X;
	bgrect.y = IMAGE_Y;
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
	SDL_BlitSurface(img, NULL, screen, &bgrect);

	/* // title */
	draw_text(TITLE_X, TITLE_Y, TITLE_STRING, titleColor, VAlignBottom | HAlignCenter);
	/* draw_text(10, 4, buf, titleColor, VAlignBottom); */
	draw_text(EXIT_X, EXIT_Y, EXIT_STRING, txtColor, VAlignMiddle | HAlignLeft);
}

void draw_point(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
	// DBG("");
	SDL_Rect rect;
	rect.w = w;
	rect.h = h;
	rect.x = x + bgrect.x;
	rect.y = y + bgrect.y;
	SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 0, 150, 0));
}

void quit(int err) {
	DBG("");
	system("sync");
	if (font) TTF_CloseFont(font);
	font = NULL;
	SDL_Quit();
	TTF_Quit();
	exit(err);
}

uint16_t getMMCStatus() {
	return (memdev > 0 && !(memregs[0x10500 >> 2] >> 0 & 0b1));
}

uint16_t getUDCStatus() {
	return (memdev > 0 && (memregs[0x10300 >> 2] >> 7 & 0b1));
}

uint16_t getTVOutStatus() {
	return (memdev > 0 && !(memregs[0x10300 >> 2] >> 25 & 0b1));
}

uint16_t getPhonesStatus() {
	return (memdev > 0 && !(memregs[0x10300 >> 2] >> 6 & 0b1));
}

void pushEvent() {
	SDL_Event user_event;
	user_event.type = SDL_KEYUP;
	SDL_PushEvent(&user_event);
}


static int hw_input(void *ptr)
{
	while (1) {
		udcStatus = getUDCStatus();
		if (udcPrev != udcStatus) {
			keys[GPIO_USB] = udcPrev = udcStatus;
			pushEvent();
		}
		mmcStatus = getMMCStatus();
		if (mmcPrev != mmcStatus) {
			keys[GPIO_MMC] = mmcPrev = mmcStatus;
			pushEvent();
		}

		tvOutStatus = getTVOutStatus();
		if (tvOutPrev != tvOutStatus) {
			keys[GPIO_TV] = tvOutPrev = tvOutStatus;
			pushEvent();
		}

		phonesStatus = getPhonesStatus();
		if (phonesPrev != phonesStatus) {
			keys[GPIO_PHONES] = phonesPrev = phonesStatus;
			pushEvent();
		}

		SDL_Delay(100);
	}

	return 0;
}

int main(int argc, char* argv[]) {
	DBG("");
	signal(SIGINT, &quit);
	signal(SIGSEGV,&quit);
	signal(SIGTERM,&quit);

	char title[64] = "";
	keys = SDL_GetKeyState(NULL);

	/* sprintf(title, "IO TESTEN"); */
	/* printf("%s\n", title); */

	setenv("SDL_NOMOUSE", "1", 1);

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		return -1;
	}
	SDL_PumpEvents();
	SDL_ShowCursor(0);

	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, SDL_SWSURFACE);

	SDL_EnableKeyRepeat(1, 1);

	if (TTF_Init() == -1) {
		printf("failed to TTF_Init\n");
		return -1;
	}
	rw = SDL_RWFromMem(rwfont, sizeof(rwfont));
	font = TTF_OpenFontRW(rw, 1, 8);
	TTF_SetFontHinting(font, TTF_HINTING_NORMAL);
	TTF_SetFontOutline(font, 0);

	SDL_Surface* _img = IMG_Load(IMAGE_PATH);
	img = SDL_DisplayFormat(_img);
	SDL_FreeSurface(_img);

#if defined(TARGET_RETROFW)
	memdev = open("/dev/mem", O_RDWR);
	if (memdev > 0) {
		memregs = (uint32_t*)mmap(0, 0x20000, PROT_READ | PROT_WRITE, MAP_SHARED, memdev, 0x10000000);
	
		SDL_Thread *thread = SDL_CreateThread(hw_input, (void *)NULL);
	
		if (memregs == MAP_FAILED) {
			close(memdev);
		}
	}
#endif

	int loop = 1, running = 0;
	do {
		draw_background(title);

		int nextline = 0;	

		if (event.key.keysym.sym) {
			sprintf(buf, "Last key: %s", SDL_GetKeyName(event.key.keysym.sym));
			draw_text(TEXT_X, TEXT_Y + nextline, buf, subTitleColor, VAlignBottom);
			nextline += LINE_SPACING;

			sprintf(buf, "Keysym.sym: %d", event.key.keysym.sym);
			draw_text(TEXT_X, TEXT_Y + nextline, buf, subTitleColor, VAlignBottom);
			nextline += LINE_SPACING;

			sprintf(buf, "Keysym.scancode: %d", event.key.keysym.scancode);
			draw_text(TEXT_X, TEXT_Y + nextline, buf, subTitleColor, VAlignBottom);
			nextline += LINE_SPACING;
		}

		if (udcStatus) {
			draw_point(RECT_GPIO_USB);
			draw_text(TEXT_X, TEXT_Y + nextline, "USB Connected", subTitleColor, VAlignBottom);
			nextline += LINE_SPACING;
		}
		if (tvOutStatus) {
			draw_point(RECT_GPIO_TV);
			draw_text(TEXT_X, TEXT_Y + nextline, "TV-Out Connected", subTitleColor, VAlignBottom);
			nextline += LINE_SPACING;
		}
		if (mmcStatus) {
			draw_point(RECT_GPIO_MMC);
			draw_text(TEXT_X, TEXT_Y + nextline, "SD Card Connected", subTitleColor, VAlignBottom);
			nextline += LINE_SPACING;
		}

		if (phonesStatus) {
			draw_point(RECT_GPIO_PHONES);
			draw_text(TEXT_X, TEXT_Y + nextline, "Phones Connected", subTitleColor, VAlignBottom);
			nextline += LINE_SPACING;
		}

		// if (keys[BTN_SELECT] && keys[BTN_START]) loop = 0;
		if (keys[BTN_START]) draw_point(RECT_BTN_START);
		if (keys[BTN_SELECT]) draw_point(RECT_BTN_SELECT);
		if (keys[BTN_POWER]) draw_point(RECT_BTN_POWER);
		if (keys[BTN_BACKLIGHT]) draw_point(RECT_BTN_BACKLIGHT);
		if (keys[BTN_L1]) draw_point(RECT_BTN_L1);
		if (keys[BTN_R1]) draw_point(RECT_BTN_R1);
		if (keys[BTN_LEFT]) draw_point(RECT_BTN_LEFT);
		if (keys[BTN_RIGHT]) draw_point(RECT_BTN_RIGHT);
		if (keys[BTN_UP]) draw_point(RECT_BTN_UP);
		if (keys[BTN_DOWN]) draw_point(RECT_BTN_DOWN);
		if (keys[BTN_EAST]) draw_point(RECT_BTN_EAST);
		if (keys[BTN_SOUTH]) draw_point(RECT_BTN_SOUTH);
		if (keys[BTN_NORTH]) draw_point(RECT_BTN_NORTH);
		if (keys[BTN_WEST]) draw_point(RECT_BTN_WEST);
		if (keys[BTN_L2]) draw_point(RECT_BTN_L2);
		if (keys[BTN_R2]) draw_point(RECT_BTN_R2);
		if (keys[BTN_RESET]) draw_point(RECT_BTN_RESET);
		
		SDL_Flip(screen);

		while (SDL_WaitEvent(&event)) {
			if (event.type == SDL_KEYDOWN) {
				if (keys[BTN_SELECT] && keys[BTN_START]) loop = 0;
				break;
			}

			if (event.type == SDL_KEYUP) {
				break;
			}
		}
	} while (loop);

	if (memdev > 0) close(memdev);

	quit(0);
	return 0;
}
