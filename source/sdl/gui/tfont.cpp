/* Font support : we'll use first the fonts from SDL_gfx
 * they are extremely easy to use : constant char width & height, with
 * a default 8x8 font available if we don't load any font
 * It might be interesting to use ttf fonts one day, so we'll try to keep
 * this as generic as possible */

#include <string.h>
#include <stdio.h>
#include "files.h"
#include "sdl/compat.h"
#include "sdl/SDL_gfx/SDL_gfxPrimitives.h"
#include "tfont.h"
#include <dirent.h>
#include "confile.h"
#include "menu.h"
#include "sdl/display_sdl.h"

int min_font_size = 8;
static int max_font_size = 30;

void save_font_config() {
  raine_set_config_int("GUI", "min_font_size", min_font_size);
  raine_set_config_int("GUI", "max_font_size", max_font_size);
}

void read_font_config() {
  min_font_size = raine_get_config_int("GUI", "min_font_size", 8);
  max_font_size = raine_get_config_int("GUI", "max_font_size", 30);
}

// These lists of 3 values are ranges : min, max, step
static menu_item_t font_options[] =
{
  { "Min font size", NULL, &min_font_size, 3, {8, 20, 1} },
  { "Max font size", NULL, &max_font_size, 3, {20, 40, 1} },
};

int add_fonts_gui_options(menu_item_t *menu) {
  menu[0] = font_options[0];
  menu[1] = font_options[1];
  return 2;
}

static void get_font_dimensions(char *s, unsigned int *width, unsigned int *height) {
  while ((*s < '0' || *s > '9') && *s)
    s++;
  *width = atoi(s);
  while ((*s >= '0' && *s <= '9')) // find separator
    s++;
  while ((*s < '0' || *s > '9') && *s) // find next number
    s++;
  *height = atoi(s);
}

TFont::TFont(int size, char *font) {
  loaded_font = NULL;
}

TFont::TFont(char *myfont) {
  loaded_font = NULL;
  load_font(myfont);
}

void TFont::select_ideal_font(int ideal_width, int ideal_height) {
  unsigned int bestw = 0, besth=0;
  char *fdir = get_shared("fonts");
  DIR *dir = opendir(fdir);
  loaded_font = NULL;
  if (dir) {
    char selected[256];
    unsigned int w,h;
    struct dirent *entry;
    // 1st look for height
    while ((entry = readdir(dir))) {
      get_font_dimensions(entry->d_name,&w,&h);
      if (abs(h-ideal_height) < abs(besth - ideal_height))
	besth = h;
    }
    rewinddir(dir);
    // Now look for the best width with this height
    while ((entry = readdir(dir))) {
      get_font_dimensions(entry->d_name,&w,&h);
      if (h == besth && (abs(w-ideal_width) < abs(bestw-ideal_width))) {
	strcpy(selected, entry->d_name);
	bestw = w;
      }
    }
    closedir(dir);
    TFont::load_font(selected);
  }
}

TFont::TFont(int ideal_width, int ideal_height) {
  select_ideal_font(ideal_width,ideal_height);
}

void TFont::load_font(char *myfont) {
  FILE *f;
  unsigned int len,width,height;
  get_font_dimensions(myfont,&width,&height);
  if (!width || !height) { // load default font
    charWidth = charHeight = 0;
    if (loaded_font) {
      free(loaded_font);
      loaded_font = NULL;
    }
    return;
  }
  if (strchr(myfont,'/') == 0) {
    char tpath[1024];
    sprintf(tpath,"fonts/%s",myfont);
    f = fopen(get_shared(tpath),"rb");
  } else
    f = fopen(get_shared(myfont),"rb");

  if (f) {
    fseek(f,0L,SEEK_END);
    len = ftell(f);
    fseek(f,0L,SEEK_SET);
    if (loaded_font)
      free(loaded_font);

    loaded_font = (char*)malloc(len);
    if (loaded_font) {
      fread(loaded_font,len,1,f);
      charWidth = width;
      charHeight = height;
      gfxPrimitivesSetFont(loaded_font,charWidth,charHeight);
    } else {
      charWidth = charHeight = 0;
      // in this case CharWidth = CharHeight = 8, see get_font_height...
    }
    fclose(f);
  }
}

TFont::~TFont() {
  if (loaded_font) {
    free(loaded_font);
    loaded_font = NULL;
  }
}

void TFont::dimensions(const char *s, int *w, int *h) {
  if (loaded_font) {
    *w = strlen(s)*charWidth;
    *h = charHeight;
  } else {
    *w = strlen(s)*8;
    *h = 8;
  }
}

void TFont::put_string(int x, int y, const char *s, int color) {
  surf_string(sdl_screen,x,y,s,color);
}

void TFont::put_string(int x, int y, const char *s, int color, int bgcolor) {
  surf_string(sdl_screen,x,y,s,color,bgcolor);
}

