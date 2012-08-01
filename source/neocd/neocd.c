/* This is mostly inspired from my work on neocdpsp, and from the source
 * of the neogeo driver in mame (to help to clarify things at some places!)
 * and got some info from an old version of the ncdz emu (mainly the array
 * to get the short names of the games + the info about loading animations)
 *
 * Notice that the cd audio emulation is based on values in RAM !!! 
 * The upload area which was a big mystery in the psp version is now correct
 * at least for the writes, 
 * 
 */

#include "gameinc.h"
#include "sdl/dialogs/messagebox.h"
#include "pd4990a.h"
#include "files.h"
#include "2610intf.h"
#include "neocd.h"
#include "emumain.h" // reset_game_hardware
#include "cdrom.h"
#include "config.h"
#include "savegame.h"
#include "blit.h"
#include "zoom/16x16.h"
#include "cdda.h"
#include "dsw.h"
#include "profile.h"
#include "cache.h"
#include "default.h"
#include "hiscore.h"
#include "history.h"
#include "display.h"
#ifdef RAINE_DEBUG
#include "sdl/gui.h"
#endif
#include "sdl/control_internal.h"
#include "sdl/dialogs/fsel.h"
#include "sdl/gui.h"
#include "loadpng.h"
#include "games/gun.h"

#define DBG_RASTER 1
#define DBG_IRQ    2
#define DBG_LEVEL DBG_RASTER

#ifndef RAINE_DEBUG
#define debug
#else
void debug(int level, const char *format, ...)
{
    if (level & DBG_LEVEL) {
	va_list ap;
	va_start(ap,format);
	vprintf(format,ap);
	va_end(ap);
    }
}
#endif

#define NB_LINES 264 // lines on the screen (including borders)
#define START_SCREEN 36 // screen starts at this line (1st visible line)

static int capture_mode = 0,start_line,screen_cleared;
static int capture_block; // block to be shown...
int allowed_speed_hacks = 1,disable_irq1 = 0;
static int one_palette;
static int assigned_banks, current_bank;
int capture_new_pictures;
static BITMAP *raster_bitmap;
static void draw_neocd();
static void draw_sprites(int start, int end, int start_line, int end_line);

static struct VIDEO_INFO neocd_video =
{
  draw_neocd,
  320,
  224,
  16,
  VIDEO_ROTATE_NORMAL |
    VIDEO_ROTATABLE,
  NULL,
};

void restore_neocd_config() {
  allowed_speed_hacks = raine_get_config_int("neocd","allowed_speed_hacks",1);
  disable_irq1 = raine_get_config_int("neocd","disable_irq1",0);
  capture_new_pictures = raine_get_config_int("neocd","capture_new_pictures",0);
}

void save_neocd_config() {
  raine_set_config_int("neocd","allowed_speed_hacks",allowed_speed_hacks);
  raine_set_config_int("neocd","disable_irq1",disable_irq1);
  raine_set_config_int("neocd","capture_new_pictures",capture_new_pictures);
}

static void toggle_capture_mode() {
  capture_mode++;
  if (capture_mode == 2)
      one_palette = 1;
  else one_palette = 0;
  if (capture_mode > 2)
      capture_mode = 0;
}

static void prev_sprite_block() {
  switch (capture_mode) {
  case 1: if (capture_block > 0) capture_block--; break;
  case 2: if (current_bank > 0) current_bank--; break;
  }
}

static void next_sprite_block() {
  switch(capture_mode) {
  case 1: capture_block++; break;
  case 2: if (current_bank < assigned_banks) current_bank++; break;
  }
}

static void draw_sprites_capture(int start, int end, int start_line, 
	int end_line);

static FILE *fdata;
#define MAX_BANKS 256
static UINT8 banks[MAX_BANKS];

static void do_capture() {
    if (!capture_mode) return;
    int bpp = display_cfg.bpp;
    display_cfg.bpp = 8;
    current_game->video_info->flags |= VIDEO_NEEDS_8BPP;
    SetupScreenBitmap();
    set_colour_mapper(&col_Map_15bit_xRGBRRRRGGGGBBBB);
    one_palette = 1;
    if (capture_mode < 2)
	assigned_banks = current_bank = -1;
    do {
	ClearPaletteMap();
	ClearPaletteMap(); // 2 ClearPaletteMap, it's not an error
	// it's to clear both palettes in double buffer mode
	char filename[256];
	if (current_bank > -1)
	    sprintf(filename,"%ssavedata" SLASH "block%d-%d.map", dir_cfg.exe_path, capture_block,(capture_mode == 2 ? current_bank : current_bank+1));
	else
	    sprintf(filename,"%ssavedata" SLASH "block%d.map", dir_cfg.exe_path, capture_block);
	fdata = fopen(filename,"w");
	draw_sprites_capture(0,384,0,224);
	memset(&pal[16],1,sizeof(pal)-16*sizeof(SDL_Color));
	fclose(fdata);
	fdata = NULL;
	// memcpy(&pal[0],&pal[1],16*sizeof(SDL_Color)); // fix the palette for the
	pal[15].r = pal[15].g = pal[15].b = 1;
	/* Color 15 is background, always transparent for sprites.
	 * I set it to almost black (1,1,1), this color is impossible to render
	 * with the native neocd colors so there should be no color collision with
	 * it */
	if (current_bank > 0)
	    sprintf(filename,"%sblock%d-%d.png",dir_cfg.screen_dir,capture_block,current_bank);
	else
	    sprintf(filename,"%sblock%d.png",dir_cfg.screen_dir,capture_block);
	fdata = fopen(filename,"rb");
	if (fdata && capture_new_pictures) {
	    fclose(fdata);
	    char *s = strstr(filename,".png");
	    int nb = 0;
	    do {
		sprintf(s,"-%d.png",nb);
		fdata = fopen(filename,"rb");
		if (fdata) {
		    fclose(fdata);
		    nb++;
		}
	    } while (fdata); // find an unused name
	}
	save_png(filename,GameViewBitmap,pal);
	if (capture_mode == 2) break;
    } while (current_bank < assigned_banks);
    if (capture_mode < 2) one_palette = 0;
    display_cfg.bpp = bpp;
    current_game->video_info->flags &= ~VIDEO_NEEDS_8BPP;
    SetupScreenBitmap();
    set_colour_mapper(&col_Map_15bit_xRGBRRRRGGGGBBBB);
}

static struct DEF_INPUT_EMU list_emu[] =
{
 { SDLK_BACKSPACE,  0x00, "Toggle capture mode", toggle_capture_mode },
 { SDLK_RIGHT,      0x00, "Next sprite block", next_sprite_block     },
 { SDLK_LEFT,       0x00, "Prev sprite block", prev_sprite_block     },
 { SDLK_c,          0x00, "Capture block", do_capture },
};

#define FRAME_NEO  CPU_FRAME_MHz(12,60)
// neocd_path points to a neocd image, neocd_dir is the last path used for
// neocd files (which is not always an image path).
char neocd_path[1024],neocd_dir[1024];
char neocd_bios_file[1024];

static struct INPUT_INFO neocd_inputs[] = // 4 players, 3 buttons
{
  { KB_DEF_P1_UP, MSG_P1_UP, 0x00, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_P1_DOWN, MSG_P1_DOWN, 0x00, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P1_LEFT, MSG_P1_LEFT, 0x00, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_P1_RIGHT, MSG_P1_RIGHT, 0x00, 0x08, BIT_ACTIVE_0 },
  { KB_DEF_P1_B1, MSG_P1_B1, 0x00, 0x10, BIT_ACTIVE_0 },
  { KB_DEF_P1_B2, MSG_P1_B2, 0x00, 0x20, BIT_ACTIVE_0 },
  { KB_DEF_P1_B3, MSG_P1_B3, 0x00, 0x40, BIT_ACTIVE_0 },
  { KB_DEF_P1_B4, MSG_P1_B4, 0x00, 0x80, BIT_ACTIVE_0 },

  { KB_DEF_P2_UP, MSG_P2_UP, 0x02, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_P2_DOWN, MSG_P2_DOWN, 0x02, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P2_LEFT, MSG_P2_LEFT, 0x02, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_P2_RIGHT, MSG_P2_RIGHT, 0x02, 0x08, BIT_ACTIVE_0 },
  { KB_DEF_P2_B1, MSG_P2_B1, 0x02, 0x10, BIT_ACTIVE_0 },
  { KB_DEF_P2_B2, MSG_P2_B2, 0x02, 0x20, BIT_ACTIVE_0 },
  { KB_DEF_P2_B3, MSG_P2_B3, 0x02, 0x40, BIT_ACTIVE_0 },
  { KB_DEF_P2_B4, MSG_P2_B4, 0x02, 0x80, BIT_ACTIVE_0 },

  { KB_DEF_P1_START, MSG_P1_START, 0x04, 0x01, BIT_ACTIVE_0 },
  { KB_DEF_COIN1, MSG_COIN1, 0x04, 0x02, BIT_ACTIVE_0 },
  { KB_DEF_P2_START, MSG_P2_START, 0x04, 0x04, BIT_ACTIVE_0 },
  { KB_DEF_COIN2, MSG_COIN2, 0x04, 0x08, BIT_ACTIVE_0 },
  // Bit 4 (0x10) is 0 if the memory card is present !!!
  // neogeo doc :
  // bit 5 = mc 2 insertion status (0 = inserted)
  // bit 6 write protect 0 = write enable
  // bit 7 = neogeo mode : 0 = neogeo / 1 = mvs !!!

  { 0, NULL,        0,        0,    0            },
};

static int layer_id_data[2];

UINT8 *neocd_bios;

void setup_neocd_bios() {
  if (neocd_bios)
    return;
  neocd_bios = malloc(0x80000);
  // unsigned char rom_fix_usage[4096];
  int ret = 0;
  if (!*neocd_bios_file) {
      if (exists(get_shared("neocd.bin")))
	  strcpy(neocd_bios_file,get_shared("neocd.bin"));
      else if (exists(get_shared("neocd.zip")))
	  strcpy(neocd_bios_file,get_shared("neocd.zip"));
  }
  int tries = 0;
  do {
      if (*neocd_bios_file) {
	  if (!stricmp(&neocd_bios_file[strlen(neocd_bios_file)-3],"zip")) {
	      ret = load_zipped(neocd_bios_file, "neocd.bin", 0x80000, 0, neocd_bios, 1);
	  } else {
	      ret = load_file(neocd_bios_file,neocd_bios,0x80000);
	  }
      }
      if (!ret && !tries) {
	  char *exts[] = { "neocd.*", NULL };
	  *neocd_bios_file = 0;
	  fsel(dir_cfg.share_path,exts,neocd_bios_file,"Find Neocd bios");
	  if (!*neocd_bios_file) break;
	  tries++;
      } else
	  break;
  } while (1);

  if (!ret) {
      MessageBox("Fatal error", "Find the neocd bios (neocd.bin).\nAsk Google if you can't find it !","OK");
      exit(1);
  }

  // Check BIOS validity
  if (ReadWord(&neocd_bios[0xA822]) != 0x4BF9)
  {
    ErrorMsg("Fatal Error: Invalid BIOS file.");
    exit(1);;
  }

#if 1
  /*** Patch BIOS CDneocd_bios Check ***/
  // WriteWord(&neocd_bios[0xB040], 0x4E71);
  // WriteWord(&neocd_bios[0xB042], 0x4E71);
  /*** Patch BIOS upload command ***/
  // WriteWord(&neocd_bios[0x546], 0x60fe); // 0xFAC1);
  // WriteWord(&neocd_bios[0x548], 0x4E75);

  /*** Patch BIOS CDDA check ***/
  /* 	*((short*)(neogeo_rom_memory+0x56A)) = 0xFAC3; */
  /* 	*((short*)(neogeo_rom_memory+0x56C)) = 0x4E75; */

  // WriteWord(&neocd_bios[0x56a],0x60fe);
  /*** Full reset, please ***/
  WriteWord(&neocd_bios[0xA87A], 0x4239);
  WriteWord(&neocd_bios[0xA87C], 0x0010);
  WriteWord(&neocd_bios[0xA87E], 0xFDAE);

  // WriteWord(&neocd_bios[0xd736],0x4e75);

  /*** Trap exceptions ***/
  // WriteWord(&neocd_bios[0xA5B6], 0x4e70); // reset instruction !

#if 0
  WriteWord(&neocd_bios[0xA5B6], 0x4ef9); // reset instruction !
  WriteWord(&neocd_bios[0xA5B8], 0xc0); 
  WriteWord(&neocd_bios[0xA5Ba], 0xa822); 
#endif
#endif
}

static UINT16 result_code,sound_code,pending_command,*neogeo_vidram,video_modulo,video_pointer;
static UINT8 neogeo_memorycard[8192];
UINT8 *neogeo_fix_memory,*video_fix_usage,*video_spr_usage; 

static UINT8 temp_fix_usage[0x300],saved_fix;

void save_fix(int vidram) {
  // used by the cdrom before calling the animation functions...
  if (vidram) {
    memcpy(&RAM[0x110804],&neogeo_vidram[0x7000],0x500*2);
    // memset(&neogeo_vidram[0x7000],0,0x500*2);
  }
  memcpy(&RAM[0x115e06],neogeo_fix_memory,0x6000);
  memcpy(temp_fix_usage,video_fix_usage,0x300);
  saved_fix = 1;
}

