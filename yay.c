/*

  yay - fast and simple yuv viewer

  (c) 2005-2010 by Matthias Wientapper
  (m.wientapper@gmx.de)

  Support of multiple formats added by Cuero Bugot.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "regex.h"

#include "SDL.h"
//#include <SDL/SDL.h>

#if SDL_MAJOR_VERSION == 1
#undef SDL2_SUPPORT
#else
#define SDL2_SUPPORT
#endif

SDL_Event event;
SDL_Rect video_rect;
#ifdef SDL2_SUPPORT
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_Window *window;
#else
SDL_Surface *screen;
SDL_Overlay *my_overlay;
const SDL_VideoInfo *info = NULL;
#endif

Uint32 width = 0;
Uint32 height = 0;
int depth = 8;
char *vfilename;
FILE *fpointer;
Uint8 *y_data, *cr_data, *cb_data;
Uint16 zoom = 1;
Uint16 min_zoom = 1;
Uint16 frame = 0;
Uint16 quit = 0;
Uint8 grid = 0;
Uint8 bpp = 0;
int cfidc = 1;
int isY4M = 0;

static const Uint8 SubWidthC[4] = {0, 2, 2, 1};
static const Uint8 SubHeightC[4] = {0, 2, 1, 1};
static const Uint8 SubSizeC[4] = {0, 4, 2, 1};
static const Uint8 MbWidthC[4] = {0, 8, 8, 16};
static const Uint8 MbHeightC[4] = {0, 8, 16, 16};
static const Uint8 FrameSize2C[4] = {2, 3, 4, 6};

int load_frame() {
  Uint32 cnt;
  /* Fill in video data */
  if (isY4M) fseek(fpointer, strlen("FRAME "), SEEK_CUR);
  cnt = fread(y_data, 1, width * height, fpointer);
  fprintf(stderr,"read [%dx%d] %d y bytes\n", width, height, cnt);
  if (cnt < width * height) {
    return 0;
  } else if (cfidc > 0) {
    cnt = fread(cb_data, 1, height * width / SubSizeC[cfidc], fpointer);
    fprintf(stderr,"read [%dx%d] %d cb bytes\n", width, height, cnt);
    if (cnt < width * height / 4) {
      return 0;
    } else {
      cnt = fread(cr_data, 1, height * width / SubSizeC[cfidc], fpointer);
      fprintf(stderr,"read [%dx%d] %d cr bytes\n", width, height, cnt);
      if (cnt < width * height / 4) {
        return 0;
      }
    }
  }
  return 1;
}

void convert_chroma_to_420() {
#ifndef SDL2_SUPPORT
  int i, j;
  // printf("%dx%d\n",width, height);
  if (cfidc > 0) {
    for (j = 0; j < height / 2; j++)
      for (i = 0; i < width / 2; i++) {
        my_overlay->pixels[1][j * my_overlay->pitches[1] + i] =
            cr_data[i * MbWidthC[cfidc] / 8 +
                    j * (width / SubWidthC[cfidc]) * MbHeightC[cfidc] / 8];
        my_overlay->pixels[2][j * my_overlay->pitches[2] + i] =
            cb_data[i * MbWidthC[cfidc] / 8 +
                    j * (width / SubWidthC[cfidc]) * MbHeightC[cfidc] / 8];
      }
  } else {
    for (i = 0; i < height / 2; i++) {
      memset(my_overlay->pixels[1] + i * my_overlay->pitches[1], 128,
             width / 2);
      memset(my_overlay->pixels[2] + i * my_overlay->pitches[2], 128,
             width / 2);
    }
  }
#endif
}
/* 422 (YUY2, etc) formats are the largest */
#define MAX_YUV_SURFACE_SIZE(W, H, P) (H * 4 * (W + P + 1) / 2)

