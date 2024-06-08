#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <SDL.h>
#include "font8x8_basic.h"
#include "vga256.h"

#define GPU_W 1920
#define GPU_H 1080

#define W 800
#define H 600

#define GLYPH_W 8
#define GLYPH_H 8

#define GLYPH_SCALE 1

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define rmask 0xff000000
#define gmask 0x00ff0000
#define bmask 0x0000ff00
#define amask 0x000000ff
#else
#define rmask 0x000000ff
#define gmask 0x0000ff00
#define bmask 0x00ff0000
#define amask 0xff000000
#endif

static int fd_tty = -1;
static int console_width = 80;
static int console_height = 40;
static int console_lock_size = 0;
static int tty_number = 2;

static int lctrl = 0;
static int rctrl = 0;
static int lshift = 0;
static int rshift = 0;
int redraw = 1;

static SDL_Texture *font_texture;

#define CONSOLE_LEN (4 + console_width * console_height * 2)

/*
SDL_Surface *back_surface;
*/

SDL_Texture *gpu_texture;

void render8x8(SDL_Surface *surface, int surface_x, int surface_y, unsigned char *bitmap)
{
    int x,y;
    int set;
    int color;
    Uint32 *p = (Uint32*)surface->pixels + (surface_y * (8*16) + surface_x);

    for (x=0; x < 8; x++) {
        for (y=0; y < 8; y++) {
            set = bitmap[x] & 1 << y;
            color = set ? 0xffffffff : 0xff00ffff;
            p[x * (8*16) + y] = color;
        }
    }
}

SDL_Surface *make_font()
{
    SDL_Surface *result = SDL_CreateRGBSurfaceWithFormat(0, 16 * 8, 16 * 8, 32, SDL_PIXELFORMAT_RGBA8888);
    for (int c = 0; c < 127; c++) {
        render8x8(result, (c%16)*8, (c/16)*8, font8x8_basic[c]);
    }
    return result;
}

static void put_glyph_rgb(SDL_Renderer *renderer, int x, int y, unsigned char c, int bg, int fg)
{
    int dstx = x * GLYPH_W * GLYPH_SCALE;
    int dsty = y * GLYPH_H * GLYPH_SCALE;
    int srcx = (int)(c % 16) * GLYPH_W;
    int srcy = (int)(c / 16) * GLYPH_H;

    int fg_r = vga256[fg*3] * 3;
    int fg_g = vga256[fg*3+1] * 3;
    int fg_b = vga256[fg*3+2] * 3;

    int bg_r = vga256[bg*3] * 3;
    int bg_g = vga256[bg*3+1] * 3;
    int bg_b = vga256[bg*3+2] * 3;

    SDL_Rect src = { srcx, srcy, GLYPH_W, GLYPH_H };
    SDL_Rect dst = { dstx, dsty, GLYPH_W*GLYPH_SCALE, GLYPH_H*GLYPH_SCALE };

    SDL_SetRenderDrawColor(renderer, bg_r, bg_g, bg_b, 0xff);
    SDL_RenderFillRect(renderer, &dst);

    SDL_SetTextureColorMod(font_texture, fg_r, fg_g, fg_b);
    SDL_RenderCopy(renderer, font_texture, &src, &dst); 
}

static void write_key(int key_sym)
{
	int result;
    static char device[64];
    if( key_sym < 256) {
        result = ioctl(fd_tty, TIOCSTI, &key_sym);
        if(result < 0) {
            close(fd_tty);
            sprintf(device, "/dev/tty%d", tty_number);
            fd_tty = open(device, O_WRONLY);
            result = ioctl(fd_tty, TIOCSTI, &key_sym);
            if(result < 0) {
                printf("couldn't re-open %s!\n", device);
            }
        }
    } else {
        /* magic and dragons */
    }
}

static void write_text(const char *text)
{
    int len = strlen(text);
    for(int i = 0; i < len && text[i]; i++) {
        if (isprint(text[i])) {
            write_key(text[i]);
        }
    }
}