void restore_fix(int vidram) {
  if (vidram)
    memcpy(&neogeo_vidram[0x7000],&RAM[0x110804],0x500*2); 
  memcpy(neogeo_fix_memory,&RAM[0x115e06],0x6000);
  memcpy(video_fix_usage,temp_fix_usage,0x300);
  saved_fix = 0;
}

// Save ram : the neogeo hardware seems to have a non volatile ram, but it
// was not mapped in neocdpsp, so I don't know if it's used or not...
// To be tested...

static UINT8 read_memorycard(UINT32 offset) {
  if ((offset & 1)) {
    return neogeo_memorycard[(offset & 0x3fff) >> 1];
  }
  return 0xff;
}

static UINT16 read_memorycardw(UINT32 offset) {
  return 0xff00 | neogeo_memorycard[(offset & 0x3fff) >> 1];
}

static int memcard_write;

static void write_memcard(UINT32 offset, UINT32 data) {
  if ((offset & 1)) {
    memcard_write = 1;
    neogeo_memorycard[(offset & 0x3fff) >> 1] = data;
  } 
}

static void write_memcardw(UINT32 offset, UINT32 data) {
  memcard_write = 1;
  neogeo_memorycard[(offset & 0x3fff) >> 1] = data;
}

static void set_res_code(UINT32 offset, UINT16 data) {
  result_code = data;
}

static UINT16 read_sound_cmd(UINT32 offset) {
  pending_command = 0;
  return sound_code;
}

static int z80_enabled,direct_fix,spr_disabled,fix_disabled,video_enabled;

static void write_sound_command(UINT32 offset, UINT16 data) {
  if (z80_enabled && RaineSoundCard) {
    pending_command = 1;
    sound_code = data;
    cpu_int_nmi(CPU_Z80_0);
#if 1
    // Very few games seem to need this, but Ironclad is one of them (you loose
    // the z80 just after choosing "start game" if you don't do this !)
    // Also mslug produces bad cd songs without this !!!
    ExitOnEI = 1;
    int ticks = dwElapsedTicks;
    cpu_execute_cycles(CPU_Z80_0, 60000);
    dwElapsedTicks = ticks; // say this never happened
    ExitOnEI = 0;
#endif
  }
}

static void write_sound_command_word(UINT32 offset, UINT16 data) {
  write_sound_command(offset,data >> 8);
}

static int cpu_readcoin(int addr)
{
  addr &= 0xFFFF;
  if (addr & 0x1) {
    // get calendar status - 2 bits
    /* Notice : lsb bits are not used in the neocd version, it's IN3 in mame
     * Here are the used bits :
     PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
     PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
     PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_SERVICE1 )
     PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_UNKNOWN )  // having this ACTIVE_HIGH causes you to start with 2 credits using USA bios roms
     PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_UNKNOWN ) // having this ACTIVE_HIGH causes you to start with 2 credits using USA bios roms
     PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_SPECIAL ) 
     */
    int coinflip = pd4990a_testbit_r(0);
    int databit = pd4990a_databit_r(0);
    return 0xff ^ (coinflip << 6) ^ (databit << 7);
  }
  {
    int res = result_code;

    if (RaineSoundCard)
    {
      if (pending_command)
	res &= 0x7f;
    }
    print_debug("read result_code %x sound_card %d\n",res,RaineSoundCard);

    return res;
  }
}

static struct {
  int start, control, pos; // flags for irq1
  int disable,wait_for_vbl_ack; // global / irq2
} irq;

static int neogeo_frame_counter_speed,raster_frame,neogeo_frame_counter,
	   scanline,watchdog_counter,
	   // irq3_pending,
	   display_position_interrupt_pending, vblank_interrupt_pending;

static void get_scanline() {
  if (!raster_frame) {
    // scanline isn't available, find it
    int cycles = s68000readOdometer() % FRAME_NEO;
    scanline = cycles * NB_LINES / FRAME_NEO;
  }
}

static void neo_irq1pos_w(int offset, UINT16 data)
{
  /* This is entierly from mame, so thanks to them ! */
  if (offset) {
    irq.pos = (irq.pos & 0xffff0000) | data;
    if (raster_frame) {
      print_debug("update irq.pos at line %d = %x\n",scanline,irq.pos);
    } else {
      print_debug("update irq.pos %x control %x\n",irq.pos,irq.control);
    }
    // irq.pos is in ticks of the neogeo pixel clock (6 MHz)
    // 1 tick happens for every pixel, not for every scanline.
    // So 1 scanline is 0x180 ticks, and you have 0x180*0x108 ticks for the
    // whole screen. Since the clock is at 6 MHz, this gives in 1s :
    // 6e6/(0x180*0x108) = 59.19 fps
    // Anyway for the current problem : some games like ridhero put 0x17d in
    // irq.pos to get 1 irq1 for every scanline. Since we use only a scanline
    // counter (irq.start), we must use (irq.pos + 3)/0x180 or we would not
    // get the interrupt repeat...
    if (irq.control & IRQ1CTRL_LOAD_LOW)
    {
      /* This version is from the neocdpsp code.
       * Clearly this is a rough approximation (the irq.pos value is not
       * compared at all to any clock), but it seems to work well enough ! */
      get_scanline();
      irq.start = irq.pos + 3;
      debug(DBG_RASTER,"irq.start = %d on scanline %d from irq.pos %x\n",irq.start,scanline,irq.pos);
      if (irq.pos == 0xffffffff)
	irq.start = -1;
    }
  } else {
    irq.pos = (irq.pos & 0x0000ffff) | (data << 16);
    if (raster_frame) {
      print_debug("update2 irq.pos at line %d = %x\n",scanline,irq.pos);
    } else {
      print_debug("update2 irq.pos %x\n",irq.pos);
    }
  }
}

static void update_interrupts(void)
{
  int level = 0;

  if (vblank_interrupt_pending) level = 2;
  if (display_position_interrupt_pending) level = 1;
  // if (irq3_pending) level = 3;

  /* either set or clear the appropriate lines */
  if (level) {
    if (irq.disable) {
	debug(DBG_IRQ,"update_interrupts: irqs disabled\n");
	return;
    }
    if (level == 2) {

       if (irq.wait_for_vbl_ack) {
	   // For some unknown reason this seems to be
	   // necessary for last blade 2
	   // Without this, the stack is filled !!!
	   debug(DBG_IRQ,"received vbl, still waiting for ack sr=%x\n",s68000context.sr);
	   return;
       } 
    }

    if (s68000context.interrupts[0] & (1<<level)) {
      debug(DBG_IRQ,"irq already pending, ignoring...\n");
    } else {
	debug(DBG_IRQ,"irq %d on line %d sr %x\n",level,scanline,s68000context.sr);
	cpu_interrupt(CPU_68K_0,level);
	if (level == 2) 
	    irq.wait_for_vbl_ack = 1;
    }
#if 0
    else
      /* Finally, the hblank neither : in super sidekicks 3, sometimes the
       * programs acks the irq, but not always !!! If we don't disable it here
       * then we make a stack overflow !!! */
      display_position_interrupt_pending = 0;
#endif
  }
  else {
#ifdef RAINE_DEBUG
    if (s68000context.interrupts[0] & 7) 
      print_debug("should be cleared. %x\n",s68000context.interrupts[0]); 
#endif
    s68000context.interrupts[0] &= ~7;
    irq.wait_for_vbl_ack = 0;
  }
}

static void write_videoreg(UINT32 offset, UINT32 data) {
    // Called LSPC in the neogeo api documentation
  switch((offset >> 1) & 7) {
    case 0x00: // Address register
      video_pointer = data; break;
    case 0x01:  // Write data register
      if (raster_frame && scanline > start_line && raster_bitmap &&
	      scanline < 224+START_SCREEN) {
	  // Must draw the upper part of the screen BEFORE changing the sprites
	  start_line -= START_SCREEN;
	  debug(DBG_RASTER,"draw_sprites between %d and %d\n",start_line,scanline-START_SCREEN);
	  draw_sprites(0,384,start_line,scanline-START_SCREEN);
	  debug(DBG_RASTER,"blit hbl from %d lines %d\n",start_line,scanline-START_SCREEN-start_line);
	  blit(GameBitmap,raster_bitmap,16,start_line+16,
		  0,start_line,
		  neocd_video.screen_x,
		  scanline-START_SCREEN-start_line);
	  start_line = scanline;
      }
      neogeo_vidram[video_pointer] = data;

      video_pointer += video_modulo;
      break;
    case 0x02: video_modulo = data; break; // Automatic increment register
    case 0x03: // Mode register
	       neogeo_frame_counter_speed=(data>>8)+1;
	       irq.control = data & 0xff;
	       debug(DBG_IRQ,"irq.control = %x\n",data);
	       break;
    case    4: neo_irq1pos_w(0,data); /* timer high register */    break;
    case    5: neo_irq1pos_w(1,data); /* timer low */    break;
    case    6:    /* IRQ acknowledge */
	       // if (data & 0x01) irq3_pending = 0;
	       debug(DBG_IRQ,"irq ack %d\n",data);
	       if (data & 0x02) display_position_interrupt_pending = 0;
	       if (data & 0x04) vblank_interrupt_pending = 0;
	       update_interrupts();
	       break;
    case 0x07: break; /* LSPC2 : timer stop switch - indicate wether the hbl
			 interrupt should also count the supplemental lines
			 in pal mode. AFAIK all the games are NTSC, this is
			 not supported.
			 Anyway, the doc says only bit 0 is used :
			 default value = 1
			 bit 0 = 0 timer counter is stopped when in pal mode
			 bit 0 = 1 when in pal, timer counter is stopped 32
			 horizontal lines
 */
  }
}

static UINT16 read_videoreg(UINT32 offset) {
  switch((offset >> 1) & 3) {
    case 0:
    case 1: return neogeo_vidram[video_pointer];
    case 2: return video_modulo;
    case 3: 
	    /*
	     * From mame :
	     *
	     The format of this very important location is:  AAAA AAAA A??? BCCC

	     A is the raster line counter. mosyougi relies solely on this to do the
	     raster effects on the title screen; sdodgeb loops waiting for the top
	     bit to be 1; zedblade heavily depends on it to work correctly (it
	     checks the top bit in the IRQ1 handler).
	     B is definitely a PAL/NTSC flag. Evidence:
	     1) trally changes the position of the speed indicator depending on
	     it (0 = lower 1 = higher).
	     2) samsho3 sets a variable to 60 when the bit is 0 and 50 when it's 1.
	     This is obviously the video refresh rate in Hz.
	     3) samsho3 sets another variable to 256 or 307. This could be the total
	     screen height (including vblank), or close to that.
	     Some games (e.g. lstbld2, samsho3) do this (or similar):
	     bclr    #$0, $3c000e.l
	     when the bit is set, so 3c000e (whose function is unknown) has to be
	     related
	     C animation counter lower 3 bits
	     */
	    /* Ok, so to sum up, there are 264 video lines, only 224 displayed
	     * and the counter goes from 0xf8 to 0x1ff with a twist... ! */
	    {
	      get_scanline();
	      int vcounter = scanline + 0xf8;
	      print_debug("access vcounter %d frame_counter %x from %x\n",vcounter,neogeo_frame_counter,s68000readPC());
	      debug(DBG_RASTER,"access vcounter %d frame_counter %x from pc=%x scanline=%d\n",vcounter,neogeo_frame_counter,s68000readPC(),scanline);

	      return (vcounter << 7) | (neogeo_frame_counter & 7);
	    }
  }
  return 0xffff;
}

/*************************************
 * From mame :
 *
 *  Watchdog
 *
 *
 *    - The watchdog timer will reset the system after ~0.13 seconds
 *     On an MV-1F MVS system, the following code was used to test:
 *        000100  203C 0001 4F51             MOVE.L   #0x14F51,D0
 *        000106  13C0 0030 0001             MOVE.B   D0,0x300001
 *        00010C  5380                       SUBQ.L   #1,D0
 *        00010E  64FC                       BCC.S    *-0x2 [0x10C]
 *        000110  13C0 0030 0001             MOVE.B   D0,0x300001
 *        000116  60F8                       BRA.S    *-0x6 [0x110]
 *     This code loops long enough to sometimes cause a reset, sometimes not.
 *     The move takes 16 cycles, subq 8, bcc 10 if taken and 8 if not taken, so:
 *     (0x14F51 * 18 + 14) cycles / 12000000 cycles per second = 0.128762 seconds
 *     Newer games force a reset using the following code (this from kof99):
 *        009CDA  203C 0003 0D40             MOVE.L   #0x30D40,D0
 *        009CE0  5380                       SUBQ.L   #1,D0
 *        009CE2  64FC                       BCC.S    *-0x2 [0x9CE0]
 *     Note however that there is a valid code path after this loop.
 *
 *     The watchdog is used as a form of protecetion on a number of games,
 *     previously this was implemented as a specific hack which locked a single
 *     address of SRAM.
 *
 *     What actually happens is if the game doesn't find valid data in the
 *     backup ram it will initialize it, then sit in a loop.  The watchdog
 *     should then reset the system while it is in this loop.  If the watchdog
 *     fails to reset the system the code will continue and set a value in
 *     backup ram to indiate that the protection check has failed.
 *
 *************************************/