void draw_frame() {
#ifdef SDL2_SUPPORT

  static void *raw_yuv = 0;
  int pitch = width;
    printf("ptich:%d\r\n", pitch);

  if (raw_yuv == 0)
      raw_yuv = SDL_calloc(1, MAX_YUV_SURFACE_SIZE(width, height, 0));
  memcpy(raw_yuv, y_data, height * width);
  memcpy(raw_yuv + height * width, cb_data, height * width / SubSizeC[cfidc]);
  memcpy(raw_yuv + height * width + height * width / SubSizeC[cfidc], cr_data, height * width / SubSizeC[cfidc]);

  SDL_UpdateTexture(texture, NULL, raw_yuv, pitch);

  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  //SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderPresent(renderer);
#else

  Sint16 x, y;
  Uint16 i;

  /* Fill in pixel data - the pitches array contains the length of a line in
   * each plane*/
  SDL_LockYUVOverlay(my_overlay);

  // we cannot be sure, that buffers are contiguous in memory
  if (width != my_overlay->pitches[0]) {
    for (i = 0; i < height; i++) {
      memcpy(my_overlay->pixels[0] + i * my_overlay->pitches[0],
             y_data + i * width, width);
    }
  } else {
    memcpy(my_overlay->pixels[0], y_data, width * height);
  }

  if (cfidc == 1) {
    if (width != my_overlay->pitches[1]) {
      for (i = 0; i < height / 2; i++) {
        memcpy(my_overlay->pixels[1] + i * my_overlay->pitches[1],
               cr_data + i * width / 2, width / 2);
      }
    } else {
      memcpy(my_overlay->pixels[1], cr_data, width * height / 4);
    }

    if (width != my_overlay->pitches[2]) {
      for (i = 0; i < height / 2; i++) {
        memcpy(my_overlay->pixels[2] + i * my_overlay->pitches[2],
               cb_data + i * width / 2, width / 2);
      }
    } else {
      memcpy(my_overlay->pixels[2], cb_data, width * height / 4);
    }
  }
  convert_chroma_to_420();

  if (grid) {
    // horizontal grid lines
    for (y = 0; y < height; y = y + 16) {
      for (x = 0; x < width; x += 8) {
        *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = 0xF0;
        *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x + 4) = 0x20;
      }
    }
    // vertical grid lines
    for (x = 0; x < width; x = x + 16) {
      for (y = 0; y < height; y += 8) {
        *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = 0xF0;
        *(my_overlay->pixels[0] + (y + 4) * my_overlay->pitches[0] + x) = 0x20;
      }
    }
  }

  SDL_UnlockYUVOverlay(my_overlay);

  video_rect.x = 0;
  video_rect.y = 0;
  video_rect.w = width * zoom;
  video_rect.h = height * zoom;

  SDL_DisplayYUVOverlay(my_overlay, &video_rect);
#endif
}

void print_usage() {
  fprintf(stdout,
          "Usage: yay [-s <widht>x<heigh>] [-f format] filename.yuv\n\t format "
          "can be: 0-Y only, 1-YUV420, 2-YUV422, 3-YUV444\n");
}

static Uint32 vflags = 0;
void init_sdl() {
#ifdef SDL2_SUPPORT

  unsigned int width_ = width * zoom;
  unsigned int height_ = height * zoom;
  SDL_Init(SDL_INIT_VIDEO);
  window = SDL_CreateWindow("YUV Viewer", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, width_, height_, 0);

  renderer = SDL_CreateRenderer(window, -1, 0);
  // TODO: Check more format
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV,
                              SDL_TEXTUREACCESS_STREAMING, width_, height_);
#else
  // SDL init
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
    exit(1);
  }
  atexit(SDL_Quit);

  info = SDL_GetVideoInfo();
  if (!info) {
    fprintf(stderr, "SDL ERROR Video query failed: %s\n", SDL_GetError());
    SDL_Quit();
    exit(0);
  }

  bpp = info->vfmt->BitsPerPixel;
  if (info->hw_available)
    vflags = SDL_HWSURFACE;
  else
    vflags = SDL_SWSURFACE;

  if ((screen = SDL_SetVideoMode(width * zoom, height * zoom, bpp, vflags)) ==
      0) {
    fprintf(stderr, "SDL ERROR Video mode set failed: %s\n", SDL_GetError());
    SDL_Quit();
    exit(0);
  }

  // DEBUG output
  // printf("SDL Video mode set successfully. \nbbp: %d\nHW: %d\nWM: %d\n",
  // 	info->vfmt->BitsPerPixel, info->hw_available, info->wm_available);

  my_overlay = SDL_CreateYUVOverlay(width, height, SDL_YV12_OVERLAY, screen);
  if (!my_overlay) {                               // Couldn't create overlay?
    fprintf(stderr, "Couldn't create overlay\n");  // Output to stderr and quit
    exit(1);
  }
#endif
}

void deinit_sdl() {
#ifndef SDL2_SUPPORT
  SDL_FreeYUVOverlay(my_overlay);
#endif
}