static void handle_keyup(int k)
{
    switch(k) {
    case SDLK_RCTRL:
        rctrl = 0;
        break;
    case SDLK_LCTRL:
        lctrl = 0;
        break;
    case SDLK_LSHIFT:
        lshift = 0;
        break;
    case SDLK_RSHIFT:
        rshift = 0;
        break;
    }
}

static void handle_keydown(int k)
{
    switch(k) {
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE:
    case SDLK_RETURN:
    case SDLK_TAB:
    case SDLK_DELETE:
        write_key(k);
        break;
    case SDLK_RCTRL:
        rctrl = 1;
        break;
    case SDLK_LCTRL:
        lctrl = 1;
        break;
    case SDLK_RSHIFT:
        rshift = 1;
        break;
    case SDLK_LSHIFT:
        lshift = 1;
        break;
    case SDLK_UP:
        write_key(0x1b);
        write_key(0x5b);
        write_key(0x41);
        break;
    case SDLK_DOWN:
        write_key(0x1b);
        write_key(0x5b);
        write_key(0x42);
        break;
    case SDLK_RIGHT:
        write_key(0x1b);
        write_key(0x5b);
        write_key(0x43);
        break;
    case SDLK_LEFT:
        write_key(0x1b);
        write_key(0x5b);
        write_key(0x44);
        break;
    default:
        if (lctrl || rctrl) {
            if (k == SDLK_u) {
                write_key(0x15);
            } else if (k == SDLK_z) {
                write_key(0x1a);
            } else if ((lctrl || rctrl) && k >= 'a' && k <= 'z') {
                write_key((k-64)&0xf);
            }
        }

        if (lshift || rshift) {
            /* idk */
        }
        break;
    }
}

static int handle_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                redraw = 1;
            }
            break;
        case SDL_TEXTINPUT:
            write_text(event.text.text);
            break;
        case SDL_TEXTEDITING:
            /*
            Update the composition text.
            Update the cursor position.
            Update the selection length (if any).
            composition = event.edit.text;
            cursor = event.edit.start;
            selection_len = event.edit.length;
            */
            break;
        case SDL_QUIT:
            return -1;
        case SDL_KEYDOWN:
            handle_keydown(event.key.keysym.sym);
            break;
        case SDL_KEYUP:
            handle_keyup(event.key.keysym.sym);
            break;
        default:
            break;
        }
    }
    return 0;
}

static void read_vcsa(const char *path_vcsa, char *buffer)
{
    int fd_vcsa = open(path_vcsa, O_RDWR);
    if (fd_vcsa < 0) {
        printf("couldn't open %s\n", path_vcsa);
        exit(1);
    }

    if (read(fd_vcsa, buffer, CONSOLE_LEN) == -1) {
        printf("error reading %s\n", path_vcsa);
        exit(1);
    }
    close(fd_vcsa);
}

static int render_console(SDL_Renderer *renderer, char *buffer, char *console)
{
    int i = 0;
    int x = 0;
    int y = 0;
    int c = 0;
    int fg = 0;
    int bg = 0;
    int cursor_x = buffer[2] & 0xff;
    int cursor_y = buffer[3] & 0xff;
    int delay_ms = 20;

    // We only want the console and will stretch to fit
    SDL_Rect r = {0};
    r.w = console_width * GLYPH_W;
    r.h = console_height * GLYPH_H;
    r.x = 0;
    r.y = 0;

    if (redraw || memcmp(buffer, console, CONSOLE_LEN)) {
        memcpy(console, buffer, CONSOLE_LEN);
        redraw = 0;
       
        // Clear backbuffer texture
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, gpu_texture);
        SDL_RenderClear(renderer);

        for(i = 0; i < console_width * console_height; i++) {

            // Current position
            x = i % console_width;
            y = i / console_width;

            // Character
            c = buffer[4+2*i] & 0xff;

            // Colors
            fg = buffer[5+i*2] & 0x0f;
            bg = (buffer[5+i*2]>>4) & 0x0f;

            // Draw a glyph
            put_glyph_rgb(renderer, x, y, c, bg, fg);
        }

        // Draw the cursor
        put_glyph_rgb(renderer, cursor_x, cursor_y, '_', bg, fg);

        SDL_SetRenderTarget(renderer, NULL);
        SDL_RenderCopy(renderer, gpu_texture, &r, NULL);

        delay_ms = 20;
    } else {
        delay_ms = 40;
    }

    SDL_RenderPresent(renderer);

    return delay_ms;
}