static void watchdog_w(UINT32 offset, UINT16 data)
{
    /* Ok, maybe there is a watchdog after all, it takes 0.13s from mame to
     * reset the hardware, which is 8 frames. With 8, mslug resets at the
     * beginning of stage 3, 9 seems to be ok. I'll keep this value for now */
  watchdog_counter = 9;
}

void neogeo_set_screen_dark(UINT32 bit) {
  // not supported, is it really usefull ???
}

#define NEO_VECTORS 0x80

static UINT8 game_vectors[NEO_VECTORS], game_vectors_set;

static void    neogeo_select_bios_vectors (int bit) {
  if (bit) {
    print_debug("set game vectors\n");
    if (game_vectors_set) {
      print_debug("already set\n");
    } else {
      memcpy(RAM,game_vectors,NEO_VECTORS);
      game_vectors_set = 1;
    }
  } else {
    if (game_vectors_set) {
      memcpy(game_vectors,RAM,NEO_VECTORS);
      game_vectors_set = 0;
    }
    print_debug("set bios vectors\n");
    memcpy(RAM, neocd_bios, NEO_VECTORS);
  }
}

void update_game_vectors() {
  memcpy(game_vectors,RAM,NEO_VECTORS);
  game_vectors_set = 1;
  print_debug("game vectors updated\n");
}

static int fixed_layer_source;

static void neogeo_set_fixed_layer_source(UINT8 data)
{
  // This is used to select the gfx source (cartridge or bios)
  // so maybe it's not used in the neocd version ?
  fixed_layer_source = data;
  print_ingame(600,"layer_source %d",data);
}

static int palbank;
extern UINT8 *RAM_PAL;

static void neogeo_set_palette_bank(int bit) {
  if (palbank != bit) {
    palbank = bit;
    RAM_PAL = RAM + 0x230000 + 0x2000*palbank;
    set_68000_io(0,0x400000, 0x401fff, NULL, RAM_PAL);
    print_debug("palbank %d\n",bit);
  }
}

static int last_cdda_cmd, last_cdda_track;

static void restore_bank() {
  int new_bank = palbank;
  palbank = -1;
  neogeo_set_palette_bank(new_bank);
  print_debug("palette bank restored\n");
  // now also restore the loading progress status...
  neocd_lp.sectors_to_load = 0;
  if (neocd_lp.bytes_loaded) {
    // saved in the middle of loading a file, we'd better reload it !
    neocd_lp.file_to_load--;
    neocd_lp.bytes_loaded = 0;
  }
  if (neocd_lp.function) 
    current_game->exec_frame = &loading_progress_function;
  else
    current_game->exec_frame = &execute_neocd;
  // These last 2 should have been saved but I guess I can just reset them...
  last_cdda_cmd = 0;
  last_cdda_track = 0;
  restore_override(0);
}

static void system_control_w(UINT32 offset, UINT16 data)
{
  offset >>=1;
  UINT8 bit = (offset >> 3) & 0x01;

  switch (offset & 0x07)
  {
    default:
    case 0x00: neogeo_set_screen_dark(bit); break;
    case 0x01: neogeo_select_bios_vectors(bit); break;
    case 0x05: neogeo_set_fixed_layer_source(bit); break;
    // case 0x06: set_save_ram_unlock(bit); break;
    case 0x07: neogeo_set_palette_bank(bit); break;

    case 0x02: /* unknown - HC32 middle pin 1 */ // mc 1 write enable
    case 0x03: /* unknown - uPD4990 pin ? */     // mc 2 write enable
    case 0x04: /* unknown - HC32 middle pin 10 */ // mc register select/normal
	       // writes 0 here when the memory card has been detected
	       print_debug("PC: %x  Unmapped system control write.  Offset: %x  bank: %x data %x return %x ret2 %x\n", s68000readPC(), offset & 0x07, bit,data,ReadLongSc(&RAM[s68000context.areg[7]]),ReadLongSc(&RAM[s68000context.areg[7]+4]));
	       break;
  }
}

static char config_game_name[80];

static struct YM2610interface ym2610_interface =
{
  1,
  8000000,
  { (180|(OSD_PAN_CENTER << 8)) },
  { 0 },
  { 0 },
  { 0 },
  { 0 },
  { z80_irq_handler },	/* irq */
  { 0 },	/* delta_t */
  { 0 },	/* adpcm */
  { YM3012_VOL(255,OSD_PAN_LEFT,255,OSD_PAN_RIGHT) },
};

struct SOUND_INFO neocd_sound[] =
{
  { SOUND_YM2610,  &ym2610_interface,  },
  { 0,             NULL,               },
};

static void restore_memcard() {
  char path[1024];
  sprintf(path,"%ssavedata" SLASH "%s.bin", dir_cfg.exe_path, current_game->main_name); // 1st try game name in savedata
  FILE *f = fopen(path,"rb");
  memcard_write = 0;
  if (!f) {
    sprintf(path,"%s%smemcard.bin",neocd_dir,SLASH); // otherwise try this
    f = fopen(path,"rb");
  }
  if (f) {
    print_debug("memcard read from %s\n",path);
    fread(neogeo_memorycard,sizeof(neogeo_memorycard),1,f);
    fclose(f);
  }
}

static void save_memcard() {
  if (memcard_write) {
    char path[1024];
    sprintf(path,"%ssavedata" SLASH "%s.bin", dir_cfg.exe_path, current_game->main_name); // 1st try game name in savedata
    FILE *f = fopen(path,"wb");
    if (f) {
      fwrite(neogeo_memorycard,sizeof(neogeo_memorycard),1,f);
      fclose(f);
    }
    memcard_write = 0;
  }
}

int neocd_id;
static int offx, maxx;
/* The spr_disabled...video_enabled are usefull to clean up loading sequences
 * they are really used by the console, verified on a youtube video for aof3
 * without them, there are some flashing effects */

typedef struct {
  char *name;
  int id,width;
} NEOCD_GAME;

// There seems to be a majority of games using 304x224, so the default value
// for the width is 304 (when left blank).
const NEOCD_GAME games[] =
{
  { "nam1975",    0x0001 },
  { "bstars",     0x0002, 320 },
  { "tpgolf",     0x0003, 304 },
  { "mahretsu",   0x0004, },
  { "maglord",    0x0005, 320 },
  { "ridhero",    0x0006 },
  { "alpham2",    0x0007 },
  { "ncombat",    0x0009 },
  { "cyberlip",   0x0010 },
  { "superspy",   0x0011 },
  { "mutnat",     0x0014 },
  { "sengoku",    0x0017, 320 },
  { "burningf",   0x0018 },
  { "lbowling",   0x0019 },
  { "gpilots",    0x0020 },
  { "joyjoy",     0x0021 },
  { "bjourney",   0x0022, 320 },
  { "lresort",    0x0024 },
  { "2020bb",     0x0030 },
  { "socbrawl",   0x0031 },
  { "roboarmy",   0x0032 },
  { "fatfury",    0x0033 },
  { "fbfrenzy",   0x0034 },
  { "crswords",   0x0037 },
  { "rallych",    0x0038 },
  { "kotm2",      0x0039 },
  { "sengoku2",   0x0040 },
  { "bstars2",    0x0041 },
  { "3countb",    0x0043, 320 },
  { "aof",        0x0044, 304 },
  { "samsho",     0x0045, 320 },
  { "tophuntr",   0x0046, 320 },
  { "fatfury2",   0x0047, 320 },
  { "janshin",    0x0048 },
  { "androdun",   0x0049 },
  { "ncommand",   0x0050 },
  { "viewpoin",   0x0051 },
  { "ssideki",    0x0052 },
  { "wh1",        0x0053, 320 },
  { "crsword2",   0x0054 },
  { "kof94",      0x0055, 304 },
  { "aof2",       0x0056, 304 },
  { "wh2",        0x0057 },
  { "fatfursp",   0x0058, 320 },
  { "savagere",   0x0059 },
  { "ssideki2",   0x0061, 320 },
  { "samsho2",    0x0063, 320 },
  { "wh2j",       0x0064 },
  { "wjammers",   0x0065 },
  { "karnovr",    0x0066 },
  { "pspikes2",   0x0068, 320 },
  { "aodk",       0x0074 },
  { "sonicwi2",   0x0075, 320 },
  { "galaxyfg",   0x0078 },
  { "strhoop",    0x0079 },
  { "quizkof",    0x0080, 304 },
  { "ssideki3",   0x0081, 320 },
  { "doubledr",   0x0082, 320 },
  { "pbobblen",   0x0083, 320 },
  { "kof95",      0x0084, 304 },
  { "ssrpg",      0x0085 },
  { "samsho3",    0x0087 },
  { "stakwin",    0x0088, 320 },
  { "pulstar",    0x0089, 320 },
  { "whp",        0x0090, 320 },
  { "kabukikl",   0x0092, 320 },
  { "gowcaizr",   0x0094 },
  { "rbff1",      0x0095, 320 },
  { "aof3",       0x0096, 304 },
  { "sonicwi3",   0x0097, 320 },
  { "fromanc2",   0x0098 },
  { "turfmast",   0x0200 },
  { "mslug",      0x0201,304 },
  { "puzzledp",   0x0202 },
  { "mosyougi",   0x0203 },
  { "adkworld",   0x0204 },
  { "ngcdsp",     0x0205 },
  { "neomrdo",    0x0207 },
  { "zintrick",   0x0211 },
  { "overtop",    0x0212 },
  { "neodrift",   0x0213, 304 },
  { "kof96",      0x0214, 304 },
  { "ninjamas",   0x0217, 320 },
  { "ragnagrd",   0x0218, 320 },
  { "pgoal",      0x0219 },
  { "ironclad",   0x0220, 304 },
  { "magdrop2",   0x0221 },
  { "samsho4",    0x0222, 320 },
  { "rbffspec",   0x0223, 320 },
  { "twinspri",   0x0224 },
  { "kof96ngc",   0x0229 },
  { "breakers",   0x0230, 320 },
  { "kof97",      0x0232, 304 },
  { "lastblad",   0x0234, 320 },
  { "rbff2",      0x0240, 320 },
  { "mslug2",     0x0241 },
  { "kof98",      0x0242, 304 },
  { "lastbld2",   0x0243, 320 },
  { "kof99",      0x0251, 304 },
  { "fatfury3",   0x069c, 320 },
  { "neogeocd",   0x0000 },
  { NULL, 0 }
};

static const NEOCD_GAME *game;
static int current_neo_frame, desired_68k_speed, stopped_68k;

// isprint is broken in windows, they allow non printable characters !!!
#define ischar(x) ((x)>=32) //  && (x)<=127)

void neogeo_read_gamename(void)
{
  unsigned char	*Ptr;
  int	temp;

  int region_code = GetLanguageSwitch();
  Ptr = RAM + ReadLongSc(&RAM[0x116]+4*region_code);
  memcpy(config_game_name,Ptr,80);
  ByteSwap((UINT8*)config_game_name,80);

  for(temp=0;temp<80;temp++) {
    if (!ischar(config_game_name[temp])) {
      config_game_name[temp]=0;
      break;
    }
  }
  while (config_game_name[temp-1] == ' ')
    temp--;
  config_game_name[temp] = 0;
  temp = 0;
  while (config_game_name[temp] == ' ')
    temp++;
  if (temp)
    memcpy(config_game_name,&config_game_name[temp],strlen(config_game_name)-temp+1);
  print_debug("game name : %s\n",config_game_name);
  current_game->long_name = (char*)config_game_name;

  neocd_id = ReadWord(&RAM[0x108]);
  // get the short name based on the id. This info is from neocdz...
  game = &games[0];
  while (game->name && game->id != neocd_id)
    game++;
  if (game->id == neocd_id)
    current_game->main_name = game->name;
  else {
    print_debug("warning could not find short name for this game\n");
    current_game->main_name = "neocd"; // resets name in case we don't find
  }
  print_debug("main_name %s\n",current_game->main_name);
  if (memcard_write)
    save_memcard(); // called after a reset
  else
    restore_memcard(); // called after loading

  if (neocd_id == 0x48 || neocd_id == 0x0221) {
    desired_68k_speed = current_neo_frame; // no speed hack for mahjong quest
    // nor for magical drop 2 (it sets manually the vbl bit for the controls
    // on the main menu to work, which makes the speed hack much more complex!
  }

  /* update window title with game name */
  char neocd_wm_title[160];
  sprintf(neocd_wm_title,"NeoRaine - %s",config_game_name);
  SDL_WM_SetCaption(neocd_wm_title,neocd_wm_title);
}

static struct ROMSW_DATA romsw_data_neocd[] =
{
  { "Japan",           0x00 },
  { "USA",             0x01 },
  { "Europe",          0x02 },
  { NULL,                    0    },
};

static struct ROMSW_INFO neocd_romsw[] =
{
  { /* 6 */ 0x10FD83 , 0x2, romsw_data_neocd },
  // { 0xc00401, 0x2, romsw_data_neocd },
  { 0,        0,    NULL },
};