void parse_y4m_info()
{
    /* We got a Y4M ! Hurray ! */
    fseek(fpointer, 0, SEEK_SET);
    while (!feof(fpointer)) {
        // Skip Y4MPEG string
        int c = fgetc(fpointer);
        int d, csp = 0;
        while (!feof(fpointer) && (c != ' ') && (c != '\n')) {
            c = fgetc(fpointer);
        }
        
        while (c == ' ' && !feof(fpointer)) {
            // read parameter identifier
            switch (fgetc(fpointer)) {
                case 'W':
                    width = 0;
                    while (!feof(fpointer)) {
                        c = fgetc(fpointer);
                        
                        if (c == ' ' || c == '\n') {
                            break;
                        } else {
                            width = width * 10 + (c - '0');
                        }
                    }
                    break;
                case 'H':
                    height = 0;
                    while (!feof(fpointer)) {
                        c = fgetc(fpointer);
                        if (c == ' ' || c == '\n') {
                            break;
                        } else {
                            height = height * 10 + (c - '0');
                        }
                    }
                    break;
                    
                case 'F':
                    /* rateNum = 0; */
                    /* rateDenom = 0; */
                    while (!feof(fpointer)) {
                        c = fgetc(fpointer);
                        if (c == '.') {
                            /* rateDenom = 1; */
                            while (!feof(fpointer)) {
                                c = fgetc(fpointer);
                                if (c == ' ' || c == '\n') {
                                    break;
                                } else {
                                    /* rateNum = rateNum * 10 + (c - '0'); */
                                    /* rateDenom = rateDenom * 10; */
                                }
                            }
                            
                            break;
                        } else if (c == ':') {
                            while (!feof(fpointer)) {
                                c = fgetc(fpointer);
                                if (c == ' ' || c == '\n') {
                                    break;
                                } else {
                                    /* rateDenom = rateDenom * 10 + (c - '0'); */
                                }
                            }
                            break;
                        } else {
                            /* rateNum = rateNum * 10 + (c - '0'); */
                        }
                    }
                    break;
                    
                case 'A':
                    /* sarWidth = 0; */
                    /* sarHeight = 0; */
                    while (!feof(fpointer)) {
                        c = fgetc(fpointer);
                        if (c == ':') {
                            while (!feof(fpointer)) {
                                c = fgetc(fpointer);
                                if (c == ' ' || c == '\n') {
                                    break;
                                } else {
                                    /* sarHeight = sarHeight * 10 + (c - '0'); */
                                }
                            }
                            break;
                        } else {
                            /* sarWidth = sarWidth * 10 + (c - '0'); */
                        }
                    }
                    break;
                    
                case 'C':
                    csp = 0;
                    d = 0;
                    while (!feof(fpointer)) {
                        c = fgetc(fpointer);
                        
                        if (c <= '9' && c >= '0') {
                            csp = csp * 10 + (c - '0');
                        } else if (c == 'p') {
                            // example: C420p16
                            while (!feof(fpointer)) {
                                c = fgetc(fpointer);
                                
                                if (c <= '9' && c >= '0')
                                    d = d * 10 + (c - '0');
                                else
                                    break;
                            }
                            break;
                        } else
                            break;
                    }
                    if (d >= 8 && d <= 16) depth = d;
                    cfidc = (csp == 444) ? 3 : (csp == 422) ? 2 : 1;
                    break;
                    
                default:
                    while (!feof(fpointer)) {
                        // consume this unsupported configuration word
                        c = fgetc(fpointer);
                        if (c == ' ' || c == '\n') break;
                    }
                    
                    break;
            }
        }
        
        if (c == '\n') {
            break;
        }
    }
    isY4M = ftell(fpointer);
}

