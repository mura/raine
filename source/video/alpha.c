#include "raine.h"
#include "alpha.h"
#include "emudx.h"

/* Alpha blending support - mmx */
/* This module just handles global variables required by the mmx functions */

static UINT32 alpha,dalpha;

/* The CAPITALS for the global variables are kept because they were like that
   in the original code and I'd like to change it as little as possible */

#ifdef DARWIN
#undef RAINE_UNIX
#endif

UINT64 ALPHA64, ALPHABY4, COLORKEY64;

UINT64 MASKRED = 0xF800F800F800F800ll;
UINT64 MASKGREEN = 0x07E007E007E007E0ll;
UINT64 MASKBLUE = 0x001F001F001F001Fll;

UINT64 ADD64 = 0x0040004000400040ll;

/* May Microsoft be cursed for the eternity with its stupid _ everywhere !!! */

void init_alpha(UINT32 my_alpha) {
  alpha = my_alpha;
  dalpha = 256-alpha;

  asm(
#ifdef RAINE_UNIX
      "movd alpha,%mm2    \n" // Copy ALPHA into %mm2
#else
      "movd _alpha,%mm2    \n" // Copy ALPHA into %mm2
#endif
      "punpcklwd %mm2,%mm2 \n" // Unpack %mm2 - 0000 0000 00aa 00aa
      "punpckldq %mm2,%mm2 \n" // Unpack %mm2 - 00aa 00aa 00aa 00aa
#ifdef RAINE_UNIX
      "movq %mm2,ALPHA64  \n" // Save the result into ALPHA64
#else
      "movq %mm2,_ALPHA64  \n" // Save the result into ALPHA64
#endif
      "psrlw  $2,%mm2      \n" // Divide each ALPHA value by 4
#ifdef RAINE_UNIX
      "movq %mm2,ALPHABY4 \n" // Save the result to ALPHABY4
#else
      "movq %mm2,_ALPHABY4 \n" // Save the result to ALPHABY4
#endif

#ifdef RAINE_UNIX
      "movd emudx_transp,%mm4 \n" // Copy ColorKey into %mm4
#else
      "movd _emudx_transp,%mm4 \n" // Copy ColorKey into %mm4
#endif
      "punpcklwd %mm4,%mm4 \n" // Unpack %mm4 - 0000 0000 cccc cccc
      "punpckldq %mm4,%mm4 \n" // Unpack %mm4 - cccc cccc cccc cccc
#ifdef RAINE_UNIX
      "movq %mm4,COLORKEY64 \n" // Save the result into COLORKEY64
#else
      "movq %mm4,_COLORKEY64 \n" // Save the result into COLORKEY64
#endif
      "finit \n"
      );
}

void blend_16(UINT16 *dest, UINT16 src) {
    UINT8 rd,gd,bd,r,g,b;
    SDL_GetRGB(*dest,color_format,&rd,&gd,&bd);
    SDL_GetRGB(src,color_format,&r,&g,&b);
    *dest = SDL_MapRGB(color_format,
	    ((rd * dalpha) >> 8) + ((r * alpha) >> 8),
	    ((gd * dalpha) >> 8) + ((g * alpha) >> 8),
	    ((bd * dalpha) >> 8) + ((b * alpha) >> 8));
}


void blend50_16(UINT16 *dest, UINT16 src) {
    UINT8 rd,gd,bd,r,g,b;
    SDL_GetRGB(*dest,color_format,&rd,&gd,&bd);
    SDL_GetRGB(src,color_format,&r,&g,&b);
    *dest = SDL_MapRGB(color_format,
	    rd/2+r/2,
	    gd/2+g/2,
	    bd/2+g);
}