/* Draw entire Character Foreground */
void video_draw_fix(void)
{
  UINT16 x, y;
  UINT16 code, colour;
  UINT16 *fixarea=&neogeo_vidram[0x7002];
  UINT8 *map;

  for (y=0; y < 28; y++)
  {
    for (x = 0; x < 40; x++)
    {
      code = fixarea[x << 5];

      colour = (code&0xf000)>>12;
      code  &= 0xfff;

      // Since some part of the fix area is in the bios, it would be
      // a mess to convert it to the unpacked version, so I'll keep it packed
      // for now...
      if(video_fix_usage[code]) {
	// printf("%d,%d,%x,%x\n",x,y,fixarea[x << 5],0x7002+(x<<5));
	MAP_PALETTE_MAPPED_NEW(colour,16,map);
	/*	if (video_fix_usage[code] == 2)
		Draw8x8_Packed_Mapped_Rot(&neogeo_fix_memory[code<<5],x<<3,y<<3,map);
		else no opaque version for packed sprites !!! */
	Draw8x8_Trans_Packed_Mapped_Rot(&neogeo_fix_memory[code<<5],(x<<3)+offx,(y<<3)+16,map);
      }
    }
    fixarea++;
  }
}

static int mousex, mousey;

typedef struct {
    int x,y,sprite,attr,tileno,rzx,zy;
    char name[14];
} tsprite;

static void draw_sprites_capture(int start, int end, int start_line, int end_line) {
    // Version which draws only 1 specific block of sprites !
    int         sx =0,sy =0,oy =0,rows =0,zx = 1, rzy = 1;
    int         offs,count,y;
    int         tileatr,y_control,zoom_control;
    UINT16 tileno;
    char         fullmode=0;
    int         rzx=16,zy=0;
    UINT8 *map;
    int nb_block = 0,new_block;
    int bank = -1;
    int mx,my;
    int fixed_palette = 0;
    tsprite *sprites = NULL;
    int nb_sprites = 0,alloc_sprites = 0;

    GetMouseMickeys(&mx,&my);
    mousex += mx; if (mousex > 320-16) mousex = 320-16;
    if (mousex < -8) mousex = -8;
    mousey += my;
    if (mousey > 224-8) mousey = 224-8;
    if (mousey < -16) mousey = -16;
    // display gun at the end to see something !!!


    for (count=start; count<end;count++) {

	zoom_control = neogeo_vidram[0x8000 + count];
	y_control = neogeo_vidram[0x8200 + count];

	// If this bit is set this new column is placed next to last one
	if (y_control & 0x40) {
	    new_block = 0;
	    sx += (rzx);

	    // Get new zoom for this column
	    zx = (zoom_control >> 8)&0x0F;

	    sy = oy;
	} else {   // nope it is a new block
	    new_block = 1;
	    // Sprite scaling
	    sx = (neogeo_vidram[0x8400 + count]) >> 7;
	    sy = 0x1F0 - (y_control >> 7);
	    rows = y_control & 0x3f;
	    zx = (zoom_control >> 8)&0x0F;

	    rzy = (zoom_control & 0xff)+1;


	    // Number of tiles in this strip
	    if (rows == 0x20)
		fullmode = 1;
	    else if (rows >= 0x21)
		fullmode = 2;   // most games use 0x21, but
	    else
		fullmode = 0;   // Alpha Mission II uses 0x3f

	    if (sy > 0x100) sy -= 0x200;

	    if (fullmode == 2 || (fullmode == 1 && rzy == 0x100))
	    {
		while (sy < -16) sy += 2 * rzy;
	    }
	    oy = sy;

	    if(rows==0x21) rows=0x20;
	    else if(rzy!=0x100 && rows!=0) {
		rows=((rows*16*256)/rzy + 15)/16;
	    }

	    if(rows>0x20) rows=0x20;
	}

	rzx = zx+1;
	// skip if falls completely outside the screen
	if (sx >= 0x140 && sx <= 0x1f0) {
	    // printf("%d,%d,%d continue sur sx count %x\n",sx,sy,rzx,count);
	    continue;
	}

	if ( sx >= 0x1F0 )
	    sx -= 0x200;

	// No point doing anything if tile strip is 0
	if ((rows==0)|| sx < -offx || (sx>= maxx)) {
	    continue;
	}
	if (new_block && capture_mode==1) {
	    // check if sprite block is enabled only here, because there are lots
	    // of bad blocks in sprite ram usually, so we just skip them first
	    if (nb_block < capture_block) {
		// look for next start of block
		do {
		    count++;
		} while ((neogeo_vidram[0x8200 + count] & 0x40) && count < end);
		count--; // continue will increase count again...
		nb_block++;
		continue;
	    }
	    nb_block++;
	    if (nb_block > capture_block+1)
		break;
	}

	offs = count<<6;

	// TODO : eventually find the precise correspondance between rzy and zy, this
	// here is just a guess...
	zy = (rzy >> 4);

	// rows holds the number of tiles in each vertical multisprite block
	for (y=0; y < rows ;y++) {
	    tileno = neogeo_vidram[offs];
	    tileatr = neogeo_vidram[offs+1];
	    offs += 2;
	    if (y)
		// This is much more accurate for the zoomed bgs in aof/aof2
		sy = oy + (((rzy+1)*y)>>4);

	    if (!(irq.control & IRQ1CTRL_AUTOANIM_STOP)) {
		if (tileatr&0x8) {
		    // printf("animation tileno 8\n");
		    tileno = (tileno&~7)|(neogeo_frame_counter&7);
		} else if (tileatr&0x4) {
		    // printf("animation tileno 4\n");
		    tileno = (tileno&~3)|(neogeo_frame_counter&3);
		}
	    }

	    //         tileno &= 0x7FFF;
	    if (tileno>0x7FFF) {
		// printf("%d,%d continue sur tileno %x count %x\n",sx,sy,tileno,count);
		continue;
	    }

	    if (fullmode == 2 || (fullmode == 1 && rzy == 0xff))
	    {
		if (sy >= 248) {
		    sy -= 2 * (rzy + 1);
		}
	    }
	    else if (fullmode == 1)
	    {
		if (y >= 0x10) sy -= 2 * (rzy + 1);
	    }
	    else if (sy > 0x110) sy -= 0x200;

	    if (((tileatr>>8))&&(sy<end_line && sy+zy>=start_line) && video_spr_usage[tileno])
	    {
		if (one_palette) {
		    if (bank == -1) {
			if (current_bank < 0) {
			    bank = tileatr >> 8;
			    current_bank = assigned_banks = 0;
			    banks[current_bank] = bank;
			} else {
			    if (capture_mode == 1) {
				current_bank++;
				if (current_bank > assigned_banks) {
				    printf("banks error: %d > %d\n",current_bank,assigned_banks);
				    exit(1);
				}
			    }
			    bank = banks[current_bank];
			    if (bank != (tileatr >> 8))
				continue;
			}
		    } else if (bank != (tileatr >> 8)){ // 2nd palette of this sprite bank
			// store the new bank for next call (if we don't have it already)
			int new = tileatr >> 8;
			int n;
			int found = 0;
			for (n=0; n<=assigned_banks; n++) {
			    if (banks[n] == new) {
				found = 1;
				break;
			    }
			}
			if (found)
			    continue;
			assigned_banks++;
			if (assigned_banks == MAX_BANKS) {
			    printf("color banks overflow\n");
			    exit(1);
			}
			banks[assigned_banks] = tileatr >> 8;
			continue;
		    }
		    char *name;
		    int nb=-1;
		    get_cache_origin(SPR_TYPE,tileno<<8,&name,&nb);
		    if (fdata && nb > -1 && sx+offx+rzx >= 16 && sx+offx < 320+16) {
			if (nb_sprites == alloc_sprites) {
			    alloc_sprites += 20;
			    sprites = realloc(sprites,alloc_sprites*sizeof(tsprite));
			}
			tsprite *spr = &sprites[nb_sprites];
			spr->x = sx+offx-16;
			spr->y = sy;
			spr->sprite = nb/256;
			spr->tileno = tileno;
			spr->rzx = rzx;
			spr->zy = zy;
			spr->attr = tileatr & 3;
			strcpy(spr->name,name);
			nb_sprites++;

			put_override(SPR_TYPE,name,0);
		    }
		}
		MAP_PALETTE_MAPPED_NEW(
			(tileatr >> 8),
			16,
			map);
		if (one_palette) {
		    // in 8bpp, there is 1 reserved color (white)
		    // as a result, the color n is mapped to n+1, and we want a direct
		    // mapping here...
		    if (bitmap_color_depth(GameBitmap) == 8 && !fixed_palette) {
			fixed_palette = 1;
			printf("palette fix\n");
			PALETTE pal2;
			int n;
			for (n=0; n<16; n++) {
			    pal2[n] = pal[map[n]];
			    printf("pal %d = pal %d rgb %02x %02x %02x\n",n,map[n],
				    pal2[n].r,pal2[n].g,pal2[n].b);
			    map[n] = n;
			}
			memcpy(pal,pal2,sizeof(SDL_Color)*16);
			// clear screen with transparent color : color 0.
			clear_game_screen(0);
		    }
		} 
		if (sx+offx < 0) {
		    printf("bye: %d,%d rzx %d offx %d\n",sx+offx,sy+16,rzx,offx);
		    exit(1);
		}
		// printf("sprite %d,%d,%x\n",sx,sy,tileno);
		if (sx >= mousex && sx < mousex+16 && sy>= mousey && sy < mousey+16)
		    print_ingame(1,"%d,%d,%x",sx,sy,tileno);

		if (!fdata) {
		    if (video_spr_usage[tileno] == 2) // all solid
			Draw16x16_Mapped_ZoomXY_flip_Rot(&GFX[tileno<<8],sx+offx,sy+16,map,rzx,zy,tileatr & 3);
		    else
			Draw16x16_Trans_Mapped_ZoomXY_flip_Rot(&GFX[tileno<<8],sx+offx,sy+16,map,rzx,zy,tileatr & 3);
		}
	    } 
	}  // for y
    }  // for count
    if (nb_block <= capture_block && capture_block > 0) {
	capture_block = 0;
	draw_sprites_capture(start,end,start_line,end_line);
	return;
    }
    if (!raine_cfg.req_pause_game)
	print_ingame(1,"block %d",capture_block);
    if (!one_palette)
	disp_gun(0,mousex+offx+8,mousey+16+8);
    for (offs = 0x104000; offs <= 0x105000; offs+= 0x100)
	// MESSCONT, but the bytes are swapped...
	if (!strncmp((char*)&RAM[offs+4],"EMSSOCTN",8))
	    break;
    print_ingame(1,"offs: %x [%x] palbank %x",ReadLongSc(&RAM[offs+0x4c]),offs,current_bank);
    if (fdata && sprites) {
	int nb = nb_sprites-1;
	int nb2 = nb-1;
	while (nb > 0) {
	    tsprite *spr = &sprites[nb], *spr2 = &sprites[nb2];
	    if (abs(spr->x - spr2->x) < 16 && abs(spr->y - spr2->y) < 16) {
		// the 2 sprites overlap
		// in this case the last of the list is on top, so I remove nb2
		memmove(&sprites[nb2],&sprites[nb2+1],
			(nb_sprites-nb2)*sizeof(tsprite));
		nb--;
		nb_sprites--;
	    }
	    nb2--;
	    if (nb2 < 0) {
		nb--;
		nb2 = nb-1;
	    }
	}
	nb = nb_sprites-1;
	while (nb >= 0) {
	    tsprite *spr = &sprites[nb];
	    fprintf(fdata,"%d,%d,%x,%d,%s\n",spr->x,spr->y,spr->sprite,spr->attr,
		    spr->name);
	    if (video_spr_usage[tileno] == 2) // all solid
		Draw16x16_Mapped_ZoomXY_flip_Rot(&GFX[spr->tileno<<8],spr->x+16,
			spr->y+16,map,spr->rzx,spr->zy,spr->attr);
	    else
		Draw16x16_Trans_Mapped_ZoomXY_flip_Rot(&GFX[spr->tileno<<8],spr->x+16,
			spr->y+16,map,spr->rzx,spr->zy,spr->attr);
	    nb--;
	}
	free(sprites);
    }
}