SDL_Renderer* init_sdl()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Window *window = SDL_CreateWindow("ttysdl", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xff);
    SDL_RenderClear(renderer);

    SDL_Surface *font_surface = make_font();
    SDL_SetColorKey(font_surface, SDL_TRUE, SDL_MapRGB(font_surface->format, 0xff, 0x00, 0xff ));
    font_texture = SDL_CreateTextureFromSurface(renderer, font_surface);
    SDL_FreeSurface(font_surface);

    SDL_DisplayMode mode;
    SDL_GetDesktopDisplayMode(0, &mode);

    gpu_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, mode.w, mode.h);

    //SDL_RenderSetLogicalSize(renderer, W, H);
    /*
    back_surface = SDL_CreateRGBSurface(0, W, H, 32, rmask, gmask, bmask, amask);
    SDL_SetSurfaceBlendMode(back_surface, SDL_BLENDMODE_NONE);
    */

    SDL_StartTextInput();
    return renderer;
}

int main(int argc, char **argv)
{
    char *buffer;
    char *console;
    struct winsize dimensions;
    char path_tty[64];
    char path_vcsa[64];

    if (argc > 1) {
        if ((tty_number = atoi(argv[1])) < 1) {
            printf("Usage: %s tty_number\n", argv[0]);
            exit(1);
        }

        if (argc == 4) {
            console_lock_size = 1;
            if ((console_width = atoi(argv[2])) < 1) {
                printf("Usage: %s tty_number\n", argv[0]);
                exit(1);
            }
            if ((console_height = atoi(argv[3])) < 1) {
                printf("Usage: %s tty_number\n", argv[0]);
                exit(1);
            }
        }
    }

    // Construct tty and vcsa paths
    snprintf(path_tty, sizeof(path_tty), "/dev/tty%d", tty_number);
    snprintf(path_vcsa , sizeof(path_vcsa), "/dev/vcsa%d", tty_number);

    // Open tty
    if ((fd_tty = open(path_tty, O_WRONLY)) < 0) {
        perror("error");
        exit(1);
    }
    printf("opened %s\n", path_tty);

    // Get console size
    if (ioctl(fd_tty, TIOCGWINSZ, &dimensions) >= 0) {
        if (!console_lock_size) {
            console_width = dimensions.ws_col;
            console_height = dimensions.ws_row;
        }
        buffer = malloc(CONSOLE_LEN);
        console = malloc(CONSOLE_LEN);
        console[0] = 0;
    } else {
        perror("error");
        return -1;
    }

    printf("we think the console is %dx%d characters...", console_width, console_height);

    // Init SDL
    SDL_Renderer *renderer = init_sdl();

    do {

        // Read screen
        read_vcsa(path_vcsa, buffer);

        // Adjust if screen size changed
        if (!console_lock_size) {
            if(console_height != (buffer[0] & 0xff) || console_width != (buffer[1] & 0xff)) {
                console_height = (buffer[0] & 0xff);
                console_width = (buffer[1] & 0xff);

                free(buffer);
                free(console);
                buffer = malloc(CONSOLE_LEN);
                console = malloc(CONSOLE_LEN);
                printf("console size changed, realloc'ing");
                continue;
            }
        }

        // Render
        SDL_Delay(render_console(renderer, buffer, console));

    } while (handle_events() != -1);

    free(buffer);
    free(console);
    return 0;
}
