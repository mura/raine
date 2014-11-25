/******************************************************************************/
/*                                                                            */
/*                         RAINE 16x16 TILE DRAWING                           */
/*                                                                            */
/******************************************************************************/

#include "deftypes.h"
#include "raine.h"
#include "emudx.h"
#include "blit.h"
#include "alpha.h"

/*

Mapped
Transparent Mapped
Direct-Mapped
Transparent Direct-Mapped

*/

static BITMAP *mask_bitmap;
void init_spr16x16asm_mask(BITMAP *my_bitmap)
{
  mask_bitmap = my_bitmap;
}

void init_newspr2asm() {}
void init_newspr2asm_16() {}
void init_newspr2asm_32() {}

// These args macro to keep the lines of the render macro short, they fit
// in 80 columns this way which is nicer to read, and it's always the same
// arguments anyway !
#define ARG_MAP UINT8 *SPR, int x, int y, UINT8 *cmap
#define ARG_DIR UINT8 *SPR, int x, int y, UINT8 cmap
#define render(SIZE,BPP)                                               \
void init_spr##SIZE##x##SIZE##asm_##BPP(void)                          \
{                                                                      \
}                                                                      \
                                                                       \
void init_spp##SIZE##x##SIZE##asm_##BPP(void)                          \
{                                                                      \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Mapped_##BPP(ARG_MAP)                       \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy]))+ x;               \
      for(xx=0; xx<SIZE; xx++){                                        \
         *line++ = ((UINT##BPP *)cmap)[ *SPR++ ];                      \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Mapped_##BPP##_FlipX(ARG_MAP)               \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy]))+ x;               \
      for(xx=0; xx<SIZE; xx++){                                        \
         *line++ = ((UINT##BPP *)cmap)[ *SPR++ ];                      \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Mapped_##BPP##_FlipY(ARG_MAP)               \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x + 15;         \
      for(xx=0; xx<SIZE; xx++){                                        \
         *line-- = ((UINT##BPP *)cmap)[ *SPR++ ];                      \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Mapped_##BPP##_FlipXY(ARG_MAP)              \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x + 15;         \
      for(xx=0; xx<SIZE; xx++){                                        \
         *line-- = ((UINT##BPP *)cmap)[ *SPR++ ];                      \
      }                                                                \
   }                                                                   \
}                                                                      \
/* ##SIZE##x##SIZE## mapped transparent sprites */                     \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Mapped_##BPP(ARG_MAP)                 \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx++, SPR++){                                 \
         if(*SPR)                                                      \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR ];                    \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Mapped_##BPP##_FlipX(ARG_MAP)         \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx++, SPR++){                                 \
         if(*SPR)                                                      \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR ];                    \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Mapped_##BPP##_FlipY(ARG_MAP)         \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-1; xx>=0; xx--, SPR++){                              \
         if(*SPR)                                                      \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR ];                    \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Mapped_##BPP##_FlipXY(ARG_MAP)        \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-1; xx>=0; xx--, SPR++){                              \
         if(*SPR)                                                      \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR ];                    \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
/* packed version - transparent only */                                \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Packed_Mapped_##BPP(ARG_MAP)          \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx+=2, SPR++){                                \
         if(*SPR & 15)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];               \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Packed_Mapped_##BPP##_FlipX(ARG_MAP)  \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx+=2, SPR++){                                \
         if(*SPR & 15)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];               \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Packed_Mapped_##BPP##_FlipY(ARG_MAP)  \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-2; xx>=0; xx-=2, SPR++){                             \
         if(*SPR & 15)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR & 15 ];               \
         if(*SPR >> 4)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Packed_Mapped_##BPP##_FlipXY(ARG_MAP) \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-2; xx>=0; xx-=2, SPR++){                             \
         if(*SPR & 15)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR & 15 ];               \
         if(*SPR >> 4)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##xH_Trans_Packed_Mapped_##BPP                          \
(UINT8 *SPR, int x, int y, int height, UINT8 *cmap)                    \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<height; yy++){                                         \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx+=2, SPR++){                                \
         if(*SPR & 15)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];               \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##xH_Trans_Packed_Mapped_##BPP##_FlipX                  \
(UINT8 *SPR, int x, int y, int height, UINT8 *cmap)                    \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=height-1; yy>=0; yy--){                                      \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx+=2, SPR++){                                \
         if(*SPR & 15)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];               \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##xH_Trans_Packed_Mapped_##BPP##_FlipY                  \