static void draw_sprites(int start, int end, int start_line, int end_line) {
    if (!check_layer_enabled(layer_id_data[1])) return;
    if (end_line > 223) end_line = 223;
  if (capture_mode) return draw_sprites_capture(start,end,start_line,end_line);
  int         sx =0,sy =0,oy =0,rows =0,zx = 1, rzy = 1;
  int         offs,count,y;
  int         tileatr,y_control,zoom_control;
  UINT16 tileno;
  char         fullmode=0;
  int         rzx=16,zy=0;
  UINT8 *map;

  for (count=start; count<end;count++) {

    zoom_control = neogeo_vidram[0x8000 + count];
    y_control = neogeo_vidram[0x8200 + count];

    // If this bit is set this new column is placed next to last one
    if (y_control & 0x40) {
	if (rows == 0) continue; // chain on an erased 3d sprite
      sx += (rzx);

      // Get new zoom for this column
      zx = (zoom_control >> 8)&0x0F;

      sy = oy;
    } else {   // nope it is a new block
      // Sprite scaling
      sx = (neogeo_vidram[0x8400 + count]) >> 7;
      sy = 0x1F0 - (y_control >> 7);
      rows = y_control & 0x3f;
      if (rows == 0) continue;
      zx = (zoom_control >> 8)&0x0F;

      rzy = zoom_control & 0xff;

      // Number of tiles in this strip
      if (rows == 0x20)
	fullmode = 1;
      else if (rows >= 0x21)
	  // The neogeo api doc says it should be 0x21 (specific setting)
	fullmode = 2;   // most games use 0x21, but
      else
	fullmode = 0;   // Alpha Mission II uses 0x3f

      /* Super ugly hack : this 1 line fix for sy below is required for games
       * like neo drift out, but if you enable it without fullmode==2, then
       * the upper part of the playground is not drawn correctly in super
       * sidekicks 2 & 3 ! Absolutely no idea why it works like that for now!
       * So this fix reads something like "if the code is stupid enough to set
       * fullmode to 2 (which should not happen normally !), then fix its
       * coordinates this way.
       * Not generic at all, frustrating, but it works (afaik). */
      if (sy > 0x100 && fullmode==2) sy -= 0x200;

      if (fullmode == 2 || (fullmode == 1 && rzy == 0xff))
      {
	while (sy < -16) sy += 2 * (rzy + 1);
      }
      oy = sy;

      if(rows==0x21) rows=0x20;
      else if(rzy!=0xff && rows!=0) {
	rows=((rows*16*256)/(rzy+1) + 15)/16;
      }

      if(rows>0x20) rows=0x20;
    }

    rzx = zx+1;

    if ( sx >= 0x1F0 ) 
      sx -= 0x200;

    // No point doing anything if tile strip is 0
    if (sx < -offx || (sx>= maxx)) {
      continue;
    }

    offs = count<<6;

    // TODO : eventually find the precise correspondance between rzy ane zy, this
    // here is just a guess...
    if (rzy)
      zy = (rzy >> 4) + 1;
    else
      zy = 0;

    // rows holds the number of tiles in each vertical multisprite block
    for (y=0; y < rows ;y++) {
	// 100% specific to neocd : maximum possible sprites $80000
	// super sidekicks 2 draws the playground with bit $8000 set
	// the only way to see the playground is to use and $7fff
	// Plus rasters must be enabled
  	tileno = neogeo_vidram[offs] & 0x7fff;
      tileatr = neogeo_vidram[offs+1];
      offs += 2;
      if (y)
	// This is much more accurate for the zoomed bgs in aof/aof2
	sy = oy + (((rzy+1)*y)>>4);

      if (!(irq.control & IRQ1CTRL_AUTOANIM_STOP)) {
	if (tileatr&0x8) {
	  // printf("animation tileno 8\n");
	  tileno = (tileno&~7)|(neogeo_frame_counter&7);
	} else if (tileatr&0x4) {
	  // printf("animation tileno 4\n");
	  tileno = (tileno&~3)|(neogeo_frame_counter&3);
	}
      }

      if (fullmode == 2 || (fullmode == 1 && rzy == 0xff))
      {
	if (sy >= 248) {
	  sy -= 2 * (rzy + 1);
	}
      }
      else if (fullmode == 1)
      {
	if (y >= 0x10) sy -= 2 * (rzy + 1);
      }
      else if (sy > 0x110) sy -= 0x200;

      if (((tileatr>>8))&&(sy<=end_line && sy+zy>=start_line) && video_spr_usage[tileno])
      {
	MAP_PALETTE_MAPPED_NEW(
	    (tileatr >> 8),
	    16,
	    map);
	if (video_spr_usage[tileno] == 2) // all solid
	  Draw16x16_Mapped_ZoomXY_flip_Rot(&GFX[tileno<<8],sx+offx,sy+16,map,rzx,zy,tileatr & 3);
	else
	  Draw16x16_Trans_Mapped_ZoomXY_flip_Rot(&GFX[tileno<<8],sx+offx,sy+16,map,rzx,zy,tileatr & 3);
      } 
    }  // for y
  }  // for count
}

static void clear_screen() {
    UINT8 *map;
    if (screen_cleared)
	return;  
    screen_cleared = 1;
    /* Not totally sure the palette can be cleared here and not every
     * time the sprites are drawn during a raster interrupt.
     * The doc says the palette should be changed only during vbl to
     * avoid noise... we'll try, I didn't find any game so far changing
     * the palette during an hbl */
    ClearPaletteMap();
    /* Confirmed by neogeo doc : palette 255 for bg */
    MAP_PALETTE_MAPPED_NEW(
	    0xff,
	    16,
	    map);
    switch(display_cfg.bpp) {
    case 8: clear_game_screen(map[15]); break;
    case 15:
    case 16: clear_game_screen(ReadWord(&map[15*2])); break;
    case 32: clear_game_screen(ReadLong(&map[15*4])); break;
    }
}

static void draw_neocd() {
  static int fc;
  // Apparently there are only sprites to be drawn, zoomable and chainable
  // + an 8x8 text layer (fix) over them

  clear_screen();
  if (!video_enabled)
    return;

  if (raster_frame && start_line > 0) {
      blit(raster_bitmap,GameBitmap,0,0,16,16,neocd_video.screen_x,start_line);
      debug(DBG_RASTER,"draw_neocd: sprites disabled on raster frame blit until line %d\n",start_line);
  }
  int start = 0, end = 0x300 >> 1;
  if (!spr_disabled) {
    if (neocd_id == 0x0085 && !capture_mode) {
      // pseudo priority code specific to ssrpg (samurai spirits rpg)
      // it draws the sprites at the begining of the list at the end to have them
      // on top. This code is taken from the japenese source of ncd 0.5 !
      if ((neogeo_vidram[(0x8200 + 2)] & 0x40) == 0 &&
	  (neogeo_vidram[(0x8200 + 3)] & 0x40) != 0) {
	// if a block starts at count = 2, then starts to draw the sprites after
	// this block. This doesn't make any sense, but it seems to work fine
	// for ssrpg... really weird. There shouldn't be any specific code for a
	// game since it's a console... !!!
	start = 6 >> 1;
	while ((neogeo_vidram[0x8200 + start] & 0x40) != 0)
	  start++;
	if (start == 6 >> 1) start = 0;
      }

      do {
	draw_sprites(start,end,start_line,224);
	end = start;
	start = 0;
      } while (end != 0);
    } else
      draw_sprites(0, 384,start_line,224);
  }

  if (!(irq.control & IRQ1CTRL_AUTOANIM_STOP))
  {
    if (fc++ >= neogeo_frame_counter_speed) {
      neogeo_frame_counter++;
      fc=0;
    }
  }

  if (check_layer_enabled(layer_id_data[0]) && !fix_disabled) 
    video_draw_fix();
}

void draw_neocd_paused() {
    clear_screen();
    draw_sprites_capture(0,384,0,224);
    if (check_layer_enabled(layer_id_data[0]) && !fix_disabled) 
	video_draw_fix();
}

void set_neocd_exit_to(int code) {
  neocd_bios[0xad36] = code;
}

static void z80_enable(UINT32 offset, UINT8 data) {
  if (!data) {
    print_debug("z80_enable: reset z80\n");
    cpu_reset(CPU_Z80_0);
    reset_timers();
    z80_enabled = 0;
  } else {
    print_debug("z80_enable: received %x\n",data);
    z80_enabled = 1;
  }
}

static char *old_name;
static int region_code;

void spr_disable(UINT32 offset, UINT8 data) {
  spr_disabled = data;
}

void fix_disable(UINT32 offset, UINT8 data) {
  fix_disabled = data;
}

void video_enable(UINT32 offset, UINT8 data) {
  video_enabled = data;
}

static int frame_count;

static void neogeo_hreset(void)
{
  // The region_code can be set from the gui, even with an empty ram
  frame_count = 0;
  if (saved_fix)
    restore_fix(0);
  current_neo_frame = FRAME_NEO;
  region_code = GetLanguageSwitch();
  old_name = current_game->main_name;
  z80_enabled = 0;
  direct_fix = -1;
  fix_disabled = 0;
  spr_disabled = 0;
  video_enabled = 1;
  palbank = 0;
  memset(&irq,0,sizeof(irq));
  pd4990a_init();
  pending_command = sound_code = 0;
  last_cdda_cmd = 0;
  last_cdda_track = 0;
  /* Clear last bank of palette so that the bg color of screen is black
   * when you start the game (last color of last bank) */
  memset(RAM_PAL+0xff*0x20,0,0x20);

  video_modulo = video_pointer = 0;

  SetLanguageSwitch(region_code);

  neogeo_cdrom_load_title();
  WriteLongSc(&RAM[0x11c810], 0xc190e2); // default anime data for load progress
  // First time init
  M68000_context[0].pc = 0xc0a822;
  M68000_context[0].sr = 0x2700;
  M68000_context[0].areg[7] = 0x10F300;
  M68000_context[0].asp = 0x10F400;
  M68000_context[0].interrupts[0] = 0;
  s68000SetContext(&M68000_context[0]); 
  if (!neogeo_cdrom_process_ipl(NULL)) {
    ErrorMsg("Error: Error while processing IPL.TXT.\n");
    ClearDefault();
    return;
  }

  // is irq3 really usefull for neocd ??? I couldn't find any game where it
  // made a difference so far...
  // s68000interrupt(3, -1);
}

void postprocess_ipl() {
  // called at the end of process_ipl, to setup stuff before emulation really
  // starts. This has to be in a separate function because process_ipl can
  // now be called many times before really finishing to process ipl.txt.

  /* read game name */
  neogeo_read_gamename();

  watchdog_counter = 9;

  SetLanguageSwitch(region_code);
  if (old_name != current_game->main_name) {
    load_game_config();
    int region2 = GetLanguageSwitch();
    if (region2 != region_code)
      neogeo_read_gamename();
  }
  if (cdrom_speed) {
	  // Force clearing of the screen to be sure to erase the interface when
	  // changing the game resolution
	  int fps = raine_cfg.show_fps_mode;
	  raine_cfg.show_fps_mode = 0;
	  clear_ingame_message_list();
	  clear_game_screen(0);
	  DrawNormal();
	  raine_cfg.show_fps_mode = fps;
  }
  if (game->width == 320) {
    neocd_video.screen_x = 320;
    offx = 16;
    maxx = 320;
  } else {
    neocd_video.screen_x = 304;
    offx = 16-8;
    maxx = 320-8;
  }
  if (cdrom_speed && neocd_video.screen_x != GameBitmap->w) {
    /* If loading animations are enabled, then the game name is known only after
     * having started the emulation, so we must reset a few parameters at this
     * time */
    ScreenChange();
    hs_close();
    hs_open();
    hs_init();
    hist_open();
  }
  if (cdrom_speed)
    reset_ingame_timer();
}

/* Upload area : this area is NOT in the neogeo cartridge systems
 * it allows the 68k to access memory areas directly such as the z80 memory
 * the sprites memory and the fix memory to initialize them from the cd for
 * example. */

static int upload_type;
static UINT8 upload_param[0x10],dma_mode[9*2];

static void upload_type_w(UINT32 offset, UINT8 data) {
  print_debug("upload_type set to %x\n",data);
  upload_type = data;
}

static int get_upload_type() {
  // return a zone type suitable for the upload area from the upload type
  // This upload type is used for reading bytes from the z80 (instead of
  // whole blocks)
  int zone;
  switch(upload_type) {
    case 0: zone = SPR_TYPE; break;
    case 1: zone = PCM_TYPE; break;
    case 4: zone = Z80_TYPE; break;
    case 5: zone = FIX_TYPE; break;
  }
  return zone;
}

static int read_upload(int offset) {
  /* The read is confirmed at least during kof96 demo : it reads the main ram
   * (offset < 0x200000, zone 0) by the upload area instead of accessing
   * directly... so at least it shows this thing is really used after all ! */

  int zone = RAM[0x10FEDA ^ 1];
  int size = ReadLongSc(&RAM[0x10FEFC]);
  int bank = RAM[0x10FEDB ^ 1];
  if (size == 0 && upload_type != 0xff) {
    zone = get_upload_type();
    // this thing finally explains what these upload reads/writes occuring
    // every frame were for in some games : to communicate with the z80,
    // certainly to see if it has some cd commands in store.
  }
  // print_debug("read_upload: offset %x offset2 %x offset_dst %x zone %x bank %x size %x pc:%x\n",offset,offset2,offset_dst,zone,bank,size,s68000readPC());
  // int bank = m68k_read_memory_8(0x10FEDB);
  offset &= 0xfffff;

  switch (zone & 0xf) {
    case 0x00:  // /* 68000 */          return neogeo_prg_memory[offset^1];
      // return subcpu_memspace[(offset>>1)^1];
      if (offset < 0x200000)
	return RAM[offset^1];
      print_debug("read overflow\n");
      return 0xffff;

    case FIX_TYPE:
      offset >>=1;
      // the offsets are not verified, I don't know if this things needs to be
      // byteswapped or not
      int offsets[4] = { -16, -24, -0, -8 };
      if (offset < 23)
	return neogeo_fix_memory[offset ^ 1] | 0xff00;
      return neogeo_fix_memory[offset+offsets[offset & 3]] | 0xff00;
    case Z80_TYPE:
      if (offset < 0x20000) {
	return Z80ROM[offset >> 1];
      }
      return 0xff;
    case PCM_TYPE:
      offset = (offset>>1) + (bank<<19);
      if (offset > 0x100000) {
	print_debug("read_upload: pcm overflow\n");
	return 0xffff;
      }
      return PCMROM[offset] | 0xff00;
    default:
      //sprintf(mem_str,"read_upload unimplemented zone %x\n",zone);
      //debug_log(mem_str);
      print_debug("read_upload unmapped zone %x bank %x\n",zone,RAM[0x10FEDB ^ 1]);
      return -1;
  }
}

