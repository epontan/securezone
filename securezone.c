/* SecureZone - A movie inspired screen lock/saver written in C and SDL
 *
 * Copyright 2010 Pontus Andersson [pesa]
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <shadow.h>
#include <SDL.h>
#include <SDL_image.h>

#define PREFIX _PATH_PREFIX
#define MAX_WILDCARDS 32

SDL_Surface *screen, *message, *granted, *denied;
SDL_Rect inputfield;
SDL_Rect wildcards[MAX_WILDCARDS];
Uint32 bgcolor, fgcolor;
char input[256];
int inputlen, activated;
const char *pwdhash;

void exit_error(const char *error_str, ...);
int retreive_pwdhash(void);
int event_loop(SDL_Event *ev);
void init_graphics(void);
void draw_background(int direct);
void draw_message(int direct);
void draw_inputfield(int direct);
void draw_granted(int direct);
void draw_denied(int direct);
void position_wildcards(int direct);
void update_inputfield(void);
void update_screen(void);
int handle_keyevent(SDL_KeyboardEvent *ev);
int check_input(void);

int main(int argc, char **argv)
{
	SDL_VideoInfo *vinfo;
	SDL_Event ev;
	int width, height;
	int i;
	
	if(argc == 2 && strcmp(argv[1], "-v") == 0) {
		printf("securezone-%s, Copyright 2010 Pontus Andersson [pesa]\n", VERSION);
		exit(EXIT_SUCCESS);
	}

	if(!retreive_pwdhash())
		exit_error("Unable to get shadow entry. Make sure to suid!\n");

	if(argc == 2 && strcmp(argv[1], "-b") == 0)
		activated = 0;
	else
		activated = 1;

	inputlen = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		exit_error("Could not init SDL video\n");

	SDL_ShowCursor(SDL_DISABLE);
	SDL_EnableUNICODE(SDL_ENABLE);

	vinfo = (SDL_VideoInfo*)SDL_GetVideoInfo();
	width = vinfo->current_w;
	height = vinfo->current_h;

	if((screen = SDL_SetVideoMode(width, height, 32, SDL_SWSURFACE)) == NULL)
		exit_error("Unable to set surface: %s\n", SDL_GetError());

	bgcolor = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
	fgcolor = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff);

	SDL_WM_ToggleFullScreen(screen);
	draw_background(0);
	update_screen();
	
	if(activated)
		init_graphics();

	while(event_loop(&ev));

	for(i = 0; i < inputlen; i++)
		input[i] = '\0';

	SDL_Quit();
	return EXIT_SUCCESS;
}

void exit_error(const char *error_str, ...) {
	va_list ap;
	va_start(ap, error_str);
	fprintf(stderr, "ERROR: ");
	vfprintf(stderr, error_str, ap);
	va_end(ap);
	SDL_Quit();
	exit(EXIT_FAILURE);
}

int retreive_pwdhash(void) {
	struct spwd *sp;

	if(geteuid() != 0)
		return 0;

	sp = getspnam(getenv("USER"));
	endspent();
	pwdhash = sp->sp_pwdp;

	return 1;
}

int event_loop(SDL_Event *ev) {
	if(!SDL_PollEvent(ev))
		return 1;

	if(ev->type == SDL_KEYDOWN) {
		if(activated)
			return handle_keyevent(&ev->key);
		else
			init_graphics();
	}

	return 1;
}

void init_graphics(void)
{
	draw_message(1);
	draw_inputfield(1);
	position_wildcards(1);
	activated = 1;
}

void draw_background(int direct)
{
	SDL_Rect bg;
	bg.x = 0; bg.y = 0;
	bg.w = screen->w; bg.h = screen->h;
	SDL_FillRect(screen, &bg, bgcolor);

	if(direct)
		update_screen();
}

void draw_message(int direct)
{
	SDL_Rect dest;
	SDL_RWops *rwop;
	rwop = SDL_RWFromFile(PREFIX"/share/securezone/authreq_enterpasswd.png", "rb");
	message = IMG_LoadPNG_RW(rwop);
	if(!message) {
		printf("IMG_LoadPNG_RW: %s\n", IMG_GetError());
		exit(EXIT_FAILURE);
	}
	dest.x = (screen->w * .5) - (message->w * .5);
	dest.y = (screen->h * .5) - (message->h * 2);
	SDL_BlitSurface(message, NULL, screen, &dest);

	if(direct)
		update_screen();
}

void draw_inputfield(int direct)
{
	SDL_Rect border;
	int weight = 3;
	border.w = screen->w - (screen->w * .25);
	border.h = screen->h * .05;
	border.x = (screen->w * .5) - (border.w * .5);
	border.y = (screen->h * .5) - (border.h * .5);
	inputfield.w = border.w - (weight * 2);
	inputfield.h = border.h - (weight * 2);
	inputfield.x = border.x + weight;
	inputfield.y = border.y + weight;
	SDL_FillRect(screen, &border, fgcolor);
	SDL_FillRect(screen, &inputfield, bgcolor);

	if(direct)
		update_screen();
}

void position_wildcards(int direct)
{
	SDL_Rect *wc;
	int i, size, y;

	size = (inputfield.w / MAX_WILDCARDS) - (inputfield.w * .02);
	y = inputfield.y + ((inputfield.h * .5) - (size * .5)); 

	for(i = 0; i < MAX_WILDCARDS; i++) {
		wc = &wildcards[i];
		wc->w = size; wc->h = size;
		wc->y = y;
		wc->x = inputfield.x + (size * 3) + ((size * 2.8) * i);
		SDL_FillRect(screen, wc, bgcolor);
	}

	if(direct)
		update_screen();
}

void draw_granted(int direct)
{
	SDL_Rect dest;
	SDL_RWops *rwop;
	rwop = SDL_RWFromFile(PREFIX"/share/securezone/access_granted.png", "rb");
	granted = IMG_LoadPNG_RW(rwop);
	if(!granted) {
		printf("IMG_LoadPNG_RW: %s\n", IMG_GetError());
		exit(EXIT_FAILURE);
	}
	dest.w = screen->w;
	dest.h = granted->h;
	dest.x = (screen->w * .5) - (granted->w * .5);
	dest.y = (screen->h * .5) + (granted->h * 2);
	SDL_FillRect(screen, &dest, bgcolor);
	SDL_BlitSurface(granted, NULL, screen, &dest);

	if(direct)
		update_screen();

}

void draw_denied(int direct)
{
	SDL_Rect dest;
	SDL_RWops *rwop;
	rwop = SDL_RWFromFile(PREFIX"/share/securezone/access_denied.png", "rb");
	denied = IMG_LoadPNG_RW(rwop);
	if(!denied) {
		printf("IMG_LoadPNG_RW: %s\n", IMG_GetError());
		exit(EXIT_FAILURE);
	}
	dest.w = screen->w;
	dest.h = denied->h;
	dest.x = (screen->w * .5) - (denied->w * .5);
	dest.y = (screen->h * .5) + (denied->h * 2);
	SDL_FillRect(screen, &dest, bgcolor);
	SDL_BlitSurface(denied, NULL, screen, &dest);

	if(direct)
		update_screen();

}

void update_inputfield(void)
{
	int i, len;

	len = (inputlen % MAX_WILDCARDS) + 1;

	for(i = 0; i < len; i++)
		SDL_FillRect(screen, &wildcards[i], fgcolor);

	if(len < MAX_WILDCARDS)
		for(i = inputlen; i < MAX_WILDCARDS; i++)
			SDL_FillRect(screen, &wildcards[i], bgcolor);
	
	update_screen();
}

void update_screen(void)
{
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

int handle_keyevent(SDL_KeyboardEvent *ev)
{
	if ((ev->keysym.unicode & 0xFF80) == 0) {
		char c = (ev->keysym.unicode) & 0x7F;
		if(c > 0x1F && c < 0x7F) {
			input[inputlen++] = c;
			update_inputfield();
		} else {
			if(ev->keysym.sym == SDLK_RETURN) {
				return check_input();
			} else if(ev->keysym.sym == SDLK_BACKSPACE) {
				inputlen = inputlen > 0 ? inputlen - 1 : 0;
				update_inputfield();
			} else if(ev->keysym.sym == SDLK_ESCAPE) {
				inputlen = 0;
				update_inputfield();
			}
		}
	}

	return 1;
}

int check_input(void)
{
	input[inputlen] = '\0';

	if(strcmp(crypt(input, pwdhash), pwdhash) == 0) {
		draw_granted(1);
		sleep(1);
		return 0;
	} else {
		draw_denied(1);
	}

	inputlen = 0;

	return 1;
}