(UINT8 *SPR, int x, int y, int height, UINT8 *cmap)                    \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<height; yy++){                                         \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-2; xx>=0; xx-=2, SPR++){                             \
         if(*SPR & 15)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR & 15 ];               \
         if(*SPR >> 4)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##xH_Trans_Packed_Mapped_##BPP##_FlipXY                 \
(UINT8 *SPR, int x, int y, int height, UINT8 *cmap)                    \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=height-1; yy>=0; yy--){                                      \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-2; xx>=0; xx-=2, SPR++){                             \
         if(*SPR & 15)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR & 15 ];               \
         if(*SPR >> 4)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Packed_Mapped_SwapWord_##BPP(ARG_MAP) \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx+=8, SPR++){                                \
         if(*SPR & 15)                                                 \
            line[xx+5] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx+4] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+7] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx+6] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];               \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+3] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx+2] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Packed_Mapped_SwapWord_##BPP##_FlipX  \
(ARG_MAP)                                                              \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx+=8, SPR++){                                \
         if(*SPR & 15)                                                 \
            line[xx+5] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx+4] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+7] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx+6] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+1] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];               \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+3] = ((UINT##BPP *)cmap)[ *SPR & 15 ];             \
         if(*SPR >> 4)                                                 \
            line[xx+2] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Packed_Mapped_SwapWord_##BPP##_FlipY  \
(ARG_MAP)                                                              \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-8; xx>=0; xx-=8, SPR++){                             \
         if(*SPR & 15)                                                 \
            line[xx+7-5] = ((UINT##BPP *)cmap)[ *SPR & 15 ];           \
         if(*SPR >> 4)                                                 \
            line[xx+7-4] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];           \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR & 15 ];               \
         if(*SPR >> 4)                                                 \
            line[xx+7-6] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];           \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+7-1] = ((UINT##BPP *)cmap)[ *SPR & 15 ];           \
         if(*SPR >> 4)                                                 \
            line[xx+7] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+7-3] = ((UINT##BPP *)cmap)[ *SPR & 15 ];           \
         if(*SPR >> 4)                                                 \
            line[xx+7-2] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];           \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_Packed_Mapped_SwapWord_##BPP##_FlipXY \
(ARG_MAP)                                                              \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-8; xx>=0; xx-=8, SPR++){                             \
         if(*SPR & 15)                                                 \
            line[xx+7-5] = ((UINT##BPP *)cmap)[ *SPR & 15 ];           \
         if(*SPR >> 4)                                                 \
            line[xx+7-4] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];           \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx] = ((UINT##BPP *)cmap)[ *SPR & 15 ];               \
         if(*SPR >> 4)                                                 \
            line[xx+7-6] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];           \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+7-1] = ((UINT##BPP *)cmap)[ *SPR & 15 ];           \
         if(*SPR >> 4)                                                 \
            line[xx+7] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];             \
     SPR++;                                                            \
         if(*SPR & 15)                                                 \
            line[xx+7-3] = ((UINT##BPP *)cmap)[ *SPR & 15 ];           \
         if(*SPR >> 4)                                                 \
            line[xx+7-2] = ((UINT##BPP *)cmap)[ *SPR >> 4 ];           \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
/* ##SIZE##x##SIZE## sprites */                                        \
/* These are for emudx */                                              \
                                                                       \
void Draw##SIZE##x##SIZE##_##BPP(ARG_DIR)                              \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx++){                                        \
         *line++ = ReadWord(SPR); SPR+=(BPP/8);                        \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_##BPP##_FlipX(ARG_DIR)                      \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx++){                                        \
         *line++ = ReadWord(SPR); SPR+=(BPP/8);                        \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_##BPP##_FlipY(ARG_DIR)                      \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x + 15;         \
      for(xx=0; xx<SIZE; xx++){                                        \
         *line-- = ReadWord(SPR); SPR+=(BPP/8);                        \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_##BPP##_FlipXY(ARG_DIR)                     \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x + 15;         \
      for(xx=0; xx<SIZE; xx++){                                        \
         *line-- = ReadWord(SPR); SPR+=(BPP/8);                        \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
/* ##SIZE##x##SIZE## transparent sprites */                            \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_##BPP(ARG_DIR)                        \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx++, SPR+=(BPP/8)){                          \
         if(ReadWord(SPR) != emudx_transp)                             \
            line[xx] = ReadWord(SPR);                                  \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_##BPP##_FlipX(ARG_DIR)                \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=0; xx<SIZE; xx++, SPR+=(BPP/8)){                          \
         if(ReadWord(SPR) != emudx_transp)                             \
            line[xx] = ReadWord(SPR);                                  \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_##BPP##_FlipY(ARG_DIR)                \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=0; yy<SIZE; yy++){                                           \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-1; xx>=0; xx--, SPR+=(BPP/8)){                       \
         if(ReadWord(SPR) != emudx_transp)                             \
            line[xx] = ReadWord(SPR);                                  \
      }                                                                \
   }                                                                   \
}                                                                      \
                                                                       \
void Draw##SIZE##x##SIZE##_Trans_##BPP##_FlipXY(ARG_DIR)               \
{                                                                      \
   UINT##BPP *line;                                                    \
   int xx,yy;                                                          \
                                                                       \
   for(yy=SIZE-1; yy>=0; yy--){                                        \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;              \
      for(xx=SIZE-1; xx>=0; xx--, SPR+=(BPP/8)){                       \
         if(ReadWord(SPR) != emudx_transp)                             \
            line[xx] = ReadWord(SPR);                                  \
      }                                                                \
   }                                                                   \
}                                                                      \

render(8,8);
render(8,16);
render(8,32);

render(16,8);
render(16,16);
render(16,32);

render(32,8);
render(32,16);
render(32,32);

render(64,8);
render(64,16);
render(64,32);

#undef render
#define render(SIZE,BPP)                                         \
void Draw##SIZE##x##SIZE##_Trans_Alpha50_##BPP(ARG_DIR)          \
{                                                                \
   UINT##BPP *line;                                              \
   int xx,yy;                                                    \
                                                                 \
   for(yy=0; yy<SIZE; yy++){                                     \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;        \
      for(xx=0; xx<SIZE; xx++, SPR+=(BPP/8)){                    \
         if(ReadWord(SPR) != emudx_transp)                       \
            blend50_16(&line[xx], ReadWord(SPR));                \
      }                                                          \
   }                                                             \
}                                                                \
                                                                 \
void Draw##SIZE##x##SIZE##_Trans_Alpha50_##BPP##_FlipX(ARG_DIR)  \
{                                                                \
   UINT##BPP *line;                                              \
   int xx,yy;                                                    \
                                                                 \
   for(yy=SIZE-1; yy>=0; yy--){                                  \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;        \
      for(xx=0; xx<SIZE; xx++, SPR+=(BPP/8)){                    \
         if(ReadWord(SPR) != emudx_transp)                       \
            blend50_16(&line[xx], ReadWord(SPR));                \
      }                                                          \
   }                                                             \
}                                                                \
                                                                 \
void Draw##SIZE##x##SIZE##_Trans_Alpha50_##BPP##_FlipY(ARG_DIR)  \
{                                                                \
   UINT##BPP *line;                                              \
   int xx,yy;                                                    \
                                                                 \
   for(yy=0; yy<SIZE; yy++){                                     \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;        \
      for(xx=SIZE-1; xx>=0; xx--, SPR+=(BPP/8)){                 \
         if(ReadWord(SPR) != emudx_transp)                       \
            blend50_16(&line[xx], ReadWord(SPR));                \
      }                                                          \
   }                                                             \
}                                                                \
                                                                 \
void Draw##SIZE##x##SIZE##_Trans_Alpha50_##BPP##_FlipXY(ARG_DIR) \
{                                                                \
   UINT##BPP *line;                                              \
   int xx,yy;                                                    \
                                                                 \
   for(yy=SIZE-1; yy>=0; yy--){                                  \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;        \
      for(xx=SIZE-1; xx>=0; xx--, SPR+=(BPP/8)){                 \
         if(ReadWord(SPR) != emudx_transp)                       \
            blend50_16(&line[xx], ReadWord(SPR));                \
      }                                                          \
   }                                                             \
}                                                                \
                                                                 \
void Draw##SIZE##x##SIZE##_Trans_Alpha_##BPP(ARG_DIR)            \
{                                                                \
   UINT##BPP *line;                                              \
   int xx,yy;                                                    \
                                                                 \
   for(yy=0; yy<SIZE; yy++){                                     \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;        \
      for(xx=0; xx<SIZE; xx++, SPR+=(BPP/8)){                    \
         if(ReadWord(SPR) != emudx_transp)                       \
            blend_16(&line[xx], ReadWord(SPR));                  \
      }                                                          \
   }                                                             \
}                                                                \
                                                                 \
void Draw##SIZE##x##SIZE##_Trans_Alpha_##BPP##_FlipX(ARG_DIR)    \
{                                                                \
   UINT##BPP *line;                                              \
   int xx,yy;                                                    \
                                                                 \
   for(yy=SIZE-1; yy>=0; yy--){                                  \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;        \
      for(xx=0; xx<SIZE; xx++, SPR+=(BPP/8)){                    \
         if(ReadWord(SPR) != emudx_transp)                       \
            blend_16(&line[xx], ReadWord(SPR));                  \
      }                                                          \
   }                                                             \
}                                                                \
                                                                 \
void Draw##SIZE##x##SIZE##_Trans_Alpha_##BPP##_FlipY(ARG_DIR)    \
{                                                                \
   UINT##BPP *line;                                              \
   int xx,yy;                                                    \
                                                                 \
   for(yy=0; yy<SIZE; yy++){                                     \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;        \
      for(xx=SIZE-1; xx>=0; xx--, SPR+=(BPP/8)){                 \
         if(ReadWord(SPR) != emudx_transp)                       \
            blend_16(&line[xx], ReadWord(SPR));                  \
      }                                                          \
   }                                                             \
}                                                                \
                                                                 \
void Draw##SIZE##x##SIZE##_Trans_Alpha_##BPP##_FlipXY(ARG_DIR)   \
{                                                                \
   UINT##BPP *line;                                              \
   int xx,yy;                                                    \
                                                                 \
   for(yy=SIZE-1; yy>=0; yy--){                                  \
      line = ((UINT##BPP *)(GameBitmap->line[y+yy])) + x;        \
      for(xx=SIZE-1; xx>=0; xx--, SPR+=(BPP/8)){                 \
         if(ReadWord(SPR) != emudx_transp)                       \
            blend_16(&line[xx], ReadWord(SPR));                  \
      }                                                          \
   }                                                             \
}

render(32,16);

// Draw16x16_Mask : just one 16x16 opaque block of the same colour

void Draw16x16_Mask(ARG_DIR)
{
   UINT8 *line;
   int xx,yy;
   UINT32 mask = cmap | (cmap << 8);
   mask |= (mask << 16);

   for(yy=0; yy<16; yy++){
      line = mask_bitmap->line[y+yy] + x;
      for(xx=0; xx<16; xx+=4){
         WriteLong(&line[x],mask);
      }
   }
}

// Draw16x16_Mask_Trans

void Draw16x16_Mask_Trans(ARG_DIR)
{
   UINT8 *line;
   int xx,yy;

   for(yy=0; yy<16; yy++){
      line = mask_bitmap->line[y+yy] + x;
      for(xx=0; xx<16; xx++, SPR++){
         if(*SPR)
            line[xx] = cmap;
      }
   }
}

void Draw16x16_Mask_Trans_FlipX(ARG_DIR)
{
   UINT8 *line;
   int xx,yy;

   for(yy=15; yy>=0; yy--){
      line = mask_bitmap->line[y+yy] + x;
      for(xx=0; xx<16; xx++, SPR++){
         if(*SPR)
            line[xx] = cmap;
      }
   }
}

void Draw16x16_Mask_Trans_FlipY(ARG_DIR)
{
   UINT8 *line;
   int xx,yy;

   for(yy=0; yy<16; yy++){
      line = mask_bitmap->line[y+yy] + x;
      for(xx=15; xx>=0; xx--, SPR++){
         if(*SPR)
            line[xx] = cmap;
      }
   }
}

void Draw16x16_Mask_Trans_FlipXY(ARG_DIR)
{
   UINT8 *line;
   int xx,yy;

   for(yy=15; yy>=0; yy--){
      line = mask_bitmap->line[y+yy] + x;
      for(xx=15; xx>=0; xx--, SPR++){
         if(*SPR)
            line[xx] = cmap;
      }
   }
}