static void write_upload_word(UINT32 offset, UINT16 data) {
  /* Notice : the uploads are still NOT fully emulated, see the asm code at
   * c00546 for that. Mainly there are 2 methods of transfer depending on the
   * value of bit 4 of zone. But anyway this code seems to work with all
   * known games, so it's enough for me... */
  // Notice that interrupts must be disabled during an upload,
  // This is taken care of by irq.disable_w
  int zone = RAM[0x10FEDA ^ 1];
  int bank = RAM[0x10FEDB ^ 1];
  int offset2,size;
  size = ReadLongSc(&RAM[0x10FEFC]);
  if (size == 0 && upload_type != 0xff) {
    zone = get_upload_type();
    if (zone == Z80_TYPE) {
      // The z80 seems to be the only interesting area for bytes accesses
      // like this...
      offset &= 0x1ffff;
      offset >>= 1;
      Z80ROM[offset] = data;
      return;
    } else if (zone == PCM_TYPE) {
      offset = ((offset&0xfffff)>>1) + (bank<<19);
      if (offset < 0x100000) {
	PCMROM[offset] = data;
      } else {
	print_debug("overflow pcm write %x,%x\n",offset,data);
      }
      return;
    } else if (zone == FIX_TYPE) {
      /* This is really a hack. The only game I know which uses direct FIX
       * writes is overtop... It's silly really ! */
#if 1
      offset &= 0x3ffff;
      offset >>=1;
      static UINT8 tmp_fix[32];
      static int fill;
      fill++;
      tmp_fix[offset & 0x1f] = data;

      if (direct_fix == -1) {
	direct_fix = offset;
	fill = 1;
      } else if (offset - direct_fix == 31) {
	print_debug("direct fix conv ok %x fill %d from %x a0:%x\n",direct_fix,fill,s68000readPC(),s68000context.areg[0]);
	fix_conv(tmp_fix, neogeo_fix_memory + direct_fix, 32, video_fix_usage + (direct_fix>>5));
	direct_fix = -1;
      } else if (offset - direct_fix > 32 || offset - direct_fix < 0) {
	printf("problem receiving direct fix area offset %x direct_fix %x\n",offset,direct_fix);
	exit(1);
      }
#endif
      return;
    } else {
      print_debug("direct write to zone %d offset/2 = %x???\n",zone,offset/2);
    }
  }
  UINT8 *dest,*Source;

  if (size <= 0) {
    return;
  }
  offset2 = ReadLongSc(&RAM[0x10FEF8]);
  if (offset2 > 0xc00000) {
    offset2 -= 0xc00000;
    Source = neocd_bios + offset2;
  } else if (offset2 < 0x200000)
    Source = RAM + offset2;
  else {
    // never happens
    printf("offset source : %x ???\n",offset2);
    exit(1);
  }
  print_debug("upload_word offset direct %x zone %x\n",offset,zone);
  offset = ReadLongSc(&RAM[0x10FEF4]);
  // zone 2 starts from the end, but zone 0x12 starts from the start !!!
  // maybe it happens for the other areas as well (not confirmed yet)
  if (!(zone & 0x10) && ((zone & 0xf) != PAT_TYPE)) {
    // PAT_TYPE ignores bit 4, confirmed in bios
    if ((zone & 0xf) == PCM_TYPE) {
      if (offset + size*2 > 0x100000) {
	printf("offset %x + %x*2 > 0x100000\n",offset,size);
	exit(1);
      }
    } else
      // This offset is to be confirmed, I am not 100% sure that it is
      // used for all the zones, too lazy to check all the asm code...
      offset -= size;
    if (offset & 0xf0000000) {
      printf("offset < 0, returns... zone %x\n",zone);
      return;
    }
  }

  /* Awkward emulation of the upload area, the area used by the bios to transfer
   * different types of data to the system.
   * It's done with the help of some variables in RAM (instead of some hw
   * registers). Instead of emulating the transfers byte by byte, I try to
   * processs them as a whole, it makes much more sense for sprites for example
   * and is also more efficient. It might not work if a game tries to use this
   * area without using the bios, but I didn't find such a game yet ! */

  switch (zone & 0xf) {
    case    PRG_TYPE:
      if (offset > 0x200000) {
	// never happens neither
	printf("upload to outside the ram ??? %x\n",offset);
	exit(1);
      }
      dest = RAM + offset;
      print_debug("upload PRG src %x dest %x size %x\n",ReadLongSc(&RAM[0x10FEF8]),offset,size);
      memcpy(dest, Source, size);
      WriteLongSc( &RAM[0x10FEF4], offset+size );
      break;

    case SPR_TYPE: /* SPR */
      offset += (bank<<20);
      dest = GFX + offset*2;
      file_cache("upload",offset*2,size*2,SPR_TYPE); // for the savegames
      if (offset + size > 0x400000) {
	size = 0x400000 - offset;
	print_debug("warning: size fixed for sprite upload %d\n",size);
      }
      if (size > 0) {
	ByteSwap(Source,size);
	spr_conv(Source, dest, size, video_spr_usage+(offset>>7));
	ByteSwap(Source,size);
      }
      print_debug("upload SPR dest %x size %x\n",offset*2,size);

      offset += size;

      while (offset > 0x100000 )
      {
	bank++;
	offset -= 0x100000;
      }

      WriteLongSc( &RAM[0x10FEF4], offset );
      RAM[0x10FEDB ^ 1] = (bank>>8)&0xFF;
      RAM[0x10FEDC ^ 1] = bank&0xFF;

      break;
    case    FIX_TYPE:
      dest = neogeo_fix_memory + (offset>>1);
      if (ReadLongSc(&RAM[0x10FEF8]) < 0xc00000)
	ByteSwap(Source,size);
      fix_conv(Source, dest, size, video_fix_usage + (offset>>6));
      if (ReadLongSc(&RAM[0x10FEF8]) < 0xc00000)
	ByteSwap(Source,size);
      print_debug("upload FIX dest %x size %x from %x zone %x\n",offset,size,ReadLongSc(&RAM[0x10FEF8]),zone);
      file_cache("upload",offset/2,size,FIX_TYPE); // for the savegames

      offset += (size<<1);
      WriteLongSc( &RAM[0x10FEF4], offset);
      break;
    case    Z80_TYPE:    // Z80
      dest = Z80ROM + (offset>>1);
      print_debug("upload Z80 dest %x size %x\n",offset>>1,size);
      memcpy(dest,Source,size);
      ByteSwap(dest,size);
      WriteLongSc( &RAM[0x10FEF4], offset + (size<<1) );
      break;
    case    PAT_TYPE:    // Z80 patch
      print_debug("upload PAT offset %x bank %x size %x\n",offset,bank,size);
      neogeo_cdrom_apply_patch((short*)Source, (((bank*0x100000) + offset)/256)&0xFFFF);
      break;
    case    PCM_TYPE:
      offset = (offset>>1) + (bank<<19);
      file_cache("upload",offset,size,PCM_TYPE);
      dest = PCMROM + offset;
      if (offset + size > 0x100000) {
	print_debug("adjusting size for upload pcm area from %d\n",size);
	size = 0x100000 - offset;
      }

      memcpy(dest,Source,size);
      ByteSwap(dest,size);
      print_debug("upload PCM offset %x size %x\n",offset,size);

      // Mise � jour des valeurs
      offset = ReadLongSc(&RAM[ 0x10FEF4] ) + (size<<1);

      while (offset > 0x100000 )
      {
	bank++;
	offset -= 0x100000;
      }

      WriteLongSc( &RAM[0x10FEF4], offset );
      RAM[0x10FEDB ^ 1] = (bank>>8)&0xFF;
      RAM[0x10FEDC ^ 1] = bank&0xFF;
      break;
    default:
      //sprintf(mem_str,"write_upload_word unimplemented zone %x\n",zone);
      //debug_log(mem_str);
      print_debug("write_upload_word: unmapped zone %x bank %x\n",zone,bank);
      break;
  }
  WriteLongSc( &RAM[0x10FEFC], 0); // set the size to 0 to avoid to loop
  print_debug("upload size reset\n");
  upload_type = 0xff; // and be sure to disable this too in this case.
}

static void upload_cmd_w(UINT32 offset, UINT8 data) {
  if (data == 0x40) {
    int zone = RAM[0x10FEDA ^ 1];
    int size = ReadLongSc(&RAM[0x10FEFC]);
    print_debug("upload dma zone %x from %x size would be %d\n",zone,s68000readPC(),size);
    if (size) {
      // Actually this command is used also to clear the palette
      // using a pattern of 0, and the size is passed directly to the hw
      // register. I don't bother to emulate this for now since used colors
      // are of course initialised anyway.
      // In this case when we arrive here, size=0, and write_upload_word should
      // not be called.
      RAM[0x10FEDA ^ 1] = zone ^ 0x10;
      write_upload_word(0,0);
      RAM[0x10FEDA ^ 1] = zone;
    } else {
      int upload_src = ReadLongSc(&upload_param[0]);
      int upload_len = ReadLongSc(&upload_param[12]);
      UINT16 upload_fill = ReadWord(&upload_param[8]);
      UINT16 dma = ReadWord(&dma_mode[0]);
      int n;
      if (upload_len && upload_src) {
	if (dma == 0xffdd || dma == 0xffcd) {
	  // ffdd is fill with data word
	  // ffcd would be the same ??? not confirmed, see code at c08eca
	  // for example, it looks very much the same !!!
	  if (upload_src == 0x400000) {
	    print_debug("upload fill palette len %x fill %x from %x\n",upload_len,upload_fill,s68000readPC());
	    if (upload_len > 0x1000) {
	      upload_len = 0x1000;
	    }
	    for (n=0; upload_len > 0; n+=2, upload_len--) {
	      WriteWord(&RAM_PAL[n],upload_fill);
	    }
	  } else  if (upload_src < 0x200000) {
	    print_debug("upload fill ram src %x len %x fill %x from %x\n",upload_src,upload_len,upload_fill,s68000readPC());
	    for (n=0; upload_len > 0; n+=2, upload_len--) {
	      WriteWord(&RAM[upload_src+n],upload_fill);
	    }
	  } else if (upload_src == 0x800000) { // memory card !
	    print_debug("memory card fill len %x fill %x from %x\n",upload_len,upload_fill,s68000readPC());
	    if (upload_len > 8192/2) upload_len = 8192/2;
	    for (n=0; upload_len > 0; n+=2, upload_len--) {
	      WriteWord(&neogeo_memorycard[n],upload_fill);
	    }
	  } else {
	    print_debug("unknown fill %x upload_type %x from %x\n",upload_src,upload_type,s68000readPC());
	  }
	} else if (dma == 0xfef5) {
	  // fill the area with the address !!!
	  // this is used only by the bios to test, but anyway it's probably
	  // better to add the code for it so that I am sure it works...
	  if (upload_src == 0x400000) {
	    print_debug("upload fill palette len %x fill %x from %x\n",upload_len,upload_fill,s68000readPC());
	    if (upload_len > 0x2000/4) {
	      upload_len = 0x2000/4;
	    }
	    for (n=0; upload_len > 0; n+=4, upload_len--) {
	      WriteLongSc(&RAM_PAL[n],upload_src+n);
	    }
	  } else  if (upload_src < 0x200000) {
	    print_debug("upload fill ram src %x len %x fill %x from %x\n",upload_src,upload_len,upload_fill,s68000readPC());
	    for (n=0; upload_len > 0; n+=4, upload_len--) {
	      WriteLongSc(&RAM[upload_src+n],upload_src+n);
	    }
	  } else {
	    print_debug("unknown fill %x upload_type %x from %x dma %x\n",upload_src,upload_type,s68000readPC(),dma);
	  }
	} else {
	  print_debug("upload: unknown dma %x\n",dma);
	}
      }
    }
  }
}

static void write_upload(int offset, int data) {
  // int zone = RAM[0x10FEDA ^ 1];
  // int size = ReadLongSc(&RAM[0x10FEFC]);
  write_upload_word(offset,data);
}