void TFont::surf_string(SDL_Surface *surf,int x, int y, const char *s, int color) {
  if (loaded_font)
    gfxPrimitivesSetFont(loaded_font,charWidth,charHeight);
  stringColor(surf,x,y,s,color);
}

void TFont::surf_string(SDL_Surface *surf,int x, int y, const char *s, int color, int bgcolor) {
  boxColor(surf,x,y,charWidth*strlen(s)+x-1,charHeight+y-1,bgcolor);
  surf_string(surf,x,y,s,color);
}

// SDL_ttf

#include <SDL_ttf.h>

TFont_ttf::TFont_ttf(int size, char *font) :
 TFont::TFont(size,font) {
  if (size < min_font_size) size = min_font_size;
  if (size > max_font_size) size = max_font_size;
  ttf = NULL;
  charWidth = charHeight = size; // for loading...
  load_font(font);
  dimensions("M",&charWidth,&charHeight);
}

TFont_ttf::~TFont_ttf() {
  if (ttf) TTF_CloseFont(ttf);
}

void TFont_ttf::dimensions(const char *s,int *w, int *h) {
  if (ttf) {
    TTF_SizeText(ttf,s,w,h);
  } else {
    TFont::dimensions(s,w,h);
  }
}

void TFont_ttf::put_string(int x, int y, const char *s, int color) {
  surf_string(sdl_screen,x,y,s,color);
}

void TFont_ttf::put_string(int x, int y, const char *s, int color, int bgcolor) {
  surf_string(sdl_screen,x,y,s,color,bgcolor);
}

void TFont_ttf::surf_string(SDL_Surface *surf,int x, int y, const char *s, int color) {
  if (!ttf)
    return TFont::surf_string(surf,x,y,s,color);
  if (!*s)
    return;
#if 1
  SDL_Rect dest;
  SDL_Color sc;
  sc.b = (color >> 8) & 0xff;
  sc.g = (color >> 16) & 0xff;
  sc.r = (color >> 24) & 0xff;
  SDL_Surface *sf = TTF_RenderText_Solid(ttf,s,sc );
  dest.x = x; dest.y = y;
  SDL_BlitSurface(sf,NULL,surf,&dest);
  SDL_FreeSurface(sf);
#else
  SDL_Color sc;
  sc.b = (color >> 8) & 0xff;
  sc.g = (color >> 16) & 0xff;
  sc.r = (color >> 24) & 0xff;
  SDL_Surface *sf = TTF_RenderText_Blended(ttf,s,sc);
#if 0
  UINT32 *pixels;
  for (int y=0; y<sf->h; y++) {
    pixels = ((UINT32 *)sf->pixels) + y*sf->pitch/4;
    for (int x=0; x<sf->w; x++) {
      if (*pixels & 0xffffff)
	*pixels |= 0xff000000;
      pixels++;
    }
  }
#endif

  SDL_Rect dest;
  dest.x = x; dest.y = y;
  SDL_BlitSurface(sf,NULL,surf,&dest);
  SDL_FreeSurface(sf);
#endif
}

void TFont_ttf::surf_string(SDL_Surface *surf,int x, int y, const char *s, int color, int bgcolor) {
  if (!ttf)
    return TFont::surf_string(surf,x,y,s,color,bgcolor);
  if (!bgcolor) // 0 is totally transparent in sdl_gfx -> no bg
    return surf_string(surf,x,y,s,color);
  SDL_Rect dest;
  SDL_Color sc,bg;
  sc.b = (color >> 8) & 0xff;
  sc.g = (color >> 16) & 0xff;
  sc.r = (color >> 24) & 0xff;
  bg.b = (bgcolor >> 8) & 0xff;
  bg.g = (bgcolor >> 16) & 0xff;
  bg.r = (bgcolor >> 24) & 0xff;
  SDL_Surface *sf = TTF_RenderText_Shaded(ttf,s,sc,bg);
  // SDL_SetColorKey(sf,SDL_SRCCOLORKEY | SDL_RLEACCEL,0);
  dest.x = x; dest.y = y;
  SDL_BlitSurface(sf,NULL,surf,&dest);
  SDL_FreeSurface(sf);
}

void TFont_ttf::load_font(char *myfont) {
  if (!strstr(myfont,"ttf"))
    myfont = "Vera.ttf";
  char tpath[1024];
  sprintf(tpath,"fonts/%s",myfont);
  ttf = TTF_OpenFont(get_shared(tpath),charHeight*3/2);
  if ( ttf == NULL ) {
    fprintf(stderr, "Couldn't load %d pt font from %s: %s\n",
	charHeight, myfont, SDL_GetError());
    // falling back to bitmap font
    select_ideal_font(charWidth,charHeight);
  } else {
    TTF_SetFontStyle(ttf, TTF_STYLE_NORMAL);
    dimensions("ABCDEF",&charWidth,&charHeight);
    charWidth /= 6;
  }
}