int main(int argc, char *argv[]) {
  int opt;
  char caption[32];
  regex_t reg;
  regmatch_t pm;
  int result;
  char picsize[32] = "";
  int used_s_opt = 0;
  int play_yuv = 0;
  unsigned int start_ticks = 0;

  if (argc == 1) {
    print_usage();
    return 1;
  } else {
    while ((opt = getopt(argc, argv, "f:s:")) != -1) switch (opt) {
        case 's':
          if (sscanf(optarg, "%dx%d", &width, &height) != 2) {
            fprintf(stdout,
                    "No geometry information provided by -s parameter.\n");
            return 1;
          }
          used_s_opt = 1;
          break;
        case 'f':
          if (sscanf(optarg, "%d", &cfidc) != 1 || (cfidc < 0 && cfidc > 3)) {
            fprintf(stdout, "Invalid format provided by -f parameter.\n");
            return 1;
          }
          break;
        default:
          print_usage();
          return 1;
          break;
      }
  }
  argv += optind;
  argc -= optind;

  vfilename = argv[0];
  fpointer = fopen(vfilename, "rb");
  if (fpointer == NULL) {
    fprintf(stderr, "Error opening %s\n", vfilename);
    return 1;
  }

  if (!used_s_opt) {
    // try to find picture size from filename or path
    if (regcomp(&reg, "_[0-9]+x[0-9]+", REG_EXTENDED) != 0) return -1;
    result = regexec(&reg, vfilename, 1, &pm, REG_NOTBOL);
    if (result == 0) {
      strncpy(picsize, (vfilename + pm.rm_so + 1), (pm.rm_eo - pm.rm_so - 1));
      strcat(picsize, "\0");
    }
    if (sscanf(picsize, "%dx%d", &width, &height) != 2) {
      /* Maybe it's Y4M ? */
      const char *y4m_magic = "YUV4MPEG2";
      char input[9];

      if (fread(input, 1, 9, fpointer) != 9 || memcmp(y4m_magic, input, 9)) {
        fprintf(stdout,
                "No geometry information found in path/filename.\nPlease use "
                "-s <width>x<height> paramter.\n");
        return 1;
      } else {
          parse_y4m_info();
      }
    }
  }
  // some WM can't handle small windows...
  if (width < 100) {
    zoom = 2;
    min_zoom = 2;
  }
  // printf("using x=%d y=%d\n", width, height);
  init_sdl();
#ifndef SDL2_SUPPORT
  SDL_EnableKeyRepeat(500, 10);
#endif

  /* should allocate memory for y_data, cr_data, cb_data here */
  y_data = malloc(width * height * sizeof(Uint8));
  if (cfidc > 0) {
    cb_data = malloc(width * height * sizeof(Uint8) / SubSizeC[cfidc]);
    cr_data = malloc(width * height * sizeof(Uint8) / SubSizeC[cfidc]);
  }

  // send event to display first frame
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = SDLK_RIGHT;
  SDL_PushEvent(&event);

  // main loop
  while (!quit) {
    sprintf(caption, "frame %d, zoom=%d", frame, zoom);
#ifndef SDL2_SUPPORT
    SDL_WM_SetCaption(caption, NULL);
#endif

    // wait for SDL event
    SDL_WaitEvent(&event);

    switch (event.type) {
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
          case SDLK_SPACE: {
            play_yuv = 1;  // play it, sam!
            while (play_yuv) {
              start_ticks = SDL_GetTicks();
              sprintf(caption, "frame %d, zoom=%d", frame, zoom);
#ifndef SDL2_SUPPORT
              SDL_WM_SetCaption(caption, NULL);
#endif

              // check for next frame existing
              if (load_frame()) {
                draw_frame();
                // insert delay for real time viewing
                if (SDL_GetTicks() - start_ticks < 40)
                  SDL_Delay(40 - (SDL_GetTicks() - start_ticks));
                frame++;
              } else {
                play_yuv = 0;
              }
              // check for any key event
              if (SDL_PollEvent(&event)) {
                if (event.type == SDL_KEYDOWN) {
                  // stop playing
                  play_yuv = 0;
                }
              }
            }
            break;
          }
          case SDLK_RIGHT: {
            // check for next frame existing
            if (load_frame()) {
              draw_frame();
              frame++;
            }

            break;
          }
          case SDLK_BACKSPACE:
          case SDLK_LEFT: {
            if (frame > 1) {
              frame--;
              fseek(fpointer,
                    isY4M +
                        (frame - 1) * (height * width * FrameSize2C[cfidc] / 2 +
                                       (isY4M ? strlen("FRAME ") : 0)),
                    SEEK_SET);
              // if(draw_frame())
              load_frame();
              draw_frame();
            }
            break;
          }
#ifndef SDL2_SUPPORT
          case SDLK_UP: {
            zoom++;
            screen = SDL_SetVideoMode(width * zoom, height * zoom, bpp, vflags);
            video_rect.w = width * zoom;
            video_rect.h = height * zoom;
            SDL_DisplayYUVOverlay(my_overlay, &video_rect);
            break;
          }
          case SDLK_DOWN: {
            if (zoom > min_zoom) {
              zoom--;
              screen =
                  SDL_SetVideoMode(width * zoom, height * zoom, bpp, vflags);
              video_rect.w = width * zoom;
              video_rect.h = height * zoom;
              SDL_DisplayYUVOverlay(my_overlay, &video_rect);
            }
            break;
          }
#endif
          case SDLK_r: {
            if (frame > 1) {
              frame = 1;
              fseek(fpointer, isY4M, SEEK_SET);
              // if(draw_frame())
              load_frame();
              draw_frame();
            }
            break;
          }
          case SDLK_g:
            grid = ~grid;
            draw_frame();
            break;
          case SDLK_q:
            quit = 1;
            break;
#ifndef SDL2_SUPPORT
          case SDLK_f:
            SDL_WM_ToggleFullScreen(screen);
            break;
#endif
          default:
            break;
        }  // switch key
        break;
      case SDL_QUIT:
        quit = 1;
        break;
#ifndef SDL2_SUPPORT
      case SDL_VIDEOEXPOSE:
        SDL_DisplayYUVOverlay(my_overlay, &video_rect);
        break;
#endif
      default:
        break;

    }  // switch event type

  }  // while
  // clean up
  deinit_sdl();
  free(y_data);
  free(cb_data);
  free(cr_data);
  fclose(fpointer);
  if (!used_s_opt) regfree(&reg);

  return 0;
}