static void load_files(UINT32 offset, UINT16 data) {
  offset = ReadLongSc(&RAM[0x10f6a0]);
  print_debug("load_files command %x from %x offset %x\n",data,s68000readPC(),offset);
  if (data == 0x550) {
    if (RAM[0x115a06 ^ 1]>32 && RAM[0x115a06 ^ 1] < 127) {
      neogeo_cdrom_load_files(&RAM[0x115a06]);
    } else {
      print_debug("load_files: name %x 10f6b5 %x sector h %x %x %x\n",RAM[0x115a06^1],RAM[0x10f6b5^1],RAM[0x76C8^1],RAM[0x76C9^1],RAM[0x76Ca^1]);
    }
    // irq.disable : during test mode when testing the cd, the bios disables
    // irqs, issues cd commands, and waits for irqs to come back. So I guess
    // the cd commands restore the irqs when they complete, maybe a specific
    // command does the job, but for now it's a guess...
    // I put it here and not in finish_load_files because it can happen even
    // without a filename buffer in the bios...
    irq.disable = 0;
  } else {
    int nb_sec = ReadLongSc(&RAM[0x10f688]);
    print_debug("load_files: unknown command, name %x 10f6b5 %x sector %x %x %x nb_sec %x\n",RAM[0x115a06^1],RAM[0x10f6b5^1],RAM[0x10f6C8^1],RAM[0x10f6C9^1],RAM[0x10f6Ca^1],nb_sec);
  }
}

static void test_end_loading(UINT32 offset,UINT16 data) {
  // This is a weird test for the end of the loading of files made by the bios
  // really this should be done differently, but I don't know most of the
  // commands sent to the cd, and it's too long to try to understand them...
  // To test this : metal slug / start game / choose misson 1, black screen.
  RAM[offset^1] = data; // normal write
  RAM[offset] = data; // duplicate
}


static void cdda_cmd(UINT32 offset, UINT8 data) {
  int track = RAM[0x10F6F9];
  RAM[0x10f6f6 ^ 1] = data;
  // printf("do_cdda %d,%d\n",data,track);
  if (data == 255 && auto_stop_cdda) {
    // apparently the program sends the 255 command just before loading
    // anything from cd (and also at the start of the game).
    // Default is to ignore this command, but if we want to pause the music
    // as the original console, then we must translate this to pause (2)
    data = 2;
  }
  if (data <= 7) {
    if (data != last_cdda_cmd || last_cdda_track != track) {
      print_debug("data : %d %d pc:%x\n",RAM[0x10f6f7],RAM[0x10F6F9],s68000readPC());
      last_cdda_cmd = data;
      last_cdda_track = track;
      do_cdda(data,RAM[0x10f6f8 ^ 1]);
    }
  }
}

void myStop68000(UINT32 adr, UINT8 data) {
  Stop68000(0,0);
  stopped_68k = 1;
}

static void disable_irq_w(UINT32 offset, UINT8 data) {
  irq.disable = data;
  print_debug("irq.disable %d\n",irq.disable);
}

static UINT16 read_reg(UINT32 offset) {
  if (offset == 0xff011c)  {
    // only bit 4 of upper byte is tested - setting it to 1 makes some games
    // to freeze after a few frames (tested on futsal)
    // return 0xefff;
    // lowest 2 bits are region, region 3 portugal might not be supported
    int region_code = GetLanguageSwitch();
    return 0xff | (region_code << 8);
  }
  print_debug("RW %x -> ffff\n",offset);
  return 0xffff;
}

/*
   static void write_region(UINT32 offset, UINT8 data) {
   printf("write byte %x,%x from %x\n",offset,data,s68000readPC());
   RAM[offset ^ 1] = data;
   }
   */

static void write_pal(UINT32 offset, UINT16 data) {
  /* There are REALLY mirrors of the palette, used by kof96ng at least,
   * see demo / story */
  offset &= 0x1fff;
  WriteWord(&RAM_PAL[offset],data);
/*  get_scanline();
  print_debug("write_pal %x,%x scanline %d\n",offset,data,scanline); */
} 

/*
static void write_byte(UINT32 offset, UINT8 data) {
  printf("writeb %x,%x from %x\n",offset, data,s68000readPC());
  RAM[offset ^ 1] = data;
}

static void write_word(UINT32 offset, UINT16 data) {
  printf("writew %x,%x from %x a0:%x\n",offset, data,s68000readPC(),s68000context.areg[0]);
  WriteWord(&RAM[offset],data);
}
*/

static void load_neocd() {
    raster_frame = 0;
  register_driver_emu_keys(list_emu,4);
  layer_id_data[0] = add_layer_info("FIX layer");
  layer_id_data[1] = add_layer_info("sprites layer");
  // force the screen to 320 to avoid glitches when starting a 320 game in 304
  neocd_video.screen_x = 320;
  current_game->long_name = "No game loaded yet";
  current_game->main_name = "neocd";
  desired_68k_speed = CPU_FRAME_MHz(24,60);
  init_cdda();
  init_load_type();
  upload_type = 0xff;
  memcard_write = 0;
  if (!neocd_bios)
    setup_neocd_bios(); // game was loaded from command line !
  clear_file_cache();
  setup_z80_frame(CPU_Z80_0,CPU_FRAME_MHz(4,60));
  RAMSize = 0x200000 + // main ram
    0x010000 + // z80 ram
    0x020000 + // video ram
    0x2000*2; // palette (2 banks)
  if(!(RAM=AllocateMem(RAMSize))) return;
  // if(!(save_ram=(UINT16*)AllocateMem(0x10000))) return; // not to be saved with the ram
  if(!(GFX=AllocateMem(0x800000))) return; // sprites data, not ram (unpacked)
  if(!(neogeo_fix_memory=AllocateMem(0x20000))) return; 
  if(!(video_fix_usage=AllocateMem(4096))) return; // 0x20000/32 (packed)
  if(!(video_spr_usage=AllocateMem(0x800000/0x100))) return;
  if(!(PCMROM=AllocateMem(0x100000))) return;
  memset(RAM,0,RAMSize);
  memset(video_fix_usage,0,4096);
  memset(video_spr_usage,0,0x8000);
  memset(neogeo_memorycard,0,sizeof(neogeo_memorycard));

  // manual init of the layers (for the sprites viewer)
  tile_list_count = 2;
  tile_list[0].width = tile_list[0].height = 8;
  tile_list[0].count = 4096;
  tile_list[0].data = neogeo_fix_memory;
  tile_list[0].mask = video_fix_usage;

  tile_list[1].width = tile_list[1].height = 16;
  tile_list[1].count = 0x8000;
  tile_list[1].mask = video_spr_usage;
  tile_list[1].data = GFX;

  Z80ROM = &RAM[0x200000];
  neogeo_vidram = (UINT16*)(RAM + 0x210000);
  memset(neogeo_vidram,0,0x20000);
  RAM_PAL = RAM + 0x230000;

  set_colour_mapper(&col_Map_15bit_xRGBRRRRGGGGBBBB);
  InitPaletteMap(RAM_PAL,0x100,0x10,0x8000);

  AddZ80AROMBase(Z80ROM, 0x0038, 0x0066);
  AddZ80ARW(0x0000, 0xffff, NULL, Z80ROM);

  AddZ80AWritePort(4, 4, YM2610_control_port_0_A_w, NULL);
  AddZ80AWritePort(5, 5, YM2610_data_port_0_A_w, NULL);
  AddZ80AWritePort(6, 6, YM2610_control_port_0_B_w, NULL);
  AddZ80AWritePort(7, 7, YM2610_data_port_0_B_w, NULL);
  /* Port 8 : NMI enable / acknowledge? (the data written doesn't matter)
   * Metal Slug Passes this 35, then 0 in sequence. After a
   * mission begins it switches to 1 */
  AddZ80AWritePort(0xc, 0xc, set_res_code, NULL);
  AddZ80AWritePort(0, 0xff, DefBadWritePortZ80, NULL);

  AddZ80AReadPort(0, 0, read_sound_cmd, NULL);
  AddZ80AReadPort(4, 4, YM2610_status_port_0_A_r, NULL);
  AddZ80AReadPort(5, 5, YM2610_read_port_0_r, NULL);
  AddZ80AReadPort(6, 6, YM2610_status_port_0_B_r, NULL);
  AddZ80AReadPort(0, 0xff, DefBadReadPortZ80, NULL);
  AddZ80AInit();

  AddMemFetch(0, 0x200000, RAM);
  AddMemFetch(0xc00000, 0xc7ffff, neocd_bios - 0xc00000);
  AddMemFetch(-1, -1, NULL);

  AddWriteByte(0x10f6f6, 0x10f6f6, cdda_cmd, NULL);
  AddWriteByte(0x10F651, 0x10F651, test_end_loading, NULL);

  AddRWBW(0, 0x200000, NULL, RAM);
  AddReadBW(0xc00000, 0xc7ffff, NULL,neocd_bios);
  AddReadByte(0x300000, 0x300000, NULL, &input_buffer[1]);
  AddWriteByte(0x300001, 0x300001, watchdog_w, NULL); 
  AddReadByte(0x320000, 0x320001, cpu_readcoin, NULL); 
  AddReadByte(0x340000, 0x340000, NULL, &input_buffer[3]);
  AddReadByte(0x380000, 0x380000, NULL, &input_buffer[5]);

  AddReadByte(0x800000, 0x80ffff, read_memorycard, NULL);
  AddReadWord(0x800000, 0x80ffff, read_memorycardw, NULL);
  AddWriteByte(0x800000, 0x80ffff, write_memcard, NULL);
  AddWriteWord(0x800000, 0x80ffff, write_memcardw, NULL);

  // No byte access supported to the LSPC (neogeo doc)
  AddReadWord(0x3c0000, 0x3c0007, read_videoreg, NULL);
  AddWriteWord(0x3c0000, 0x3c000f, write_videoreg, NULL);

  AddWriteByte(0x320000, 0x320001, write_sound_command, NULL);
  AddWriteWord(0x320000, 0x320000, write_sound_command_word, NULL);

  AddWriteBW(0x3a0000, 0x3a001f, system_control_w, NULL);
  /* Notes about the palette from neogeo doc :
   * should be accessed only during vbl to avoid noise on screen, by words
   * only. Well byte access can be allowed here, it can't harm */
  AddRWBW(0x400000, 0x401fff, NULL, RAM_PAL);
  AddWriteWord(0x402000, 0x4fffff, write_pal, NULL); // palette mirror !
  AddSaveData(SAVE_USER_0, (UINT8*)&palbank, sizeof(palbank));
  prepare_cdda_save(SAVE_USER_1);
  AddSaveData(SAVE_USER_2, (UINT8 *)&cdda, sizeof(cdda));
  // I should probably put all these variables in a struct to be cleaner...
  AddSaveData(SAVE_USER_3, (UINT8*)&z80_enabled, 
    ((UINT8*)&video_enabled)-((UINT8*)&z80_enabled)+sizeof(int));
  AddSaveData(SAVE_USER_4, (UINT8*)&irq, sizeof(irq));
  AddSaveData(SAVE_USER_5, (UINT8*)&neocd_lp, sizeof(neocd_lp));
  prepare_cache_save();
  AddLoadCallback(restore_bank);
  // is the save ram usefull ?!??? probably not with neocd...
#if 0
  AddWriteByte(0xd00000, 0xd0ffff, save_ram_wb, NULL);
  AddWriteWord(0xd00000, 0xd0ffff, save_ram_ww, NULL);
  AddReadBW(0xd00000, 0xd0ffff, NULL, (UINT8*)save_ram);
#endif
  AddReadBW(0xe00000,0xefffff, read_upload, NULL);
  AddWriteByte(0xe00000,0xefffff, write_upload, NULL);
  AddWriteWord(0xe00000,0xefffff, write_upload_word, NULL);

  // cdrom : there are probably some more adresses of interest in this area
  // but I found only this one so far (still missing the ones used to control
  // the cd audio from the bios when exiting from a game).
  AddReadBW(0xff0000, 0xffffff, read_reg, NULL);
  AddWriteWord(0xff0002, 0xff0003, load_files, NULL);
  AddWriteByte(0xff0061,0xff0061, upload_cmd_w, NULL);
  AddWriteWord(0xff0064,0xff0071, NULL, upload_param);
  AddWriteWord(0xff007e, 0xff008f, NULL, dma_mode);
  AddWriteByte(0xff0105, 0xff0105, upload_type_w, NULL);
  AddWriteByte(0xff0111, 0xff0111, spr_disable, NULL);
  AddWriteByte(0xff0115, 0xff0115, fix_disable, NULL);
  AddWriteByte(0xff0119, 0xff0119, video_enable, NULL);
  AddWriteByte(0xff016f,0xff016f, disable_irq_w, NULL);
  AddWriteByte(0xff0183, 0xff0183, z80_enable, NULL);
  // ff011c seems to be some kind of status, only bit 12 is tested but I
  // couldn't find what for, it doesn't seem to make any difference...
  // The ff0100 area seems to be related to the uploads, but there are many
  // adresses... there might be some kind of locking system, but no dma
  // apprently, it seems easier to emulate this from the ram area instead of
  // using these registers directly

  AddWriteByte(0xAA0000, 0xAA0001, myStop68000, NULL);			// Trap Idle 68000
  finish_conf_68000(0);
  // There doesn't seem to be any irq3 in the neocd, irqs are very different
  // here
  // irq3_pending = 1;

  init_16x16_zoom();
  set_reset_function(neogeo_hreset);
  memset(input_buffer,0xff,4);
  input_buffer[4] = 0xf; // clear bits for memory card
  result_code = 0;
  irq.control = 0;
}

static void apply_hack(int pc) {
  WriteWord(&RAM[pc],0x4239);
  WriteWord(&RAM[pc+2],0xaa);
  WriteWord(&RAM[pc+4],0);
  current_neo_frame = desired_68k_speed;
  print_ingame(60,"Applied speed hack");
  print_debug("Applied speed hack at %x\n",pc);
}

void neocd_function(int vector) {
  // This one is called by the cdrom emulation for the loading animations
  int adr;
  if (vector < 0x200000) {
    adr = ReadLongSc(&RAM[vector]);
    if (adr > 0xe00000 || (adr & 1)) {
      adr = 0;
    }
  } else
    adr = vector;
  if (adr == 0) {
    switch (vector) {
      case 0x11c808: // setup loading screen
	adr = 0xc0c760;
	print_debug("neocd_function default %x\n",adr);
	break;
      case 0x11c80c:
	adr = 0xc0c814; // load screen progress
	print_debug("neocd_function default %x\n",adr);
	break;
      default:
	printf("unknown vector %x\n",vector);
	exit(1);
    }
  }

  // The function executed here is something which is supposed to be called
  // by a jsr. But since we don't load the files sector/sector, it's unlikely
  // to be able to emulate this directly (maybe later).
  // Anyway for now we must setup a temporary environment to execute this
  // function...
  int pc = cpu_get_pc(CPU_68K_0);
  char buff[6];
  raster_frame = 0;
  if (pc < 0x200000) {
    memcpy(buff,&RAM[pc],6);
    WriteWord(&RAM[pc],0x4239); // stop 68000 here
    WriteWord(&RAM[pc+2],0xaa);
    WriteWord(&RAM[pc+4],0);
  } else if (pc > 0xc00000) {
    pc -= 0xc00000;
    memcpy(buff,&neocd_bios[pc],6);
    WriteWord(&neocd_bios[pc],0x4239); // stop 68000 here
    WriteWord(&neocd_bios[pc+2],0xaa);
    WriteWord(&neocd_bios[pc+4],0);
    pc += 0xc00000;
  }
  s68000GetContext(&M68000_context[0]);
  s68000context.pc = adr;
  s68000context.areg[5] = 0x108000;
  if (!s68000context.areg[7]) s68000context.areg[7] = 0x10F300;
  s68000context.areg[7] -= 4*8*2; // ???
  WriteLongSc(&RAM[s68000context.areg[7]],pc);
  cpu_execute_cycles(CPU_68K_0, CPU_FRAME_MHz(32,60));
  if (s68000context.pc != pc+6) {
    print_debug("*** got pc = %x instead of %x after frame 11c810:%x vector was %x\n",s68000context.pc,pc+6,ReadLongSc(&RAM[0x11c810]),adr);
    /* Last blade 2, just before the 1st fight : it tries to access some data
     * for the animation just after it loaded a prg over it. Well the prg
     * seems to have bad data, so it was probably supposed to read the data
     * just before the prg overwrote it. Well I really don't see how it's
     * possible for now. The easiest solution is to ignore the error, it's
     * harmless anyway. */
    // exit(1);
  }
  if (pc < 0x200000) {
    memcpy(&RAM[pc],buff,6);
  } else if (pc > 0xc00000) {
    memcpy(&neocd_bios[pc- 0xc00000],buff,6);
  }
  s68000SetContext(&M68000_context[0]);
}

void loading_progress_function() {
  static int frames,init_loaded;
  screen_cleared = 0;
  if (!cdrom_speed) // prevent the cdrom_speed to disappear in the middle of
    // loading !
    cdrom_speed = neocd_lp.initial_cdrom_speed;
  if (neocd_lp.file_to_load == 0) { // init
    spr_disable(0,1);
    fix_disable(0,0);
    video_enable(0,1);
    neocd_function(0x11c808);
  }

  if (neocd_lp.bytes_loaded) {
    cdrom_load_neocd();
  }
  if (neocd_lp.bytes_loaded && neocd_lp.sectors_to_load == 0) {
    // this can happen if cdrom_speed is changed in the middle of loading...
    printf("problem sectors_to_load = 0 and bytes_loaded = %d \n",neocd_lp.bytes_loaded);
    neocd_lp.sectors_to_load = 1;
  }
  if (neocd_lp.sectors_to_load <= 0) {
    switch(neocd_lp.function) {
      case 1:
	if (!neogeo_cdrom_process_ipl(&neocd_lp)) {
	  ErrorMsg("Error: Error while processing IPL.TXT.\n");
	  current_game->exec_frame = &execute_neocd;
	  ClearDefault();
	  return;
	}
	break;
      case 2:
	neogeo_cdrom_test_files(&RAM[0x115a06],&neocd_lp);
	break;
      default:
	printf("function code unknown for loading progress %d\n",
	    neocd_lp.function);
	exit(1);
    }
    frames = 1;
    init_loaded = neocd_lp.loaded_sectors;
  }

  if (neocd_lp.sectors_to_load > 0) {
    int nb = (cdrom_speed*(150000)/60)*frames/2048; // sectors loaded at this frame
    frames++;
    if (nb >= neocd_lp.sectors_to_load) {
      nb = neocd_lp.sectors_to_load;
      neocd_lp.sectors_to_load = 0;
    }
    neocd_lp.loaded_sectors = init_loaded + nb;
    // printf("loaded sectors %d/%d section %d\n",neocd_lp.loaded_sectors,neocd_lp.total_sectors,neocd_lp.sectors_to_load);
    UINT32 progress= ((neocd_lp.loaded_sectors * 0x8000) / neocd_lp.total_sectors) << 8;
    WriteLongSc(&RAM[0x10f690],progress);
    if (progress >= 0x800000)
      WriteLongSc(&RAM[0x10f690],0x800000);
    neocd_function(0x11c80c);

    RAM[0x10f793^1] = 0;
    neocd_function(0xc0c8b2);
    if (z80_enabled && RaineSoundCard) {
      execute_z80_audio_frame(); 
    }
  }
}

void execute_neocd() {
  /* This code is still more or less experimental
   * the idea is to detect when the hblank interrupt is needed (raster_frame)
   * and to change the handling accordingly to save cycles.
   * Not sure this thing is 100% correct */
  // 7db0(a5) test� par futsal ???
  // WriteWord(&RAM[0x10fe80],0xffff);

  // lab_0432 = cd_test ???
  // printf("765 %x 7656 %x\n",RAM[0x10f765^1],RAM[0x10f656^1]);
  // printf("cd loaded %x 76b9 %x\n",RAM[0x10fec4^1],RAM[0x76b9]); 
  // RAM[0x10fd97^1] = 15;

    int pc;

  stopped_68k = 0;
  screen_cleared = 0;
  start_line = START_SCREEN;
  if (raster_bitmap && bitmap_color_depth(raster_bitmap) !=
	  bitmap_color_depth(GameBitmap)) {
      destroy_bitmap(raster_bitmap);
      raster_bitmap = NULL;
  }

  if (!raster_bitmap)
      raster_bitmap = create_bitmap_ex(bitmap_color_depth(GameBitmap),
	      320,224);
  if ((irq.control & (IRQ1CTRL_ENABLE)) && !disable_irq1) {
      debug(DBG_RASTER,"raster frame\n");


      raster_frame = 1;
      clear_screen();
      for (scanline = 0; scanline < NB_LINES; scanline++, irq.start -= 0x180) {
	  /* From http://wiki.neogeodev.org/index.php?title=Display_timing
	   *  8 scanlines vertical sync pulse
	   16 scanlines top border
	   224 scanlines active display
	   16 scanlines bottom border

	   Well apparently in ridhero there is a border of 28 pixels and not 24
	   */

	  if (irq.start < 0x180 && (irq.control & IRQ1CTRL_ENABLE)) {
	      // irq.start is a timer, in pixels, 0x180 ticks / line
	      // so if irq.start < 0x180 then there is a timer interrupt on this
	      // line
	      if (irq.control & IRQ1CTRL_AUTOLOAD_REPEAT) {
		  if (irq.pos == 0xffffffff)
		      irq.start = -1;
		  else {
		      irq.start = irq.pos + 1;	/* ridhero gives 0x17d */
		  }
		  debug(DBG_RASTER,"irq1 autorepeat %d (scanline %d)\n",irq.start,scanline);
	      }

	      display_position_interrupt_pending = 1;
	      debug(DBG_RASTER,"hbl on %d interrupts %x\n",scanline,s68000context.interrupts[0]);
	  }

	  if (display_position_interrupt_pending || vblank_interrupt_pending) {
	      update_interrupts();
	      if (stopped_68k) 
		  stopped_68k = 0;
	  }
	  if (!stopped_68k)
	      cpu_execute_cycles(CPU_68K_0,200000/NB_LINES);
	  if (goto_debuger || (stopped_68k && !(irq.start >= 0x180))) {
	      // We are obliged to stay in the loop if an irq will follow even
	      // if it's out of screen, because it can set irq.control to
	      // reload hbl on vblank (case of super sidekicks 3).
	      printf("sortie raster frame sur speed hack, irq.start %d\n",irq.start);
	      break;
	  }
      }
  } else { // normal frame (no raster)
      // the 68k frame does not need to be sliced any longer, we
      // execute cycles on the z80 upon receiving a command !
      raster_frame = 0;
      cpu_execute_cycles(CPU_68K_0, current_neo_frame);
      if (allowed_speed_hacks) {
	  static int not_stopped_frames;
	  if (!stopped_68k && desired_68k_speed > current_neo_frame && frame_count++ > 60) {
	      pc = s68000readPC();

	      if (pc < 0x200000) {
		  // printf("testing speed hack... pc=%x pc: %x pc-6:%x\n",pc,ReadWord(&RAM[pc]),ReadWord(&RAM[pc-6]));
		  not_stopped_frames = 0;
		  if ((ReadWord(&RAM[pc]) == 0xb06e || ReadWord(&RAM[pc]) == 0x4a2d) &&
			  ReadWord(&RAM[pc+4]) == 0x67fa) {
		      apply_hack(pc);
		  } else if (ReadWord(&RAM[pc]) == 0x4a39 &&
			  ReadWord(&RAM[pc+6]) == 0x6bf8) { // tst.b/bmi
		      apply_hack(pc);
		      WriteWord(&RAM[pc+6],0x4e71); // nop
		  } else if (ReadWord(&RAM[pc]) == 0x6bf8 &&
			  ReadWord(&RAM[pc-6]) == 0x4a39) {
		      apply_hack(pc-6);
		      WriteWord(&RAM[pc],0x4e71);
		  } else if (ReadWord(&RAM[pc]) == 0x0839 &&
			  ReadWord(&RAM[pc+8]) == 0x66f2) {
		      apply_hack(pc);
		      WriteWord(&RAM[pc+6],0x4e71); // nop
		      WriteWord(&RAM[pc+8],0x4e71); // nop
		  } else if ((ReadWord(&RAM[pc]) == 0x67f8 || ReadWord(&RAM[pc]) == 0x66f8) &&
			  ReadWord(&RAM[pc-6]) == 0x4a79) { // TST / BEQ/BNE
		      apply_hack(pc-6);
		      WriteWord(&RAM[pc],0x4e71); // nop
		  }
	      }
	  } else if (current_neo_frame > FRAME_NEO && frame_count > 60) {
	      // speed hack missed again for some reason (savegames can do that)
	      if (not_stopped_frames++ >= 10)
		  current_neo_frame = FRAME_NEO;
	  }
      } // allowed_speed_hacks
  }
  start_line -= START_SCREEN;
  if (z80_enabled && !irq.disable && RaineSoundCard) {
      execute_z80_audio_frame();
  }
  vblank_interrupt_pending = 1;	   /* vertical blank */
  update_interrupts();
  if (irq.control & IRQ1CTRL_AUTOLOAD_VBLANK) {
      if (irq.pos == 0xffffffff)
	  irq.start = -1;
      else {
	  irq.start = irq.pos;	/* ridhero gives 0x17d */
      }
      debug(DBG_RASTER,"irq.start %d on vblank (irq.pos %x)\n",irq.start,irq.pos);
  } 
  /* Add a timer tick to the pd4990a */
  pd4990a_addretrace();
  if (s68000readPC() == 0xc0e602) { // start button
      // For start, irqs are disabled, maybe it expects them to come back
      // from the cd ?
	  Stop68000(0,0);
	  reset_game_hardware();
  } 
}

static void clear_neocd() {
  save_memcard();
  save_debug("neocd.bin",neocd_bios,0x80000,1);
  save_debug("ram.bin",RAM,0x200000,1);
  save_debug("z80",Z80ROM,0x10000,0);
  init_cdda();
#ifdef RAINE_DEBUG
  if (debug_mode)
    ByteSwap(neocd_bios,0x80000); // restore the bios for the next game
#endif
  if (raster_bitmap) {
      destroy_bitmap(raster_bitmap);
      raster_bitmap = NULL;
  }
}

struct GAME_MAIN game_neocd = 
{
  __FILE__, /* source_file */ \
    NULL, // dirs
  NULL, // roms
  neocd_inputs,
  NULL, // dsw
  neocd_romsw,

  load_neocd,
  clear_neocd,
  &neocd_video,
  execute_neocd,
  "neocd",
  "neocd",
  "",
  COMPANY_ID_SNK,
  NULL,
  1998,
  neocd_sound,
  GAME_SHOOT
};


