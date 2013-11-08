/**
 * Powder Toy - Main source
 *
 * Copyright (c) 2008 - 2011 Stanislaw Skowronek.
 * Copyright (c) 2010 - 2011 Simon Robertshaw
 * Copyright (c) 2010 - 2011 Skresanov Savely
 * Copyright (c) 2010 - 2011 Bryan Hoyle
 * Copyright (c) 2010 - 2011 Nathan Cousins
 * Copyright (c) 2010 - 2011 cracker64
 * Copyright (c) 2011 jacksonmj
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
#include <string.h>
#include <math.h>
#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>
#include <bzlib.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#ifdef WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(LIN32) || defined(LIN64)
#include "images.h"
#endif
#include "defines.h"
#include "misc.h"
#include "font.h"
#include "powder.h"
#include "gravity.h"
#include "graphics.h"
#include "powdergraphics.h"
#include "http.h"
#include "interface.h"
#include "md5.h"
#include "update.h"
#include "air.h"
#include "console.h"
#ifdef LUACONSOLE
#include "luaconsole.h"
#include "luascriptinterface.h"
#endif
#include "save.h"
#include "hud.h"
#include "benchmark.h"

#include "game/Brush.h"
#include "game/Menus.h"
#include "simulation/Simulation.h"
#include "simulation/Tool.h"

pixel *vid_buf;

#define NUM_SOUNDS 2
struct sample {
	Uint8 *data;
	Uint32 dpos;
	Uint32 dlen;
} sounds[NUM_SOUNDS];

void mixaudio(void *unused, Uint8 *stream, int len)
{
	int i;
	Uint32 amount;

	for ( i=0; i<NUM_SOUNDS; ++i ) {
		amount = (sounds[i].dlen-sounds[i].dpos);
		if ( amount > len ) {
			amount = len;
		}
		SDL_MixAudio(stream, &sounds[i].data[sounds[i].dpos], amount, SDL_MIX_MAXVOLUME);
		sounds[i].dpos += amount;
	}
}

//plays a .wav file (sounds must be enabled)
void play_sound(char *file)
{
	int index;
	SDL_AudioSpec wave;
	Uint8 *data;
	Uint32 dlen;
	SDL_AudioCVT cvt;

	if (!sound_enable) return;

	/* Look for an empty (or finished) sound slot */
	for ( index=0; index<NUM_SOUNDS; ++index ) {
		if ( sounds[index].dpos == sounds[index].dlen ) {
			break;
		}
	}
	if ( index == NUM_SOUNDS )
		return;

	/* Load the sound file and convert it to 16-bit stereo at 22kHz */
	if ( SDL_LoadWAV(file, &wave, &data, &dlen) == NULL ) {
		fprintf(stderr, "Couldn't load %s: %s\n", file, SDL_GetError());
		return;
	}
	SDL_BuildAudioCVT(&cvt, wave.format, wave.channels, wave.freq,
	                  AUDIO_S16,   2,             22050);
	cvt.buf = (Uint8*)malloc(dlen*cvt.len_mult);
	memcpy(cvt.buf, data, dlen);
	cvt.len = dlen;
	SDL_ConvertAudio(&cvt);
	SDL_FreeWAV(data);

	/* Put the sound data in the slot (it starts playing immediately) */
	if ( sounds[index].data ) {
		free(sounds[index].data);
	}
	SDL_LockAudio();
	sounds[index].data = cvt.buf;
	sounds[index].dlen = cvt.len_cvt;
	sounds[index].dpos = 0;
	SDL_UnlockAudio();
}

static const char *it_msg =
    "\blThe Powder Toy - Version " MTOS(SAVE_VERSION) "." MTOS(MINOR_VERSION) " - http://powdertoy.co.uk, irc.freenode.net #powder\n"
    "\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\x7F\n"
    "\brJ\bla\boc\bgo\btb\bb1\bp'\bws \bbMod version " MTOS(MOD_VERSION) "." MTOS(MOD_MINOR_VERSION) "\bg   Based off of c version 83.0\n"
    "\n"
    "\bgControl+C/V/X are Copy, Paste and cut respectively.\n"
    "\bgTo choose a material, hover over one of the icons on the right, it will show a selection of elements in that group.\n"
    "\bgPick your material from the menu using mouse left/right buttons.\n"
    "Draw freeform lines by dragging your mouse left/right button across the drawing area.\n"
    "Shift+drag will create straight lines of particles.\n"
    "Ctrl+drag will result in filled rectangles.\n"
    "Ctrl+Shift+click will flood-fill a closed area.\n"
    "Use the mouse scroll wheel, or '[' and ']', to change the tool size for particles.\n"
    "Middle click or Alt+Click to \"sample\" the particles.\n"
	"Ctrl+Z will act as Undo.\n"
    "\n\boUse 'Z' for a zoom tool. Click to make the drawable zoom window stay around. Use the wheel to change the zoom strength\n"
	"The spacebar can be used to pause and unpause physics.\n"
    "Use 'S' to save parts of the window as 'stamps'.\n"
    "'L' will load the most recent stamp, 'K' shows a library of stamps you saved.\n"
    "The numbers on the keyboard will change the display mode\n"
    "\n"
    "Contributors: \bgStanislaw K Skowronek (Designed the original Powder Toy),\n"
    "\bgSimon Robertshaw, Skresanov Savely, cracker64, Catelite, Bryan Hoyle, Nathan Cousins, jacksonmj,\n"
    "\bgFelix Wallin, Lieuwe Mosch, Anthony Boot, Matthew \"me4502\", MaksProg, jacob1, mniip\n"
	"Thanks to mniip for the update server for my mod\n"
    "\n"
    "\bgTo use online features such as saving, you need to register at: \brhttp://powdertoy.co.uk/Register.html\n"
    "\n"
    "\bt" MTOS(SAVE_VERSION) "." MTOS(MINOR_VERSION) "." MTOS(BUILD_NUM) " "
#ifdef X86
	"X86 "
#endif
#ifdef X86_SSE
	"X86_SSE "
#endif
#ifdef X86_SSE2
	"X86_SSE2 "
#endif
#ifdef X86_SSE3
	"X86_SSE3 "
#endif
#ifdef LIN32
	"LIN32 "
#endif
#ifdef LIN64
	"LIN64 "
#endif
#ifdef WIN32
	"WIN32 "
#endif
#ifdef MACOSX
	"MACOSX "
#endif
#ifdef LUACONSOLE
	"LUACONSOLE "
#endif
#ifdef GRAVFFT
	"GRAVFFT "
#endif
    ;

typedef struct
{
	int start, inc;
	pixel *vid;
} upstruc;

static const char *old_ver_msg = "A new version is available - click here!";
char new_message_msg[255];
float mheat = 0.0f;

int ptsaveOpenID = 0;
int saveURIOpen = 0;
char * saveDataOpen = NULL;
int saveDataOpenSize = 0;

#ifdef INTERNAL
	int vs = 0;
#endif
int do_open = 0;
int sys_pause = 0;
int sys_shortcuts = 1;
int legacy_enable = 0; //Used to disable new features such as heat, will be set by save.
int aheat_enable; //Ambient heat
int decorations_enable = 1;
int hud_enable = 1;
int active_menu = 0;
int last_active_menu = 0;
int last_fav_menu = SC_FAV;
int framerender = 0;
int pretty_powder = 0;
char edgeMode = 0;
int amd = 1;
int MSIGN =-1;
int frameidx = 0;
int limitFPS = 60;
int main_loop = 1;
int sound_enable = 0;
std::string favMenu[18];
int finding = 0;
int locked = 0;
int highesttemp = MAX_TEMP;
int lowesttemp = MIN_TEMP;
int heatmode = 0;
int maxframes = 25;
int secret_els = 0;
int save_as = 5;
void *prop_value = 0;
int prop_format = 0;
unsigned int prop_offset = 0;
int tab_num = 1;
int num_tabs = 1;
int show_tabs = 0;
Tool* activeTools[3];
Tool* regularTools[3];
Tool* decoTools[3];
int activeToolID = 0;
float toolStrength = 1.0f;
int autosave = 0;
int realistic = 0;
int unlockedstuff = 0x08;
int old_menu = 0;
int loop_time = 0;
unsigned int decocolor = (255<<24)|PIXRGB(255,0,0);
int doUpdates = 2;

int drawinfo = 0;
int elapsedTime = 0;
int currentTime = 0;
int totaltime = 0;
int totalafktime = 0;
int afktime = 0;
double totalfps = 0;
int frames = 0;
double maxfps = 0;
int prevafktime = 0;
int timesplayed = 0;

int console_mode;
bool REPLACE_MODE = false;
bool SPECIFIC_DELETE = false;
int GRID_MODE;
int DEBUG_MODE;
int debug_flags = 0;
int debug_perf_istart = 1;
int debug_perf_iend = 0;
long debug_perf_frametime[DEBUG_PERF_FRAMECOUNT];
long debug_perf_partitime[DEBUG_PERF_FRAMECOUNT];
long debug_perf_time = 0;

sign signs[MAXSIGNS];

int numCores = 4;

int core_count()
{
	int numCPU = 1;
#ifdef MT
#ifdef WIN32
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	numCPU = sysinfo.dwNumberOfProcessors;
#else
#ifdef MACOSX
	numCPU = 4;
#else
	numCPU = sysconf( _SC_NPROCESSORS_ONLN );
#endif
#endif

	printf("Cpus: %d\n", numCPU);
	if (numCPU>1)
		printf("Multithreading enabled\n");
	else
		printf("Multithreading disabled\n");
#endif
	return numCPU;
}

int mousex = 0, mousey = 0, lastx = 0, lasty = 0;  //They contain mouse position
int kiosk_enable = 0;

void sdl_seticon(void)
{
#ifdef WIN32
	//SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(app_icon_w32, 32, 32, 32, 128, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	//SDL_WM_SetIcon(icon, NULL/*app_icon_mask*/);
#else
#ifdef MACOSX
	//SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(app_icon_w32, 32, 32, 32, 128, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	//SDL_WM_SetIcon(icon, NULL/*app_icon_mask*/);
#else
	SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(app_icon, 16, 16, 32, 64, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	SDL_WM_SetIcon(icon, NULL/*app_icon_mask*/);
#endif
#endif
}

int frame_idx=0;
void dump_frame(pixel *src, int w, int h, int pitch)
{
	char frame_name[32];
	int j,i;
	unsigned char c[3];
	FILE *f;
	sprintf(frame_name,"frame%04d.ppm",frame_idx);
	f=fopen(frame_name,"wb");
	fprintf(f,"P6\n%d %d\n255\n",w,h);
	for (j=0; j<h; j++)
	{
		for (i=0; i<w; i++)
		{
			c[0] = PIXR(src[i]);
			c[1] = PIXG(src[i]);
			c[2] = PIXB(src[i]);
			fwrite(c,3,1,f);
		}
		src+=pitch;
	}
	fclose(f);
	frame_idx++;
}

void clear_sim()
{
	int i,x, y;
	for (i=0; i<NPART; i++)
	{
		if (parts[i].animations)
		{
			free(parts[i].animations);
			parts[i].animations = NULL;
		}
	}
	memset(bmap, 0, sizeof(bmap));
	memset(emap, 0, sizeof(emap));
	memset(signs, 0, sizeof(signs));
	MSIGN = -1;
	memset(parts, 0, sizeof(particle)*NPART);
	for (i=0; i<NPART-1; i++)
		parts[i].life = i+1;
	parts[NPART-1].life = -1;
	globalSim->pfree = 0;
	parts_lastActiveIndex = 0;
	memset(pmap, 0, sizeof(pmap));
	memset(pv, 0, sizeof(pv));
	memset(vx, 0, sizeof(vx));
	memset(vy, 0, sizeof(vy));
	memset(fvx, 0, sizeof(fvx));
	memset(fvy, 0, sizeof(fvy));
	make_kernel();
	memset(photons, 0, sizeof(photons));
	memset(wireless, 0, sizeof(wireless));
	memset(gol2, 0, sizeof(gol2));
	memset(portalp, 0, sizeof(portalp));
	memset(fighters, 0, sizeof(fighters));
	fighcount = 0;
	ISSPAWN1 = ISSPAWN2 = 0;
	player.spwn = 0;
	player2.spwn = 0;
	emp_decor = 0;
	memset(pers_bg, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
	memset(fire_r, 0, sizeof(fire_r));
	memset(fire_g, 0, sizeof(fire_g));
	memset(fire_b, 0, sizeof(fire_b));
	memset(fire_alpha, 0, sizeof(fire_alpha));
	prepare_alpha(CELL, 1.0f);
	if(gravmask)
		memset(gravmask, 0xFFFFFFFF, (XRES/CELL)*(YRES/CELL)*sizeof(unsigned));
	if(gravy)
		memset(gravy, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	if(gravx)
		memset(gravx, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	if(gravp)
		memset(gravp, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	gravity_cleared = 1;
	for(x = 0; x < XRES/CELL; x++){
		for(y = 0; y < YRES/CELL; y++){
			hv[y][x] = outside_temp; //Set to room temperature (or whatever the default air temp was changed to)
		}
	}
	numballs = 0;
	memset(msindex, 0, sizeof(msindex));
	memset(msnum, 0, sizeof(msnum));
	memset(msvx, 0, sizeof(msvx));
	memset(msvy, 0, sizeof(msvy));
	memset(msrotation, 0, sizeof(msrotation));
	memset(newmsrotation, 0, sizeof(newmsrotation));
	finding &= 0x8;
	mod_save = MOD_SAVE_VERSION;
	if(edgeMode == 1)
		draw_bframe();
}

void NewSim()
{
	clear_sim();
	legacy_enable = 0;
	svf_filename[0] = 0;
	svf_fileopen = 0;
	svf_myvote = 0;
	svf_open = 0;
	svf_publish = 0;
	svf_modsave = 0;
	svf_own = 0;
	svf_id[0] = 0;
	svf_name[0] = 0;
	svf_tags[0] = 0;
	svf_description[0] = 0;
	svf_author[0] = 0;
	gravityMode = 0;
	airMode = 0;
	if (svf_last)
		free(svf_last);
	svf_last = NULL;
	svf_lsize = 0;
}

// stamps library

stamp stamps[STAMP_MAX];//[STAMP_X*STAMP_Y];

int stamp_count = 0;

unsigned last_time=0, last_name=0;
void stamp_gen_name(char *fn)
{
	unsigned t=(unsigned)time(NULL);

	if (last_time!=t)
	{
		last_time=t;
		last_name=0;
	}
	else
		last_name++;

	sprintf(fn, "%08x%02x", last_time, last_name);
}

void stamp_update(void)
{
	FILE *f;
	int i;
	f=fopen("stamps" PATH_SEP "stamps.def", "wb");
	if (!f)
		return;
	for (i=0; i<STAMP_MAX; i++)
	{
		if (!stamps[i].name[0])
			break;
		if (stamps[i].dodelete!=1)
		{
			fwrite(stamps[i].name, 1, 10, f);
		}
		else
		{
			char name[30] = {0};
			sprintf(name,"stamps%s%s.stm",PATH_SEP,stamps[i].name);
			remove(name);
		}
	}
	fclose(f);
}

void stamp_gen_thumb(int i)
{
	char fn[64];
	void *data;
	int size, factor_x, factor_y;
	pixel *tmp;

	if (stamps[i].thumb)
	{
		free(stamps[i].thumb);
		stamps[i].thumb = NULL;
	}

	sprintf(fn, "stamps" PATH_SEP "%s.stm", stamps[i].name);
	data = file_load(fn, &size);

	if (data)
	{
		stamps[i].thumb = prerender_save(data, size, &(stamps[i].thumb_w), &(stamps[i].thumb_h));
		if (stamps[i].thumb && (stamps[i].thumb_w>XRES/GRID_S || stamps[i].thumb_h>YRES/GRID_S))
		{
			factor_x = (int)ceil((float)stamps[i].thumb_w/(float)(XRES/GRID_S));
			factor_y = (int)ceil((float)stamps[i].thumb_h/(float)(YRES/GRID_S));
			if (factor_y > factor_x)
				factor_x = factor_y;
			tmp = rescale_img(stamps[i].thumb, stamps[i].thumb_w, stamps[i].thumb_h, &(stamps[i].thumb_w), &(stamps[i].thumb_h), factor_x);
			free(stamps[i].thumb);
			stamps[i].thumb = tmp;
		}
	}

	free(data);
}

int clipboard_ready = 0;
void *clipboard_data = 0;
int clipboard_length = 0;

char* stamp_save(int x, int y, int w, int h)
{
	FILE *f;
	int n;
	char fn[64], sn[16];
	void *s=build_save(&n, x, y, w, h, bmap, vx, vy, pv, fvx, fvy, signs, parts, (sdl_mod & KMOD_SHIFT));
	if (!s)
		return NULL;

#ifdef WIN32
	_mkdir("stamps");
#else
	mkdir("stamps", 0755);
#endif

	stamp_gen_name(sn);
	sprintf(fn, "stamps" PATH_SEP "%s.stm", sn);

	f = fopen(fn, "wb");
	if (!f)
		return NULL;
	fwrite(s, n, 1, f);
	fclose(f);

	free(s);

	if (stamps[STAMP_MAX-1].thumb)
		free(stamps[STAMP_MAX-1].thumb);
	memmove(stamps+1, stamps, sizeof(struct stamp)*(STAMP_MAX-1));
	memset(stamps, 0, sizeof(struct stamp));
	if (stamp_count<STAMP_MAX)
		stamp_count++;

	strcpy(stamps[0].name, sn);
	stamp_gen_thumb(0);

	stamp_update();
	return mystrdup(sn);
}

void tab_save(int num, char reloadButton)
{
	FILE *f;
	int fileSize, oldsave_as = save_as;
	char fileName[64];
	void *saveData;

	//build the tab
	save_as = 3;
	saveData = build_save(&fileSize, 0, 0, XRES, YRES, bmap, vx, vy, pv, fvx, fvy, signs, parts, 2);
	save_as = oldsave_as;
	if (!saveData)
		return;

#ifdef WIN32
	_mkdir("tabs");
#else
	mkdir("tabs", 0755);
#endif

	//save the tab
	sprintf(fileName, "tabs" PATH_SEP "%d.stm", num);
	f = fopen(fileName, "wb");
	if (!f)
		return;
	fwrite(saveData, fileSize, 1, f);
	fclose(f);

	if (!reloadButton)
		free(saveData);
	else
	{
		if (svf_last)
			free(svf_last);
		svf_last = saveData;
		svf_lsize = fileSize;
	}

	//set the tab's name
	if (strlen(svf_name))
		sprintf(tabNames[num-1], "%s", svf_name);
	else if (strlen(svf_filename))
		sprintf(tabNames[num-1], "%s", svf_filename);
	else
		sprintf(tabNames[num-1], "Untitled Simulation %i", num);
	
	//set the tab's thumbnail
	if (tabThumbnails[num-1])
		free(tabThumbnails[num-1]);
	//the fillrect is to cover up the quickoptions so they aren't in the thumbnail. the &filesize is just a dummy int variable that's unused
	fillrect(vid_buf, XRES, 0, BARSIZE, YRES/2, 0, 0, 0, 255);
	tabThumbnails[num-1] = rescale_img(vid_buf, XRES+BARSIZE, YRES, &fileSize, &fileSize, 3);
}

void *stamp_load(int i, int *size, int reorder)
{
	void *data;
	char fn[64];
	struct stamp tmp;

	if (!stamps[i].thumb || !stamps[i].name[0])
		return NULL;

	sprintf(fn, "stamps" PATH_SEP "%s.stm", stamps[i].name);
	data = file_load(fn, size);
	if (!data)
		return NULL;

	if (reorder && i>0)
	{
		memcpy(&tmp, stamps+i, sizeof(struct stamp));
		memmove(stamps+1, stamps, sizeof(struct stamp)*i);
		memcpy(stamps, &tmp, sizeof(struct stamp));

		stamp_update();
	}

	return data;
}

int tab_load(int tabNum)
{
	void *saveData;
	int saveSize;
	char fileName[64];

	sprintf(fileName, "tabs" PATH_SEP "%d.stm", tabNum);
	saveData = file_load(fileName, &saveSize);
	if (saveData)
	{
		parse_save(saveData, saveSize, 2, 0, 0, bmap, vx, vy, pv, fvx, fvy, signs, parts, pmap);
		ctrlzSnapshot();
		if (!svf_last) //only free if reload button isn't active
		{
			free(saveData);
			saveData = NULL;
		}
		return 1;
	}
	return 0;
}

void stamp_init(void)
{
	int i;
	FILE *f;

	memset(stamps, 0, sizeof(stamps));

	f=fopen("stamps" PATH_SEP "stamps.def", "rb");
	if (!f)
		return;
	for (i=0; i<STAMP_MAX; i++)
	{
		fread(stamps[i].name, 1, 10, f);
		if (!stamps[i].name[0])
			break;
		stamp_count++;
		stamp_gen_thumb(i);
	}
	fclose(f);
}

void del_stamp(int d)
{
	stamps[d].dodelete = 1;
	stamp_update();
	stamp_count = 0;
	stamp_init();
}

void thumb_cache_inval(char *id);

char *thumb_cache_id[THUMB_CACHE_SIZE];
void *thumb_cache_data[THUMB_CACHE_SIZE];
int thumb_cache_size[THUMB_CACHE_SIZE];
int thumb_cache_lru[THUMB_CACHE_SIZE];

void thumb_cache_inval(char *id)
{
	int i,j;
	for (i=0; i<THUMB_CACHE_SIZE; i++)
		if (thumb_cache_id[i] && !strcmp(id, thumb_cache_id[i]))
			break;
	if (i >= THUMB_CACHE_SIZE)
		return;
	free(thumb_cache_id[i]);
	free(thumb_cache_data[i]);
	thumb_cache_id[i] = NULL;
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		if (thumb_cache_lru[j] > thumb_cache_lru[i])
			thumb_cache_lru[j]--;
}
void thumb_cache_add(char *id, void *thumb, int size)
{
	int i,m=-1,j=-1;
	thumb_cache_inval(id);
	for (i=0; i<THUMB_CACHE_SIZE; i++)
	{
		if (!thumb_cache_id[i])
			break;
		if (thumb_cache_lru[i] > m)
		{
			m = thumb_cache_lru[i];
			j = i;
		}
	}
	if (i >= THUMB_CACHE_SIZE)
	{
		thumb_cache_inval(thumb_cache_id[j]);
		i = j;
	}
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		thumb_cache_lru[j] ++;
	thumb_cache_id[i] = mystrdup(id);
	thumb_cache_data[i] = malloc(size);
	memcpy(thumb_cache_data[i], thumb, size);
	thumb_cache_size[i] = size;
	thumb_cache_lru[i] = 0;
}
int thumb_cache_find(char *id, void **thumb, int *size)
{
	int i,j;
	for (i=0; i<THUMB_CACHE_SIZE; i++)
		if (thumb_cache_id[i] && !strcmp(id, thumb_cache_id[i]))
			break;
	if (i >= THUMB_CACHE_SIZE)
		return 0;
	for (j=0; j<THUMB_CACHE_SIZE; j++)
		if (thumb_cache_lru[j] < thumb_cache_lru[i])
			thumb_cache_lru[j]++;
	thumb_cache_lru[i] = 0;
	*thumb = malloc(thumb_cache_size[i]);
	*size = thumb_cache_size[i];
	memcpy(*thumb, thumb_cache_data[i], *size);
	return 1;
}

char http_proxy_string[256] = "";

unsigned char last_major=0, last_minor=0, last_build=0, update_flag=0;

char *tag = "(c) 2008-9 Stanislaw Skowronek";
int itc = 0;
char itc_msg[128] = "[?]";

#if defined WIN32
	#define UPDATE_ARCH "Windows32"
#elif defined LIN32
	#define UPDATE_ARCH "Linux32"
#elif defined LIN64
	#define UPDATE_ARCH "Linux64"
#elif defined MACOSX
	#define UPDATE_ARCH "MacOSX"
#else
	#define UPDATE_ARCH "Unknown"
#endif

#if defined X86_SSE3
	#define UPDATE_CPU "SSE3"
#elif defined X86_SSE2
	#define UPDATE_CPU "SSE2"
#elif defined X86_SSE
	#define UPDATE_CPU "SSE"
#else
	#define UPDATE_CPU "Unknown"
#endif

char update_uri[] = "http://" UPDATESERVER "/jacob1/update.lua?Action=Download&Architecture=" UPDATE_ARCH "&InstructionSet=" UPDATE_CPU;
char changelog_uri[] = "http://" UPDATESERVER "/jacob1/update.lua?Action=CheckVersion&Architecture=" UPDATE_ARCH "&InstructionSet=" UPDATE_CPU;
char update_uri_alt[] = "http://" UPDATESERVERALT "/jacob1/update.lua?Action=Download&Architecture=" UPDATE_ARCH "&InstructionSet=" UPDATE_CPU;
char changelog_uri_alt[] = "http://" UPDATESERVERALT "/jacob1/update.lua?Action=CheckVersion&Architecture=" UPDATE_ARCH "&InstructionSet=" UPDATE_CPU;

#ifdef RENDERER
int main(int argc, char *argv[])
{
	pixel *vid_buf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
	int load_size, i=0, j=0;
	void *load_data = file_load(argv[1], &load_size);
	unsigned char c[3];
	char ppmfilename[256], ptifilename[256], ptismallfilename[256];
	FILE *f;
	
	colour_mode = COLOUR_DEFAULT;
	init_display_modes();
	TRON_init_graphics();

	sys_pause = 1;
	parts = (particle*)calloc(sizeof(particle), NPART);
	for (i=0; i<NPART-1; i++)
		parts[i].life = i+1;
	parts[NPART-1].life = -1;
	globalSim->pfree = 0;

	pers_bg = (pixel*)calloc((XRES+BARSIZE)*YRES, PIXELSIZE);
	
	prepare_alpha(CELL, 1.0f);
	prepare_graphicscache();
	flm_data = generate_gradient(flm_data_colours, flm_data_pos, flm_data_points, 200);
	plasma_data = generate_gradient(plasma_data_colours, plasma_data_pos, plasma_data_points, 200);
	
	player.elem = player2.elem = PT_DUST;
	player.frames = player2.frames = 0;
	player.rocketBoots = 0;

	sprintf(ppmfilename, "%s.ppm", argv[2]);
	sprintf(ptifilename, "%s.pti", argv[2]);
	sprintf(ptismallfilename, "%s-small.pti", argv[2]);
	
	if(load_data && load_size){
		int parsestate = 0;
		//parsestate = parse_save(load_data, load_size, 1, 0, 0);
		parsestate = parse_save(load_data, load_size, 1, 0, 0, bmap, vx, vy, pv, fvx, fvy, signs, parts, pmap);
		
		decorations_enable = 0;
		for(i=0; i<30; i++){
			memset(vid_buf, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
			draw_walls(vid_buf);
			update_particles(vid_buf);
			render_parts(vid_buf);
			render_fire(vid_buf);
		}
		
		render_signs(vid_buf);
		
		if(parsestate>0){
			//return 0;
			info_box(vid_buf, "Save file invalid or from newer version");
		}

		//Save PTi images
		char * datares = NULL, *scaled_buf;
		int res = 0, sw, sh;
		datares = ptif_pack(vid_buf, XRES, YRES, &res);
		if(datares!=NULL){
			f=fopen(ptifilename, "wb");
			fwrite(datares, res, 1, f);
			fclose(f);
			free(datares);
			datares = NULL;
		}
		scaled_buf = resample_img(vid_buf, XRES, YRES, XRES/GRID_Z, YRES/GRID_Z);
		datares = ptif_pack(scaled_buf, XRES/GRID_Z, YRES/GRID_Z, &res);
		if(datares!=NULL){
			f=fopen(ptismallfilename, "wb");
			fwrite(datares, res, 1, f);
			fclose(f);
			free(datares);
			datares = NULL;
		}
		free(scaled_buf);
		//Save PPM image
		f=fopen(ppmfilename, "wb");
		fprintf(f,"P6\n%d %d\n255\n",XRES,YRES);
		for (j=0; j<YRES; j++)
		{
			for (i=0; i<XRES; i++)
			{
				c[0] = PIXR(vid_buf[i]);
				c[1] = PIXG(vid_buf[i]);
				c[2] = PIXB(vid_buf[i]);
				fwrite(c,3,1,f);
			}
			vid_buf+=XRES+BARSIZE;
		}
		fclose(f);
		
		return 1;
	}
	
	return 0;
}
#else

void BlueScreen(char * detailMessage)
{
	//std::string errorDetails = "Details: " + std::string(detailMessage);
	SDL_Event event;
	char * errorHelp = "An unrecoverable fault has occurred, please report this to jacob1:\n"
		" http://powdertoy.co.uk/Discussions/Thread/View.html?Thread=11117\n"
		" OR either my 'post bugs here' save / my 'Jacob1s Mod detector' save\n\n"
		"Note: TPT will now restart and reload your work";
	int positionX = (XRES+BARSIZE)/2-textwidth(errorHelp)/2-50, positionY = (YRES+MENUSIZE)/2-100;

	fillrect(vid_buf, -1, -1, XRES+BARSIZE, YRES+MENUSIZE, 17, 114, 169, 210);
	
	drawtext(vid_buf, positionX, positionY, "ERROR", 255, 255, 255, 255);
	drawtext(vid_buf, positionX, positionY + 14, detailMessage, 255, 255, 255, 255);
	drawtext(vid_buf, positionX, positionY  + 28, errorHelp, 255, 255, 255, 255);
	
	//Death loop
	while(1)
	{
		while (SDL_PollEvent(&event))
			if(event.type == SDL_QUIT)
			{
				char *exename;
				sys_pause = 1;
				tab_save(tab_num, 0);
				exename = exe_name();
				if (exename)
				{
#ifdef WIN32
					ShellExecute(NULL, "open", exename, NULL, NULL, SW_SHOWNORMAL);
#else
					execl(exename, "powder", NULL);
#endif
					free(exename);
				}
				exit(-1);
			}
		sdl_blit(0, 0, XRES+BARSIZE, YRES+MENUSIZE, vid_buf, XRES+BARSIZE);
	}
}

void SigHandler(int signal)
{
	switch(signal){
	case SIGSEGV:
		BlueScreen("Memory read/write error");
		break;
	case SIGFPE:
		BlueScreen("Floating point exception");
		break;
	case SIGILL:
		BlueScreen("Program execution exception");
		break;
	case SIGABRT:
		BlueScreen("Unexpected program abort");
		break;
	}
}

void ctrlzSnapshot()
{
	int cbx, cby, cbi;

	for (cbi=0; cbi<NPART; cbi++)
		cb_parts[cbi] = parts[cbi];

	for (cby = 0; cby<YRES; cby++)
		for (cbx = 0; cbx<XRES; cbx++)
			cb_pmap[cby][cbx] = pmap[cby][cbx];

	for (cby = 0; cby<(YRES/CELL); cby++)
		for (cbx = 0; cbx<(XRES/CELL); cbx++)
		{
			cb_vx[cby][cbx] = vx[cby][cbx];
			cb_vy[cby][cbx] = vy[cby][cbx];
			cb_pv[cby][cbx] = pv[cby][cbx];
			cb_hv[cby][cbx] = hv[cby][cbx];
			cb_bmap[cby][cbx] = bmap[cby][cbx];
			cb_emap[cby][cbx] = emap[cby][cbx];
		}
}
int main(int argc, char *argv[])
{
	pixel *part_vbuf; //Extra video buffer
	pixel *part_vbuf_store;
	void *http_ver_check, *http_session_check = NULL;
	char *ver_data=NULL, *check_data=NULL, *tmp, *changelog, *autorun_result = NULL, signal_hooks = 0;
	int i, j, bq, bc = 0, do_check=0, do_s_check=0, old_version=0, http_ret=0,http_s_ret=0, old_ver_len = 0, new_message_len=0, afk = 0, afkstart = 0;
	int x, y, line_x, line_y, b = 0, lb = 0, lx = 0, ly = 0, lm = 0;//, tx, ty;
	int da = 0, db = 0, it = 2047, mx, my;
	int load_mode=0, load_w=0, load_h=0, load_x=0, load_y=0, load_size=0;
	void *load_data=NULL;
	pixel *load_img=NULL;
	int save_mode=0, save_x=0, save_y=0, save_w=0, save_h=0, copy_mode=0;
	SDL_AudioSpec fmt;
	int username_flash = 0, username_flash_t = 1;
	int saveOpenError = 0;
	int benchmark_enable = 0;

	//init some new c++ stuff
	Simulation *mainSim = new Simulation();
	mainSim->InitElements();
	globalSim = mainSim;

#ifdef PTW32_STATIC_LIB
	pthread_win32_process_attach_np();
	pthread_win32_thread_attach_np();
#endif

	limitFPS = 60;
	vid_buf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE);
	part_vbuf = (pixel*)calloc((XRES+BARSIZE)*(YRES+MENUSIZE), PIXELSIZE); //Extra video buffer
	part_vbuf_store = part_vbuf;
	pers_bg = (pixel*)calloc((XRES+BARSIZE)*YRES, PIXELSIZE);

	for (i = 0; i < PT_NUM; i++)
		if (!ptypes[i].name)
		{
			ptypes[i].name = "";
			ptypes[i].descs = "";
		}

	for (int i = 0; i < 18; i++)
		favMenu[i] = globalSim->elements[i].Identifier;
	HudDefaults();
	memcpy(currentHud,normalHud,sizeof(currentHud));

	gravity_init();
	GSPEED = 1;

	/* Set 16-bit stereo audio at 22Khz */
	fmt.freq = 22050;
	fmt.format = AUDIO_S16;
	fmt.channels = 2;
	fmt.samples = 512;
	fmt.callback = mixaudio;
	fmt.userdata = NULL;

#ifdef MT
	numCores = core_count();
#endif
	InitMenusections();
	menu_count();
	activeTools[0] = GetToolFromIdentifier("DEFAULT_PT_DUST");
	activeTools[1] = GetToolFromIdentifier("DEFAULT_PT_NONE");
	activeTools[2] = GetToolFromIdentifier("DEFAULT_PT_NONE");
	decoTools[0] = GetToolFromIdentifier("DEFAULT_WL_26");
	decoTools[1] = GetToolFromIdentifier("DEFAULT_WL_27");
	decoTools[2] = GetToolFromIdentifier("DEFAULT_PT_NONE");
	currentBrush = new Brush(Point(5, 5), CIRCLE_BRUSH);

	cb_parts = (particle*)calloc(sizeof(particle), NPART);
	init_can_move();

#ifdef LUACONSOLE
	luacon_open();
#endif
	
	colour_mode = COLOUR_DEFAULT;
	init_display_modes();
	TRON_init_graphics();
	init_color_boxes();

	for (i=1; i<argc; i++)
	{
		if (!strncmp(argv[i], "ddir", 5) && i+1<argc)
		{
			chdir(argv[i+1]);
			i++;
		}
		else if (!strncmp(argv[i], "ptsave", 7) && i+1<argc)
		{
			//Prevent reading of any arguments after ptsave for security
			i++;
			argc = i+2;
			break;
		}
		else if (!strncmp(argv[i], "open", 5) && i+1<argc)
		{
			saveDataOpen = (char*)file_load(argv[i+1], &saveDataOpenSize);
			i++;
		}
	}
	
	load_presets();
	timesplayed = timesplayed + 1;
	if (DEBUG_MODE)
		memcpy(currentHud,debugHud,sizeof(currentHud));
	else
		memcpy(currentHud,normalHud,sizeof(currentHud));
	memset(parts, 0, sizeof(particle)*NPART);
	clear_sim();

	for (i=1; i<argc; i++)
	{
		if (!strncmp(argv[i], "scale:", 6))
		{
			sdl_scale = (argv[i][6]=='2') ? 2 : 1;
		}
		else if (!strncmp(argv[i], "proxy:", 6))
		{
			memset(http_proxy_string, 0, sizeof(http_proxy_string));
			strncpy(http_proxy_string, argv[i]+6, 255);
		}
		else if (!strncmp(argv[i], "nohud", 5))
		{
			hud_enable = 0;
		}
		else if (!strncmp(argv[i], "kiosk", 5))
		{
			kiosk_enable = 1;
			//sdl_scale = 2; //Removed because some displays cannot handle the resolution
			hud_enable = 0;
		}
		else if (!strncmp(argv[i], "sound", 5))
		{
			/* Open the audio device and start playing sound! */
			if ( SDL_OpenAudio(&fmt, NULL) < 0 )
			{
				fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
			}
			else
			{
				sound_enable = 1;
				SDL_PauseAudio(0);
			}
		}
		else if (!strncmp(argv[i], "scripts", 8))
		{
			file_script = 1;
		}
		else if (!strncmp(argv[i], "open", 5) && i+1<argc)
		{
			i++;
		}
		else if (!strncmp(argv[i], "ddir", 5) && i+1<argc)
		{
			i++;
		}
		else if (!strncmp(argv[i], "ptsave", 7) && i+1<argc)
		{
			int ci = 0, ns = 0, okay = 0, newprocess = 1;
			char * tempString = argv[i+1];
			int tempStringLength = strlen(argv[i+1])-7;
			int tempSaveID = 0;
			char tempNumberString[32];
			puts("Got ptsave");
			i++;
			tempNumberString[31] = 0;
			tempNumberString[0] = 0;
			if (!strncmp(tempString, "noopen:", 7))
				newprocess = 0;
			if ((!strncmp(tempString, "ptsave:", 7) && tempStringLength) || !newprocess)
			{
				puts("ptsave:// protocol");
				tempString+=7;
				while(tempString[ci] && ns<30 && ci<tempStringLength)
				{
					if(tempString[ci]>=48 && tempString[ci]<=57)
					{
						tempNumberString[ns++] = tempString[ci];
						tempNumberString[ns] = 0;
					}
					else if(tempString[ci]=='#')
					{
						okay = 1;
						break;
					}
					else
					{
						puts("ptsave: invalid save ID");
						break;
					}
					ci++;
				}
				if(!tempString[ci])
				{
					okay = 1;
				}
				if(okay)
				{
					tempSaveID = atoi(tempNumberString);
				}
			}
			if(tempSaveID > 0)
			{
#ifdef WIN32
				if (newprocess)
				{
					HWND modWindow = FindWindow(NULL, "Jacob1's Mod");
					if (modWindow)
					{
						//tell the already open window to open the save instead
						SendMessage(modWindow, WM_USER+614, (WPARAM)NULL, (LPARAM)tempSaveID);
						exit(0);
					}
				}
#endif
				puts("Got ptsave:id");
				saveURIOpen = tempSaveID;
				it = 0;
			}
			break;
		}
		else if (!strcmp(argv[i], "benchmark"))
		{
			benchmark_enable = 1;
			if (i+1<argc)
			{
				benchmark_file = argv[i+1];
				i++;
			}
		}
	}

	make_kernel();

	stamp_init();

	if (!sdl_open())
	{
		sdl_scale = 1;
		kiosk_enable = 0;
		if (!sdl_open()) exit(1);
	}
	save_presets(0);
	http_init(http_proxy_string[0] ? http_proxy_string : NULL);

	prepare_alpha(CELL, 1.0f);
	prepare_graphicscache();
	flm_data = generate_gradient(flm_data_colours, flm_data_pos, flm_data_points, 200);
	plasma_data = generate_gradient(plasma_data_colours, plasma_data_pos, plasma_data_points, 200);

	if (cpu_check())
	{
		error_ui(vid_buf, 0, "Unsupported CPU. Try another version.");
		return 1;
	}
	
	if(saveOpenError)
	{
		saveOpenError = 0;
		error_ui(vid_buf, 0, "Unable to open save file.");
	}

	if (strcmp(svf_user, "jacob1"))
	{
		if (doUpdates == 2)
			http_ver_check = http_async_req_start(NULL, changelog_uri_alt, NULL, 0, 0);
		else if (doUpdates)
			http_ver_check = http_async_req_start(NULL, changelog_uri, NULL, 0, 0);
		else
			http_ver_check = NULL;
	}
	else
		http_ver_check = NULL;
	if (svf_login) {
		if (http_ver_check)
		{
			http_auth_headers(http_ver_check, svf_user, NULL, NULL);
		}
		http_session_check = http_async_req_start(NULL, "http://" SERVER "/Login.api?Action=CheckSession", NULL, 0, 0);
		if (http_session_check)
			http_auth_headers(http_session_check, svf_user_id, NULL, svf_session_id);
	}
#ifdef LUACONSOLE
	if (file_exists("autorun.lua") && luacon_eval("dofile(\"autorun.lua\")", &autorun_result)) //Autorun lua script
	{
		luacon_log(mystrdup(luacon_geterror()));
	}
	if (autorun_result)
	{
		luacon_log(autorun_result);
	}
#endif
	for (i = 0; i < 10; i++)
	{
		sprintf(tabNames[i], "Untitled Simulation %i", i+1);
		tabThumbnails[i] = NULL;
	}
	if (tab_load(1))
	{
		char fn[64];
		for (i = 2; i <= 9; i++)
		{
			sprintf(fn, "tabs" PATH_SEP "%d.stm", i);
			if (file_exists(fn))
				num_tabs++;
			else
				break;
		}
	}

	if (benchmark_enable)
	{
		benchmark_run();
		exit(0);
	}

	while (!sdl_poll()) //the main loop
	{
		main_loop = 2;
		frameidx++;
		frameidx %= 30;
		if (!sys_pause||framerender) //only update air if not paused
		{
			update_air();
			if(aheat_enable)
				update_airh();
		}

		if(gravwl_timeout)
		{
			if(gravwl_timeout==1)
				gravity_mask();
			gravwl_timeout--;
		}
			
		//Pretty powders, colour cycle
		//sandcolour_r = 0;
		//sandcolour_g = 0;
		sandcolour_b = sandcolour_r = sandcolour_g = (int)(20.0f*sin((float)sandcolour_frame*(M_PI/180.0f)));
		sandcolour_frame++;
		sandcolour_frame%=360;

#ifdef OGLR
		part_vbuf = vid_buf;
#else
		if(ngrav_enable && (display_mode & DISPLAY_WARP))
		{
			part_vbuf = part_vbuf_store;
			memset(vid_buf, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
		} else {
			part_vbuf = vid_buf;
		}
#endif
		render_before(part_vbuf);
		
		if(debug_flags & (DEBUG_PERFORMANCE_CALC|DEBUG_PERFORMANCE_FRAME))
		{
			#ifdef WIN32
			#elif defined(MACOSX)
			#else
				struct timespec ts;
				clock_gettime(CLOCK_REALTIME, &ts);
				debug_perf_time = ts.tv_nsec;
			#endif
		}
		
		update_particles(part_vbuf); //update everything
			
		if(debug_flags & (DEBUG_PERFORMANCE_CALC|DEBUG_PERFORMANCE_FRAME))
		{
			#ifdef WIN32
			#elif defined(MACOSX)
			#else
				struct timespec ts;
				clock_gettime(CLOCK_REALTIME, &ts);
				
				debug_perf_partitime[debug_perf_iend]  = ts.tv_nsec - debug_perf_time;
				
				debug_perf_time = ts.tv_nsec;
			#endif
		}
		
		render_after(part_vbuf, vid_buf);
		
		if(debug_flags & (DEBUG_PERFORMANCE_CALC|DEBUG_PERFORMANCE_FRAME))
		{
			#ifdef WIN32
			#elif defined(MACOSX)
			#else
				struct timespec ts;
				clock_gettime(CLOCK_REALTIME, &ts);
				
				debug_perf_frametime[debug_perf_iend]  = ts.tv_nsec - debug_perf_time;
			#endif
			debug_perf_iend++;
			debug_perf_iend %= DEBUG_PERF_FRAMECOUNT;
			debug_perf_istart++;
			debug_perf_istart %= DEBUG_PERF_FRAMECOUNT;
		}
		
		gravity_update_async(); //Check for updated velocity maps from gravity thread
		if (!sys_pause||framerender) //Only update if not paused
			memset(gravmap, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float)); //Clear the old gravmap

		if (framerender) {
			framerender = 0;
			sys_pause = 1;
		}

		memset(vid_buf+((XRES+BARSIZE)*YRES), 0, (PIXELSIZE*(XRES+BARSIZE))*MENUSIZE);//clear menu areas
		clearrect(vid_buf, XRES-1, 0, BARSIZE+1, YRES);

		draw_svf_ui(vid_buf, sdl_mod & (KMOD_LCTRL|KMOD_RCTRL));
		
		if(debug_flags)
		{
			draw_debug_info(vid_buf, lm, lx, ly, x, y, line_x, line_y);
		}

		if (http_ver_check)
		{
			if (!do_check && http_async_req_status(http_ver_check))
			{
				int len;
				ver_data = http_async_req_stop(http_ver_check, &http_ret, &len);
				if (http_ret == 200 && ver_data)
				{
					int count, buildnum, major, minor;
					if (sscanf(ver_data, "%d %d %d%n", &buildnum, &major, &minor, &count) == 3)
						if (buildnum>MOD_BUILD_VERSION)
						{
							old_version = 1;
							changelog = (char*)malloc((len-count)*sizeof(char)+64);
							sprintf(changelog, "\bbYour version: %d.%d (%d)\nNew version: %d.%d (%d)\n\n\bwChangeLog:\n%s", MOD_VERSION, MOD_MINOR_VERSION, MOD_BUILD_VERSION, major, minor, buildnum, &ver_data[count+2]);
						}
					old_ver_len = textwidth((char*)old_ver_msg);
					free(ver_data);
				}
				else
				{
					strcpy(itc_msg, "Error, could not find update server. Press Ctrl+u to go check for a newer version manually on the tpt website");
					itc = 500;
					it = 0;
				}
				http_ver_check = NULL;
			}
			do_check = (do_check+1) & 15;
		}
		if (saveDataOpen)
		{
			//Clear all settings and simulation data
			NewSim();
			it=0;
			
			svf_last = saveDataOpen;
			svf_lsize = saveDataOpenSize;
			if(parse_save(saveDataOpen, saveDataOpenSize, 1, 0, 0, bmap, fvx, fvy, vx, vy, pv, signs, parts, pmap))
			{
				saveOpenError = 1;
				svf_last = NULL;
				svf_lsize = 0;
				free(saveDataOpen);
			}
			saveDataOpenSize = 0;
			saveDataOpen = NULL;
		}
		if (http_session_check)
		{
			if (!do_s_check && http_async_req_status(http_session_check))
			{
				check_data = http_async_req_stop(http_session_check, &http_s_ret, NULL);
				if (http_s_ret==200 && check_data)
				{
					if (!strncmp(check_data, "EXPIRED", 7))
					{
						//Session expired
						printf("EXPIRED");
						strcpy(svf_user, "");
						strcpy(svf_pass, "");
						strcpy(svf_user_id, "");
						strcpy(svf_session_id, "");
						svf_login = 0;
						svf_own = 0;
						svf_admin = 0;
						svf_mod = 0;
						svf_messages = 0;
					}
					else if (!strncmp(check_data, "BANNED", 6))
					{
						//User banned
						printf("BANNED");
						strcpy(svf_user, "");
						strcpy(svf_pass, "");
						strcpy(svf_user_id, "");
						strcpy(svf_session_id, "");
						svf_login = 0;
						svf_own = 0;
						svf_admin = 0;
						svf_mod = 0;
						svf_messages = 0;
						error_ui(vid_buf, 0, "Unable to log in\nYour account has been suspended, consider reading the rules.");
					}
					else if (!strncmp(check_data, "OK", 2))
					{
						//Session valid
						if (strlen(check_data)>2) {
							//User is elevated
							if (!strncmp(check_data+3, "ADMIN", 5))
							{
								//Check for messages
								svf_messages = atoi(check_data+9);
								svf_admin = 1;
								svf_mod = 0;
							}
							else if (!strncmp(check_data+3, "MOD", 3))
							{
								//Check for messages
								svf_messages = atoi(check_data+7);
								svf_admin = 0;
								svf_mod = 1;
							} else {
								//Check for messages
								svf_messages = atoi(check_data+3);
							}
						}
					}
					else
					{
						//No idea, but log the user out anyway
						strcpy(svf_user, "");
						strcpy(svf_pass, "");
						strcpy(svf_user_id, "");
						strcpy(svf_session_id, "");
						svf_login = 0;
						svf_own = 0;
						svf_admin = 0;
						svf_mod = 0;
						svf_messages = 0;
					}
					save_presets(0);
					free(check_data);
				} else {
					//Unable to check session, <del>YOU WILL BE TERMINATED</del>
					/*strcpy(svf_user, "");
					strcpy(svf_pass, "");
					strcpy(svf_user_id, "");
					strcpy(svf_session_id, "");
					svf_login = 0;
					svf_own = 0;
					svf_admin = 0;
					svf_mod = 0;
					svf_messages = 0;*/
				}
				http_session_check = NULL;
			} else {
				clearrect(vid_buf, XRES-125+BARSIZE/*385*/, YRES+(MENUSIZE-16), 91, 14);
				drawrect(vid_buf, XRES-125+BARSIZE/*385*/, YRES+(MENUSIZE-16), 91, 14, 255, 255, 255, 255);
				drawtext(vid_buf, XRES-122+BARSIZE/*388*/, YRES+(MENUSIZE-13), "\x84", 255, 255, 255, 255);
				if (username_flash>30) {
					username_flash_t = -1;
					username_flash = 30;
				} else if (username_flash<0) {
					username_flash_t = 1;
					username_flash = 0;
				}
				username_flash += username_flash_t;
				if (svf_login)
					drawtext(vid_buf, XRES-104+BARSIZE/*406*/, YRES+(MENUSIZE-12), svf_user, 255, 255, 255, 175-(username_flash*5));
				else
					drawtext(vid_buf, XRES-104+BARSIZE/*406*/, YRES+(MENUSIZE-12), "[checking]", 255, 255, 255, 255);
			}
			do_s_check = (do_s_check+1) & 15;
		}
		if(saveURIOpen)
		{
			char saveURIOpenString[512];
			sprintf(saveURIOpenString, "%d", saveURIOpen);
			open_ui(vid_buf, saveURIOpenString, NULL, 0);
			saveURIOpen = 0;
		}
		if (ptsaveOpenID)
		{
			if (num_tabs < 24-GetNumMenus())
			{
				char ptsaveOpenString[512];
				int oldTabNum = tab_num;
				tab_save(tab_num, 0);
				num_tabs++;
				tab_num = num_tabs;
				NewSim();
				sprintf(ptsaveOpenString, "%d", ptsaveOpenID);

				//if they didn't open it, don't make the new tab
				if (!open_ui(vid_buf, ptsaveOpenString, NULL, 0))
				{
					num_tabs--;
					tab_num = oldTabNum;
					tab_load(oldTabNum);
				}
				tab_save(tab_num, 1);
			}
			ptsaveOpenID = 0;
		}
#ifdef LUACONSOLE
		if(sdl_key){
			if(!luacon_keyevent(sdl_key, sdl_mod, LUACON_KDOWN))
				sdl_key = 0;
		}
		if(sdl_rkey){
			if(!luacon_keyevent(sdl_rkey, sdl_mod, LUACON_KUP))
				sdl_rkey = 0;
		}
#endif
		if (!deco_disablestuff && sys_shortcuts==1)//all shortcuts can be disabled by python scripts
		{
			stickmen_keys();
			if (sdl_key=='q' || sdl_key==SDLK_ESCAPE)
			{
				if (confirm_ui(vid_buf, "You are about to quit", "Are you sure you want to quit?", "Quit"))
				{
					break;
				}
			}
			if (sdl_key=='i' && (sdl_mod & KMOD_CTRL))
			{
				if(confirm_ui(vid_buf, "Install Powder Toy", "You are about to install The Powder Toy", "Install"))
				{
					if(register_extension())
					{
						info_ui(vid_buf, "Install success", "Powder Toy has been installed!");
					}
					else
					{
						error_ui(vid_buf, 0, "Install failed - You may not have permission or you may be on a platform that does not support installation");
					}
				}
			}
			if (sdl_key=='f')
			{
				if ((sdl_mod & (KMOD_LCTRL)))
				{
					if (!(finding & 0x1))
						finding |= 0x1;
					else
						finding &= ~0x1;
				}
				else if ((sdl_mod & (KMOD_RCTRL)))
				{
					if (!(finding & 0x2))
						finding |= 0x2;
					else
						finding &= ~0x2;
				}
				else if ((sdl_mod & (KMOD_SHIFT)))
				{
					if (!(finding & 0x4))
						finding |= 0x4;
					else
						finding &= ~0x4;
				}
				else
					framerender = 1;
			}
			if ((sdl_key=='l' || sdl_key=='k') && stamps[0].name[0])
			{
				if (load_mode)
				{
					free(load_img);
					free(load_data);
					load_mode = 0;
					load_data = NULL;
					load_img = NULL;
				}
				if (it > 50)
					it = 50;
				if (sdl_key=='k' && stamps[1].name[0])
				{
					int reorder = 1;
					j = stamp_ui(vid_buf, &reorder);
					if (j>=0)
						load_data = stamp_load(j, &load_size, reorder);
					else
						load_data = NULL;
				}
				else
					load_data = stamp_load(0, &load_size, 1);
				if (load_data)
				{
					load_img = prerender_save(load_data, load_size, &load_w, &load_h);
					if (load_img)
						load_mode = 1;
					else
					{
						free(load_data);
						load_data = NULL;
					}
				}
			}
			if (sdl_key=='s')
			{
				//if stkm2 is out, you must be holding right ctrl, else just either ctrl
				if ((player2.spwn && (sdl_mod&KMOD_RCTRL)) || (!player2.spwn && (sdl_mod&KMOD_CTRL)))
					tab_save(tab_num, 1);
				//if stkm2 is out, you must be holding left ctrl, else not be holding ctrl at all
				else if ((player2.spwn && (sdl_mod&KMOD_LCTRL)) || (!player2.spwn && !(sdl_mod&KMOD_CTRL)))
				{
					if (it > 50)
						it = 50;
					save_mode = 1;
				}
			}
			if (sdl_key=='r') 
			{
				if ((sdl_mod & (KMOD_CTRL)) && load_mode != 1)
				{
					if (strncmp(svf_id,"",8))
						parse_save(svf_last, svf_lsize, 1, 0, 0, bmap, vx, vy, pv, fvx, fvy, signs, parts, pmap);
				}
				else if (!(sdl_mod & (KMOD_CTRL|KMOD_SHIFT)))
					GENERATION = 0;
			}
			if (sdl_key=='o')
			{
				if  (sdl_mod & (KMOD_CTRL))
				{
					catalogue_ui(vid_buf);
				}
				else
					old_menu = !old_menu;
			}
			if(sdl_key=='e')
			{
				element_search_ui(vid_buf, &activeTools[0], &activeTools[1]);
			}
			if (sdl_key=='1')
			{
				if (sdl_mod & (KMOD_CTRL))
					display_mode = display_mode&DISPLAY_AIRV ? display_mode&!DISPLAY_AIRV:display_mode|DISPLAY_AIRV;
				else
					set_cmode(CM_VEL);
			}
			if (sdl_key=='2')
			{
				if (sdl_mod & (KMOD_CTRL))
					display_mode = display_mode&DISPLAY_AIRP ? display_mode&!DISPLAY_AIRP:display_mode|DISPLAY_AIRP;
				else
					set_cmode(CM_PRESS);
			}
			if (sdl_key=='3')
			{
				if (sdl_mod & (KMOD_CTRL))
				{
					display_mode = display_mode&DISPLAY_PERS ? display_mode&!DISPLAY_PERS:display_mode|DISPLAY_PERS;
					memset(pers_bg, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
				}
				else
					set_cmode(CM_PERS);
			}
			if (sdl_key=='4')
			{
				if (sdl_mod & (KMOD_CTRL))
					render_mode = render_mode&FIREMODE ? render_mode&~FIREMODE:render_mode|FIREMODE;
				else
					set_cmode(CM_FIRE);
			}
			if (sdl_key=='5')
			{
				if (sdl_mod & (KMOD_CTRL))
					render_mode = render_mode&PMODE_BLOB ? render_mode&~PMODE_BLOB:render_mode|PMODE_BLOB;
				else
					set_cmode(CM_BLOB);
			}
			if (sdl_key=='6')
			{
				if (sdl_mod & (KMOD_CTRL))
					colour_mode = colour_mode == COLOUR_HEAT ? 0:COLOUR_HEAT;
				else
					set_cmode(CM_HEAT);
			}
			if (sdl_key=='7')
			{
				if (sdl_mod & (KMOD_CTRL))
					display_mode = display_mode&DISPLAY_WARP ? display_mode&!DISPLAY_WARP:display_mode|DISPLAY_WARP;
				else
					set_cmode(CM_FANCY);
			}
			if (sdl_key=='8')
			{
				set_cmode(CM_NOTHING);
			}
			if (sdl_key=='9')
			{
				if (sdl_mod & (KMOD_CTRL))
					colour_mode = colour_mode == COLOUR_GRAD ? 0:COLOUR_GRAD;
				else
					set_cmode(CM_GRAD);
			}
			if (sdl_key=='0')
			{
				if (sdl_mod & (KMOD_CTRL))
					display_mode = display_mode&DISPLAY_AIRC ? display_mode&!DISPLAY_AIRC:display_mode|DISPLAY_AIRC;
				else
				set_cmode(CM_CRACK);
			}
			if (sdl_key=='1'&& (sdl_mod & (KMOD_SHIFT)) && DEBUG_MODE)
			{
				if (sdl_mod & (KMOD_CTRL))
					colour_mode = colour_mode == COLOUR_LIFE ? 0:COLOUR_LIFE;
				else
					set_cmode(CM_LIFE);
			}
			if (active_menu == SC_FAV2 && (sdl_mod & KMOD_RCTRL) && (sdl_mod & KMOD_RSHIFT))
			{
				active_menu = SC_CRACKER;
			}
			if (sdl_key==SDLK_TAB)
			{
				currentBrush->SetShape((currentBrush->GetShape()+1)%BRUSH_NUM);
			}
			if (sdl_key==SDLK_LEFTBRACKET) {
				if (sdl_zoom_trig)
				{
					ZSIZE -= 1;
					if (ZSIZE>60)
						ZSIZE = 60;
					if (ZSIZE<2)
						ZSIZE = 2;
					ZFACTOR = 256/ZSIZE;
				}
				else
				{
					if (sdl_mod & (KMOD_LALT|KMOD_RALT) && !(sdl_mod & (KMOD_SHIFT|KMOD_CTRL)))
					{
						currentBrush->ChangeRadius(Point(-1, -1));
					}
					else if (sdl_mod & (KMOD_SHIFT) && !(sdl_mod & (KMOD_CTRL)))
					{
						currentBrush->ChangeRadius(Point(-1, 0));
					}
					else if (sdl_mod & (KMOD_CTRL) && !(sdl_mod & (KMOD_SHIFT)))
					{
						currentBrush->ChangeRadius(Point(0, -1));
					}
					else
					{
						currentBrush->ChangeRadius(Point(-(int)ceil((currentBrush->GetRadius().X/5)+0.5f), -(int)ceil((currentBrush->GetRadius().Y/5)+0.5f)));
					}
				}
			}
			if (sdl_key==SDLK_RIGHTBRACKET) {
				if (sdl_zoom_trig)
				{
					ZSIZE += 1;
					if (ZSIZE>60)
						ZSIZE = 60;
					if (ZSIZE<2)
						ZSIZE = 2;
					ZFACTOR = 256/ZSIZE;
				}
				else
				{
					if (sdl_mod & (KMOD_LALT|KMOD_RALT) && !(sdl_mod & (KMOD_SHIFT|KMOD_CTRL)))
					{
						currentBrush->ChangeRadius(Point(1, 1));
					}
					else if (sdl_mod & (KMOD_SHIFT) && !(sdl_mod & (KMOD_CTRL)))
					{
						currentBrush->ChangeRadius(Point(1, 0));
					}
					else if (sdl_mod & (KMOD_CTRL) && !(sdl_mod & (KMOD_SHIFT)))
					{
						currentBrush->ChangeRadius(Point(0, 1));
					}
					else
					{
						currentBrush->ChangeRadius(Point((int)ceil((currentBrush->GetRadius().X/5)+0.5f), (int)ceil((currentBrush->GetRadius().Y/5)+0.5f)));
					}
				}
			}
			if (sdl_key=='d' && ((sdl_mod & (KMOD_CTRL)) || !player2.spwn))
			{
				DEBUG_MODE = !DEBUG_MODE;
				SetCurrentHud();
			}
			if (sdl_key=='i')
			{
				int nx, ny;
				for (nx = 0; nx<XRES/CELL; nx++)
					for (ny = 0; ny<YRES/CELL; ny++)
					{
						pv[ny][nx] = -pv[ny][nx];
						vx[ny][nx] = -vx[ny][nx];
						vy[ny][nx] = -vy[ny][nx];
					}
			}
			if (sdl_key==SDLK_INSERT)
				REPLACE_MODE = !REPLACE_MODE;
			else if (sdl_key == SDLK_DELETE)
				SPECIFIC_DELETE = !SPECIFIC_DELETE;
			else if (sdl_key == SDLK_SEMICOLON)
			{
				if (sdl_mod&KMOD_CTRL)
					SPECIFIC_DELETE = !SPECIFIC_DELETE;
				else
					REPLACE_MODE = !REPLACE_MODE;
			}
			if (sdl_key==SDLK_BACKQUOTE)
			{
				console_mode = !console_mode;
				//hud_enable = !console_mode;
			}
			if (sdl_key=='b')
			{
				if (sdl_mod & KMOD_CTRL)
				{
					decorations_enable = !decorations_enable;
					itc = 51;
					if (decorations_enable) strcpy(itc_msg, "Decorations layer: On");
					else strcpy(itc_msg, "Decorations layer: Off");
				}
				else if (active_menu == SC_DECO)
				{
					active_menu = last_active_menu;
					*decoTools = *activeTools;
					*activeTools = *regularTools;
				}
				else
				{
					last_active_menu = active_menu;
					decorations_enable = 1;
					sys_pause = 1;
					active_menu = SC_DECO;

					*regularTools = *activeTools;
					*activeTools = *decoTools;
					activeTools[0] = GetToolFromIdentifier("DEFAULT_WL_26");
				}
			}
			if (sdl_key=='g')
			{
				if(sdl_mod & (KMOD_CTRL))
				{
					drawgrav_enable =! drawgrav_enable;
				}
				else
				{
					if (sdl_mod & (KMOD_SHIFT))
						GRID_MODE = (GRID_MODE+9)%10;
					else
						GRID_MODE = (GRID_MODE+1)%10;
				}
			}
			if (sdl_key=='=')
			{
				int nx, ny;
				if (sdl_mod & (KMOD_CTRL))
				{
					for (i=0; i < NPART; i++)
						if (parts[i].type == PT_SPRK)
						{
							if (parts[i].ctype >= 0 && parts[i].ctype < PT_NUM && ptypes[parts[i].ctype].enabled)
							{
								parts[i].type = parts[i].ctype;
								parts[i].life = parts[i].ctype = 0;
							}
							else
								kill_part(i);
						}
				}
				else
				{
					for (nx = 0; nx < XRES/CELL; nx++)
						for (ny = 0; ny < YRES/CELL; ny++)
						{
							pv[ny][nx] = 0;
							vx[ny][nx] = 0;
							vy[ny][nx] = 0;
						}
					for (i=0; i < NPART; i++)
						if (parts[i].type == PT_QRTZ || parts[i].type == PT_GLAS || parts[i].type == PT_TUNG)
						{
							parts[i].pavg[0] = parts[i].pavg[1] = 0;
						}
				}
			}

			if (sdl_key=='w' && (!player2.spwn || (sdl_mod & (KMOD_CTRL)))) //Gravity, by Moach
			{
				++gravityMode; // cycle gravity mode
				itc = 51;

				switch (gravityMode)
				{
				default:
					gravityMode = 0;
				case 0:
					strcpy(itc_msg, "Gravity: Vertical");
					break;
				case 1:
					strcpy(itc_msg, "Gravity: Off");
					break;
				case 2:
					strcpy(itc_msg, "Gravity: Radial");
					break;

				}
			}
			if (sdl_key=='y')
			{
				++airMode;
				itc = 52;

				switch (airMode)
				{
				default:
					airMode = 0;
				case 0:
					strcpy(itc_msg, "Air: On");
					break;
				case 1:
					strcpy(itc_msg, "Air: Pressure Off");
					break;
				case 2:
					strcpy(itc_msg, "Air: Velocity Off");
					break;
				case 3:
					strcpy(itc_msg, "Air: Off");
					break;
				case 4:
					strcpy(itc_msg, "Air: No Update");
					break;
				}
			}

			if (sdl_key=='t')
				show_tabs = !show_tabs;
			if (sdl_key==SDLK_SPACE)
				sys_pause = !sys_pause;
			if (sdl_key=='u')
			{
				if (sdl_mod & KMOD_CTRL)
					open_link("http://powdertoy.co.uk/Discussions/Thread/View.html?Thread=11117");
				else
					aheat_enable = !aheat_enable;

			}
			if (sdl_key=='h' && !(sdl_mod & KMOD_LCTRL))
			{
				hud_enable = !hud_enable;
			}
			if (sdl_key==SDLK_F1 || (sdl_key=='h' && (sdl_mod & KMOD_LCTRL)))
			{
				if(!it)
				{
					it = 8047;
				}
				else
				{
					it = 0;
				}
			}
			if (sdl_key=='n')
			{
				if (sdl_mod & KMOD_CTRL)
				{
					if (num_tabs < 24-GetNumMenus())
					{
						tab_save(tab_num, 0);
						num_tabs++;
						tab_num = num_tabs;
						NewSim();
						tab_save(tab_num, 1);
					}
				}
				else
				{
					if(ngrav_enable)
						stop_grav_async();
					else
						start_grav_async();
				}
			}
			if (sdl_key=='p')
				dump_frame(vid_buf, XRES, YRES, XRES+BARSIZE);
			if (sdl_key=='v'&&(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))
			{
				if (clipboard_ready==1 && clipboard_data)
				{
					if (load_data)
						free(load_data);
					load_data = malloc(clipboard_length);
					if (load_data)
					{
						memcpy(load_data, clipboard_data, clipboard_length);
						load_size = clipboard_length;
						load_img = prerender_save(load_data, load_size, &load_w, &load_h);
						if (load_img)
							load_mode = 1;
						else
						{
							free(load_data);
							load_data = NULL;
						}
					}
				}
			}
			if (load_mode==1)
			{
				matrix2d transform = m2d_identity;
				vector2d translate = v2d_zero;
				void *ndata;
				int doTransform = 0;
				if (sdl_key=='r'&&(sdl_mod & (KMOD_CTRL))&&(sdl_mod & (KMOD_SHIFT)))
				{
					transform = m2d_new(-1,0,0,1); //horizontal invert
					doTransform = 1;
				}
				else if (sdl_key=='r'&&(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))
				{
					transform = m2d_new(0,1,-1,0); //rotate anticlockwise 90 degrees
					doTransform = 1;
				}
				else if (sdl_mod & (KMOD_CTRL))
				{
					doTransform = 1;
					if (sdl_key==SDLK_LEFT) translate = v2d_new(-1,0);
					else if (sdl_key==SDLK_RIGHT) translate = v2d_new(1,0);
					else if (sdl_key==SDLK_UP) translate = v2d_new(0,-1);
					else if (sdl_key==SDLK_DOWN) translate = v2d_new(0,1);
					else doTransform = 0;
				}
				if (doTransform)
				{
					ndata = transform_save(load_data, &load_size, transform, translate);
					if (ndata!=load_data) free(load_data);
					free(load_img);
					load_data = ndata;
					load_img = prerender_save(load_data, load_size, &load_w, &load_h);
				}
			}
			if (sdl_key=='x'&&(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))
			{
				save_mode = 1;
				copy_mode = 2;
			}
			if (sdl_key=='c'&&(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))
			{
				save_mode = 1;
				copy_mode = 1;
			}
			//TODO: Superseded by new display mode switching, need some keyboard shortcuts
			/*else if (sdl_key=='c')
			{
				set_cmode((cmode+1) % CM_COUNT);
				if (it > 50)
					it = 50;
			}*/
			if (sdl_key=='z') // Undo
			{
				if (sdl_mod & (KMOD_LCTRL|KMOD_RCTRL))
				{
					int cbx, cby, cbi;

					for (cbi=0; cbi<NPART; cbi++)
						parts[cbi] = cb_parts[cbi];
					parts_lastActiveIndex = NPART-1;

					for (cby = 0; cby<YRES; cby++)
						for (cbx = 0; cbx<XRES; cbx++)
							pmap[cby][cbx] = cb_pmap[cby][cbx];

					for (cby = 0; cby<(YRES/CELL); cby++)
						for (cbx = 0; cbx<(XRES/CELL); cbx++)
						{
							vx[cby][cbx] = cb_vx[cby][cbx];
							vy[cby][cbx] = cb_vy[cby][cbx];
							pv[cby][cbx] = cb_pv[cby][cbx];
							hv[cby][cbx] = cb_hv[cby][cbx];
							bmap[cby][cbx] = cb_bmap[cby][cbx];
							emap[cby][cbx] = cb_emap[cby][cbx];
						}

					force_stacking_check = 1;//check for excessive stacking of particles next time update_particles is run
				}
				else
				{
					if (sdl_mod & KMOD_ALT)//toggle
						sdl_zoom_trig = (!sdl_zoom_trig)*2;
					else
						sdl_zoom_trig = 1;
				}
			}
			if (sdl_rkey == 'z' && sdl_zoom_trig==1)//if ==2 then it was toggled with alt+z, don't turn off on keyup
				sdl_zoom_trig = 0;
		}
#ifdef INTERNAL
		int counterthing;
		if (sdl_key=='v'&&!(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)))//frame capture
		{
			if (sdl_mod & (KMOD_SHIFT)) {
				if (vs>=1)
					vs = 0;
				else
					vs = 3;//every other frame
			}
			else
			{
				if (vs>=1)
					vs = 0;
				else
					vs = 1;
			}
			counterthing = 0;
		}
		if (vs)
		{
			if (counterthing+1>=vs)
			{
				dump_frame(vid_buf, XRES, YRES, XRES+BARSIZE);
				counterthing = 0;
			}
			counterthing = (counterthing+1)%3;
		}
#endif

		if (sdl_wheel)
		{
			if (sdl_zoom_trig)//zoom window change
			{
				ZSIZE += sdl_wheel;
				if (ZSIZE>60)
					ZSIZE = 60;
				if (ZSIZE<2)
					ZSIZE = 2;
				ZFACTOR = 256/ZSIZE;
				//sdl_wheel = 0;
			}
			else //change brush size
			{
				if (!(sdl_mod & (KMOD_SHIFT|KMOD_CTRL)))
				{
					currentBrush->ChangeRadius(Point(sdl_wheel, sdl_wheel));
				}
				else if (sdl_mod & (KMOD_SHIFT) && !(sdl_mod & (KMOD_CTRL)))
				{
					currentBrush->ChangeRadius(Point(sdl_wheel, 0));
				}
				else if (sdl_mod & (KMOD_CTRL) && !(sdl_mod & (KMOD_SHIFT)))
				{
					currentBrush->ChangeRadius(Point(0, sdl_wheel));
				}
			}
		}

		bq = bc; // bq is previous mouse state
		bc = b = mouse_get_state(&x, &y); // b is current mouse state

#ifdef LUACONSOLE
		if(bc && bq){
			if(!luacon_mouseevent(x, y, bc, LUACON_MPRESS, sdl_wheel)){
				b = 0;
			}
		}
		else if(bc && !bq){
			if(!luacon_mouseevent(x, y, bc, LUACON_MDOWN, sdl_wheel)){
				b = 0;
			}
		}
		else if(!bc && bq){
			if(!luacon_mouseevent(x, y, bq, LUACON_MUP, sdl_wheel)){
				b = 0;
			}
		}
		else if (sdl_wheel){
			luacon_mouseevent(x, y, bq, 0, sdl_wheel);
		}
		
		luacon_step(x, y, activeTools[0]->GetIdentifier(), activeTools[1]->GetIdentifier(), currentBrush->GetRadius().X, currentBrush->GetRadius().Y);
		readluastuff();
#endif
		sdl_wheel = 0;

		if (!old_menu)
		{
			quickoptions_menu(vid_buf, b, bq, x, y);

			int hover = DrawMenus(vid_buf, active_menu, y);

			if (hover >= 0 && ((hover != SC_DECO && !b) || (hover == SC_DECO && b && !bq)) && x>=XRES-2 && x<XRES+BARSIZE-1)
			{
				if (hover == SC_DECO && active_menu != SC_DECO)
					last_active_menu = active_menu;
				if (hover == SC_FAV)
					active_menu = last_fav_menu;
				else
					active_menu = hover;
			}
			menu_ui_v3(vid_buf, active_menu, b, bq, x, y); //draw the elements in the current menu
		}
		else
		{
			old_menu_v2(active_menu, x, y, b, bq);
		}
		//TODO: only when entering / exiting menu
		if (active_menu == SC_DECO && activeTools[0]->GetType() != DECO_TOOL)
		{
			*regularTools = *activeTools;
			*activeTools = *decoTools;

			activeTools[0] = GetToolFromIdentifier("DEFAULT_WL_26");
		}
		else if (active_menu != SC_DECO && activeTools[0]->GetType() == DECO_TOOL)
		{
			*decoTools = *activeTools;
			*activeTools = *regularTools;

			deco_disablestuff = 0;
		}
		if (deco_disablestuff)
			b = 0;

		mouse_coords_window_to_sim(&x, &y, x, y);//change mouse position while it is in a zoom window
		mx = x;
		my = y;
		if (b && !bq && x>=(XRES-19-new_message_len) &&
		        x<=(XRES-14) && y>=(YRES-37) && y<=(YRES-24) && svf_messages)
		{
			if (b == 1)
				open_link("http://" SERVER "/Conversations.html");
			svf_messages = 0;
			b = 0;
		}
		if (update_flag)
		{
			info_box(vid_buf, "Finalizing update...");
			if (last_build>BUILD_NUM)
			{
				update_cleanup();
				error_ui(vid_buf, 0, "Update failed - try downloading a new version.");
			}
			else
			{
				if (update_finish())
					error_ui(vid_buf, 0, "Update failed - try downloading a new version.");
				else
					info_ui(vid_buf, "Update success", "You have successfully updated Jacob1's Mod!");
			}
			update_flag = 0;
		}

		if (b && !bq && x>=(XRES-19-old_ver_len) &&
		        x<=(XRES-14) && y>=(YRES-22) && y<=(YRES-9) && old_version)
		{
			if (b == 1 && confirm_ui(vid_buf, "\bwDo you want to update Jacob1's Mod?", changelog, "\btUpdate"))
			{
				free(changelog);
				if (doUpdates == 2)
					tmp = download_ui(vid_buf, update_uri_alt, &i);
				else
					tmp = download_ui(vid_buf, update_uri, &i);

				if (tmp)
				{
					save_presets(1);
					if (update_start(tmp, i))
					{
						update_cleanup();
						save_presets(0);
						error_ui(vid_buf, 0, "Update failed - try downloading a new version.");
					}
					else
						return 0;
				}
			}
			old_version = 0;
			b = 0;
		}
		if (y > YRES+MENUSIZE-BARSIZE) //mouse checks for buttons at the bottom, to draw mouseover texts
		{
			if (x>=189 && x<=202 && svf_login && svf_open && svf_myvote==0)
			{
				db = svf_own ? 275 : 272;
				if (da < 51)
					da ++;
			}
			else if (x>=204 && x<=217 && svf_login && svf_open && svf_myvote==0)
			{
				db = svf_own ? 275 : 272;
				if (da < 51)
					da ++;
			}
			else if (x>=189 && x<=217 && svf_login && svf_open && svf_myvote!=0)
			{
				db = (svf_myvote==1) ? 273 : 274;
				if (da < 51)
					da ++;
			}
			else if (x>=219 && x<=((XRES+BARSIZE-(510-349))) && svf_open)
			{
				db = svf_own ? 257 : 256;
				if (da < 51)
					da ++;
			}
			else if (x>=((XRES+BARSIZE-(510-351))) && x<((XRES+BARSIZE-(510-366))))
			{
				db = 270;
				if (da < 51)
					da ++;
			}
			else if (x>=((XRES+BARSIZE-(510-367))) && x<((XRES+BARSIZE-(510-383))))
			{
				db = 266;
				if (da < 51)
					da ++;
			}
			else if (x>=37 && x<=187)
			{
				if(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL))
				{
					db = 277;
					if (da < 51)
						da ++;
				}
				else if(svf_login)
				{
					db = 259;
					if (svf_open && svf_own && x<=55)
						db = 258;
					if (da < 51)
						da ++;
				}
			}
			else if (x>=((XRES+BARSIZE-(510-385))) && x<=((XRES+BARSIZE-(510-476))))
			{
				db = svf_login ? 261 : 260;
				if (svf_admin)
				{
					db = 268;
				}
				else if (svf_mod)
				{
					db = 271;
				}
				if (da < 51)
					da ++;
			}
			else if (x>=1 && x<=17)
			{
				if(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL))
					db = 276;
				else
					db = 262;
				if (da < 51)
					da ++;
			}
			else if (x>=((XRES+BARSIZE-(510-494))) && x<=((XRES+BARSIZE-(510-509))))
			{
				db = sys_pause ? 264 : 263;
				if (da < 51)
					da ++;
			}
			else if (x>=((XRES+BARSIZE-(510-476))) && x<=((XRES+BARSIZE-(510-491))))
			{
				db = 267;
				if (da < 51)
					da ++;
			}
			else if (x>=19 && x<=35 && svf_open)
			{
				db = 265;
				if (da < 51)
					da ++;
			}
			else if (da > 0)
				da --;
		}
		else if (da > 0)//fade away mouseover text
			da --;
		
		if (!sdl_zoom_trig && zoom_en==1)
			zoom_en = 0;

		if (sdl_key=='z' && !(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL)) && zoom_en==2 && sys_shortcuts==1)
			zoom_en = 1;


		//update coordinates for zoom window before it is placed
		if (sdl_zoom_trig && zoom_en < 2)
		{
			x -= ZSIZE/2;
			y -= ZSIZE/2;
			if (x<0) x=0;
			if (y<0) y=0;
			if (x>XRES-ZSIZE) x=XRES-ZSIZE;
			if (y>YRES-ZSIZE) y=YRES-ZSIZE;
			zoom_x = x;
			zoom_y = y;
			zoom_wx = (x<XRES/2) ? XRES-ZSIZE*ZFACTOR : 0;
			zoom_wy = 0;
			zoom_en = 1;
		}
		//update coordinates for stamp preview before it is placed
		if (load_mode)
		{
			load_x = CELL*((mx-load_w/2+CELL/2)/CELL);
			load_y = CELL*((my-load_h/2+CELL/2)/CELL);
			if (load_x+load_w>XRES) load_x=XRES-load_w;
			if (load_y+load_h>YRES) load_y=YRES-load_h;
			if (load_x<0) load_x=0;
			if (load_y<0) load_y=0;
		}
		//update coordinates for initial copy area before it is decided
		if (save_mode == 1)
		{
			save_x = mx;
			save_y = my;
			if (save_x >= XRES) save_x = XRES-1;
			if (save_y >= YRES) save_y = YRES-1;
			save_w = 1;
			save_h = 1;
		}
		//update coordinates for copy area before it is selected
		if (save_mode == 2)
		{
			save_w = mx + 1 - save_x;
			save_h = my + 1 - save_y;
			if (save_w+save_x>XRES) save_w = XRES-save_x;
			if (save_h+save_y>YRES) save_h = YRES-save_y;
			if (save_w+save_x<0) save_w = 0;
			if (save_h+save_y<0) save_h = 0;
		}

		//mouse clicks ignored when placing zoom
		if (sdl_zoom_trig && zoom_en < 2)
		{
			if (!b && bq)
			{
				zoom_en = 2;
				sdl_zoom_trig = 0;
			}
		}
		//mouse clicks ignored when placing stamps
		else if (load_mode)
		{
			if (!b && bq)
			{
				if (bq != 1 || y > YRES+MENUSIZE-BARSIZE)
				{
					free(load_data);
					load_data = NULL;
					free(load_img);
					load_img = NULL;
					load_mode = 0;
				}
				else if (bq == 1)
				{
					parse_save(load_data, load_size, 0, load_x, load_y, bmap, vx, vy, pv, fvx, fvy, signs, parts, pmap);
					ctrlzSnapshot();
					free(load_data);
					load_data = NULL;
					free(load_img);
					load_img = NULL;
					load_mode = 0;
				}
			}
		}
		//mouse clicks ignored when saving stamps
		else if (save_mode == 1)
		{
			if (b==1)
			{
				save_mode = 2;
			}
			else if (b==4)
			{
				save_mode = 0;
				copy_mode = 0;
			}
		}
		//mouse clicks ignored when saving stamps
		else if (save_mode == 2)
		{
			if (!b)
			{
				if (save_w < 0)
				{
					save_x = save_x + save_w - 1;
					save_w = abs(save_w) + 2;
				}
				if (save_h < 0)
				{
					save_y = save_y + save_h - 1;
					save_h = abs(save_h) + 2;
				}
				if (save_h > 0 && save_w > 0)
				{
					if (copy_mode==1)//CTRL-C, copy
					{
						int oldsave_as = save_as;
						save_as = 3;
						if (clipboard_data)
							free(clipboard_data);
						clipboard_data=build_save(&clipboard_length, save_x, save_y, save_w, save_h, bmap, vx, vy, pv, fvx, fvy, signs, parts, (sdl_mod & KMOD_SHIFT));
						if (clipboard_data)
							clipboard_ready = 1;
						save_as = oldsave_as;
					}
					else if (copy_mode==2)//CTRL-X, cut
					{
						int oldsave_as = save_as;
						save_as = 3;
						if (clipboard_data)
							free(clipboard_data);
						clipboard_data=build_save(&clipboard_length, save_x, save_y, save_w, save_h, bmap, vx, vy, pv, fvx, fvy, signs, parts, (sdl_mod & KMOD_SHIFT));
						if (clipboard_data)
						{
							clipboard_ready = 1;
							clear_area(save_x, save_y, save_w, save_h);
						}
						save_as = oldsave_as;
					}
					else//normal save
					{
						int oldsave_as = save_as;
						if (check_save(save_as%3, save_x, save_y, save_w, save_h, 0))
						{
							save_as = 3;
						}
						stamp_save(save_x, save_y, save_w, save_h);
						save_as = oldsave_as;
					}
				}
				copy_mode = 0;
				save_mode = 0;
			}
		}
		//there is a click
		else if (b)
		{
			if (it > 50)
				it = 50;
			if (y > YRES+MENUSIZE-BARSIZE)//check if mouse is on menu buttons
			{
				if (!lb)//mouse is NOT held down, so it is a first click
				{
					if (x>=189 && x<=202 && svf_login && svf_open && svf_myvote==0 && svf_own==0)
					{
						if (execute_vote(vid_buf, svf_id, "Up"))
						{
							svf_myvote = 1;
						}
					}
					if (x>=204 && x<=217 && svf_login && svf_open && svf_myvote==0 && svf_own==0)
					{
						if (execute_vote(vid_buf, svf_id, "Down"))
						{
							svf_myvote = -1;
						}
					}
					if (x>=219 && x<=(XRES+BARSIZE-(510-349)) && svf_open)
						tag_list_ui(vid_buf);
					if (x>=(XRES+BARSIZE-(510-351)) && x<(XRES+BARSIZE-(510-366)) && !bq)
					{
						//legacy_enable = !legacy_enable;
						simulation_ui(vid_buf);
					}
					if (x>=(XRES+BARSIZE-(510-367)) && x<=(XRES+BARSIZE-(510-383)) && !bq)
					{
						NewSim();
						for (i=0; i<NPART-1; i++)
							parts[i].life = i+1;
						parts[NPART-1].life = -1;
						globalSim->pfree = 0;
					}
					if (x>=(XRES+BARSIZE-(510-385)) && x<=(XRES+BARSIZE-(510-476)))
					{
						login_ui(vid_buf);
						if (svf_login) {
							save_presets(0);
							http_session_check = NULL;
						}
					}
					if(sdl_mod & (KMOD_LCTRL|KMOD_RCTRL))
					{
						if (x>=37 && x<=187)
						{
							save_filename_ui(vid_buf);
									
						}
						if (x>=1 && x<=17)
						{
							catalogue_ui(vid_buf);
						}
					} else {
						if (x>=37 && x<=187 && svf_login)
						{
							if (!svf_open || !svf_own || x>51)
							{
								if (save_name_ui(vid_buf)) {
									if (!execute_save(vid_buf) && svf_id[0]) {
										copytext_ui(vid_buf, "Save ID", "Saved successfully!", svf_id);
									}
									else
									{
										itc = 200;
										strcpy(itc_msg, "Error Saving");
									}
								}
							}
							else
							{
								int oldsave_as = save_as;
								int can_publish = check_save(2,0,0,XRES,YRES,0);
								if (can_publish)
								{
									svf_publish = 0;
									svf_modsave = 1;
									save_as = 3;
								}
								else
								{
									svf_modsave = 0;
									save_as = 5;
								}
								if (execute_save(vid_buf))
								{
									itc = 200;
									strcpy(itc_msg, "Error Saving");
								}
								else
								{
									itc = 200;
									strcpy(itc_msg, "Saved Successfully");
								}
								save_as = oldsave_as;
								
							}
							while (!sdl_poll())
								if (!mouse_get_state(&x, &y))
									break;
							b = bq = 0;
						}
						if (x>=1 && x<=17)
						{
							search_ui(vid_buf);
							memset(pers_bg, 0, (XRES+BARSIZE)*YRES*PIXELSIZE);
							memset(fire_r, 0, sizeof(fire_r));
							memset(fire_g, 0, sizeof(fire_g));
							memset(fire_b, 0, sizeof(fire_b));
						}
					}
					if (x>=19 && x<=35 && svf_last && !bq) {
						//int tpval = sys_pause;
						if (b == 1 || !strncmp(svf_id,"",8))
						{
							parse_save(svf_last, svf_lsize, 1, 0, 0, bmap, vx, vy, pv, fvx, fvy, signs, parts, pmap);
							ctrlzSnapshot();
						}
						else
							open_ui(vid_buf, svf_id, NULL, 0);
						//sys_pause = tpval;
					}
					if (x>=(XRES+BARSIZE-(510-476)) && x<=(XRES+BARSIZE-(510-491)) && !bq)
					{
						render_ui(vid_buf, XRES+BARSIZE-(510-491)+1, YRES+22, 3);
					}
					if (x>=(XRES+BARSIZE-(510-494)) && x<=(XRES+BARSIZE-(510-509)) && !bq)
						sys_pause = !sys_pause;
					lb = 0;
				}
			}
			else if (y<YRES && x<XRES)// mouse is in playing field
			{
				int signi;
				bool clickedSign = false;

				activeToolID = ((b&1) || b == 2) ? 0 : 1;
				Tool* activeTool = activeTools[activeToolID];
				if (activeTools[0]->GetType() == DECO_TOOL && b == 4)
					activeTool = GetToolFromIdentifier("DEFAULT_WL_27");

				//link signs are clicked from here
				if (activeTool->GetWallID() != WL_SIGN+100 || b != 1)
				{
					if (!bq)
						for (signi=0; signi<MAXSIGNS; signi++)
							if (!sregexp(signs[signi].text, "^{[ct]:[0-9]*|.*}$") || !sregexp(signs[signi].text, "^{s:.*|.*}$") || !sregexp(signs[signi].text, "^{b|.*}$"))
							{
								int signx, signy, signw, signh;
								get_sign_pos(signi, &signx, &signy, &signw, &signh);
								if (x>=signx && x<=signx+signw && y>=signy && y<=signy+signh)
								{
									if (signs[signi].text[1] == 'b')
									{
										create_part(-1, signs[signi].x, signs[signi].y, PT_SPRK);
										//hacky hack to cancel out clicks ...
										lm = -1;
										lb = 1;
										break;
									}
									char buff[256];
									int sldr;

									memset(buff, 0, sizeof(buff));

									for (sldr=3; signs[signi].text[sldr] != '|'; sldr++)
										buff[sldr-3] = signs[signi].text[sldr];

									if (buff[0])
									{
										buff[sldr-3] = '\0';
										if (signs[signi].text[1] == 'c')
										{
											open_ui(vid_buf, buff, 0, 0);
										}
										else if (signs[signi].text[1] == 's')
										{
											strcpy(search_expr, buff);
											search_own = 0;
											search_ui(vid_buf);
										}
										else
										{
											char url[256];
											sprintf(url, "http://powdertoy.co.uk/Discussions/Thread/View.html?Thread=%s", buff);
											open_link(url);
										}
									}
									clickedSign = true;
									break;
								}
							}
				}

				//for the click functions, lx and ly, are the positions of where the FIRST click happened.  x,y are current mouse position.
				if (lb) //lb means you are holding mouse down
				{
					toolStrength = 1.0f;
					if (lm == 1)//line tool
					{
						if (sdl_mod & KMOD_ALT)
						{
							float snap_angle = floor(atan2((float)y-ly, (float)x-lx)/(M_PI*0.25)+0.5)*M_PI*0.25;
							float line_mag = sqrtf(pow((float)x-lx,2.0f)+pow((float)y-ly,2.0f));
							line_x = (int)(line_mag*cos(snap_angle)+lx+0.5f);
							line_y = (int)(line_mag*sin(snap_angle)+ly+0.5f);
						}
						else
						{
							line_x = x;
							line_y = y;
						}
						xor_line(lx, ly, line_x, line_y, vid_buf);
						//WIND tool works before you even draw the line while holding shift
						if (activeTool->GetToolID() == SPC_WIND)
						{
							for (j = -currentBrush->GetRadius().Y; j <= currentBrush->GetRadius().Y; j++)
								for (i = -currentBrush->GetRadius().X; i <= currentBrush->GetRadius().X; i++)
									if (lx+i>0 && ly+j>0 && lx+i<XRES && ly+j<YRES && InCurrentBrush(i, j, currentBrush->GetRadius().X, currentBrush->GetRadius().Y))
									{
										vx[(ly+j)/CELL][(lx+i)/CELL] += (line_x-lx)*0.002f;
										vy[(ly+j)/CELL][(lx+i)/CELL] += (line_y-ly)*0.002f;
									}
						}
					}
					else if (lm == 2)//box tool
					{
						xor_line(lx, ly, lx, y, vid_buf);
						xor_line(lx, y, x, y, vid_buf);
						xor_line(x, y, x, ly, vid_buf);
						xor_line(x, ly, lx, ly, vid_buf);
					}
					else if (!lm) //while mouse is held down, it draws lines between previous and current positions
					{
						if (sdl_mod & KMOD_SHIFT)
							toolStrength = 10.0f;
						else if (sdl_mod & KMOD_CTRL)
							toolStrength = .1f;
						activeTool->DrawLine(currentBrush, Point(lx, ly), Point(x, y), true);
						lx = x;
						ly = y;
					}
				}
				else if (!clickedSign) //it is the first click
				{
					toolStrength = 1.0f;
					//start line tool
					if ((sdl_mod & (KMOD_SHIFT)) && !(sdl_mod & (KMOD_CTRL)))
					{
						lx = x;
						ly = y;
						lb = b;
						lm = 1;//line
					}
					//start box tool
					else if ((sdl_mod & (KMOD_CTRL)) && !(sdl_mod & (KMOD_SHIFT|KMOD_ALT)))
					{
						lx = x;
						ly = y;
						lb = b;
						lm = 2;//box
					}
					//flood fill
					else if ((sdl_mod & (KMOD_CTRL)) && (sdl_mod & (KMOD_SHIFT)) && !(sdl_mod & (KMOD_ALT)))
					{
						if (!bq)
							ctrlzSnapshot();

						activeTool->FloodFill(Point(x, y));
						lx = x;
						ly = y;
						lb = 0;
						lm = 0;
					}
					//sample
					else if (((sdl_mod & (KMOD_ALT)) && !(sdl_mod & (KMOD_SHIFT|KMOD_CTRL))) || b==SDL_BUTTON_MIDDLE)
					{
						if (y>=0 && y<YRES && x>=0 && x<XRES)
						{
							if (activeTool->GetType() == DECO_TOOL)
							{
								unsigned int tempcolor = vid_buf[(y)*(XRES+BARSIZE)+(x)];
								int cr = PIXR(tempcolor);
								int cg = PIXG(tempcolor);
								int cb = PIXB(tempcolor);
								if (cr || cg || cb)
								{
									if (cr && cr<255) cr++;
									if (cg && cg<255) cg++;
									if (cb && cb<255) cb++;
									decocolor = (255<<24)|PIXRGB(cr, cg, cb);
									currR = PIXR(decocolor), currG = PIXG(decocolor), currB = PIXB(decocolor), currA = decocolor>>24;
									RGB_to_HSV(currR, currG, currB, &currH, &currS, &currV);
								}
							}
							else
							{
								int sample = pmap[y][x];
								if (sample || (sample = photons[y][x]))
								{
									if ((sample&0xFF) == PT_LIFE)
									{
										activeTools[0] = new Tool(GOL_TOOL, parts[sample>>8].ctype, "DEFAULT_PT_LIFE_" + std::string(gmenu[parts[sample>>8].ctype].name));
									}
									else
									{
										activeTools[0] = new Tool(ELEMENT_TOOL, sample&0xFF, globalSim->elements[sample&0xFF].Identifier);
									}
								}
								/*else if (bmap[y/CELL][x/CELL]) //TODO: Wall sample
								{
									activeTools[0] = new Tool(WALL_TOOL, bmap[y/CELL][x/CELL], "DEFAULT_PT_WL_" + (bmap[y/CELL][x/CELL]-UI_WALLSTART));
								}*/
							}
						}
						lx = x;
						ly = y;
						lb = 0;
						lm = 0;
					}
					else //normal click, spawn element
					{
						//Copy state before drawing any particles (for undo)
						ctrlzSnapshot();

						//get tool strength, shift (or ctrl+shift) makes it faster and ctrl makes it slower
						if (sdl_mod & KMOD_SHIFT)
							toolStrength = 10.0f;
						else if (sdl_mod & KMOD_CTRL)
							toolStrength = .1f;

						activeTool->DrawPoint(currentBrush, Point(x, y));
						lx = x;
						ly = y;
						lb = b;
						lm = 0;
					}
				}
			}
		}
		else
		{
			if (lb && lm) //lm is box/line tool
			{
				ctrlzSnapshot();
				activeToolID = ((lb&1) || lb == 2) ? 0 : 1;
				Tool* activeTool = activeTools[activeToolID];
				if (activeTools[0]->GetType() == DECO_TOOL && lb == 4)
					activeTool = GetToolFromIdentifier("DEFAULT_WL_37");

				if (lm == 1)
					activeTool->DrawLine(currentBrush, Point(lx, ly), Point(line_x, line_y), false);
				else if (lm == 2)
					activeTool->DrawRect(currentBrush, Point(lx, ly), Point(x, y));
				lm = 0;
			}
			lb = 0;
		}

		if (load_mode)//draw preview of stamp
		{
			draw_image(vid_buf, load_img, load_x, load_y, load_w, load_h, 128);
			xor_rect(vid_buf, load_x, load_y, load_w, load_h);
		}

		if (save_mode)//draw dotted lines for selection
		{
			int savex = save_x, savey = save_y, savew = save_w, saveh = save_h;
			if (savew < 0)
			{
				savex = savex + savew - 1;
				savew = abs(savew) + 2;
			}
			if (saveh < 0)
			{
				savey = savey + saveh - 1;
				saveh = abs(saveh) + 2;
			}
			fillrect(vid_buf,-1,-1,savex+1,YRES,0,0,0,100);
			fillrect(vid_buf,savex-1,-1,savew+1,savey+1,0,0,0,100);
			fillrect(vid_buf,savex-1,savey+saveh-1,savew+1,YRES-savey-saveh+1,0,0,0,100);
			fillrect(vid_buf,savex+savew-1,-1,XRES-savex-savew+1,YRES+1,0,0,0,100);
			xor_rect(vid_buf, savex, savey, savew, saveh);
			da = 51;//draws mouseover text for the message
			if (copy_mode != 2)
				db = 269;//the save message
			else
				db = 278;
		}

		if (zoom_en!=1 && !load_mode && !save_mode)//draw normal cursor
		{
			render_cursor(vid_buf, mx, my, activeTools[activeToolID], currentBrush->GetRadius().X, currentBrush->GetRadius().Y);
			mousex = mx;
			mousey = my;
		}
#ifdef OGLR
		draw_parts_fbo();
#endif		
		if (zoom_en)
			render_zoom(vid_buf);

		if (da)
			switch (db)//various mouseover messages, da is the alpha
			{
			case 256:
				drawtext(vid_buf, 16, YRES-24, "Add simulation tags", 255, 255, 255, da*5);
				break;
			case 257:
				drawtext(vid_buf, 16, YRES-24, "Add and remove simulation tags", 255, 255, 255, da*5);
				break;
			case 258:
				drawtext(vid_buf, 16, YRES-24, "Save the simulation under the current name", 255, 255, 255, da*5);
				break;
			case 259:
				drawtext(vid_buf, 16, YRES-24, "Save the simulation under a new name", 255, 255, 255, da*5);
				break;
			case 260:
				drawtext(vid_buf, 16, YRES-24, "Sign into the Simulation Server", 255, 255, 255, da*5);
				break;
			case 261:
				drawtext(vid_buf, 16, YRES-24, "Sign into the Simulation Server under a new name", 255, 255, 255, da*5);
				break;
			case 262:
				drawtext(vid_buf, 16, YRES-24, "Find & open a simulation", 255, 255, 255, da*5);
				break;
			case 263:
				drawtext(vid_buf, 16, YRES-24, "Pause the simulation \bg(space)", 255, 255, 255, da*5);
				break;
			case 264:
				drawtext(vid_buf, 16, YRES-24, "Resume the simulation \bg(space)", 255, 255, 255, da*5);
				break;
			case 265:
				drawtext(vid_buf, 16, YRES-24, "Reload the simulation \bg(ctrl+r)", 255, 255, 255, da*5);
				break;
			case 266:
				drawtext(vid_buf, 16, YRES-24, "Erase all particles and walls", 255, 255, 255, da*5);
				break;
			case 267:
				drawtext(vid_buf, 16, YRES-24, "Change display mode", 255, 255, 255, da*5);
				break;
			case 268:
				drawtext(vid_buf, 16, YRES-24, "Annuit C\245ptis", 255, 255, 255, da*5);
				break;
			case 269:
				drawtext(vid_buf, 16, YRES-24, "Click-and-drag to specify a rectangle to copy (right click = cancel)", 255, 216, 32, da*5);
				break;
			case 270:
				drawtext(vid_buf, 16, YRES-24, "Simulation options", 255, 255, 255, da*5);
				break;
			case 271:
				drawtext(vid_buf, 16, YRES-24, "You're a moderator", 255, 255, 255, da*5);
				break;
			case 272:
				drawtext(vid_buf, 16, YRES-24, "Like/Dislike this save", 255, 255, 255, da*5);
				break;
			case 273:
				drawtext(vid_buf, 16, YRES-24, "You like this", 255, 255, 255, da*5);
				break;
			case 274:
				drawtext(vid_buf, 16, YRES-24, "You dislike this", 255, 255, 255, da*5);
				break;
			case 275:
				drawtext(vid_buf, 16, YRES-24, "You cannot vote on your own save", 255, 255, 255, da*5);
				break;
			case 276:
				drawtext(vid_buf, 16, YRES-24, "Open a simulation from your hard drive \bg(ctrl+o)", 255, 255, 255, da*5);
				break;
			case 277:
				drawtext(vid_buf, 16, YRES-24, "Save the simulation to your hard drive", 255, 255, 255, da*5);
				break;
			case 278: //Fix for Ctrl + X showing copy message
				drawtext(vid_buf, 16, YRES-24, "Click-and-drag to specify a rectangle to copy and then cut (right click = cancel)", 255, 216, 32, da*5);
				break;
			default:
				drawtext(vid_buf, 16, YRES-24, (char *)ptypes[db].descs, 255, 255, 255, da*5);
			}
		if (itc)//message in the middle of the screen, such as view mode changes
		{
			itc--;
			drawtext_outline(vid_buf, (XRES-textwidth(itc_msg))/2, ((YRES/2)-10), itc_msg, 255, 255, 255, itc>51?255:itc*5, 0, 0, 0, itc>51?255:itc*5);
		}
		if (it)//intro message
		{
			it--;
			drawtext(vid_buf, 16, 20, it_msg, 255, 255, 255, it>51?255:it*5);
		}

		if (old_version)
		{
			clearrect(vid_buf, XRES-21-old_ver_len, YRES-24, old_ver_len+9, 17);
			drawtext(vid_buf, XRES-16-old_ver_len, YRES-19, old_ver_msg, 255, 216, 32, 255);
			drawrect(vid_buf, XRES-19-old_ver_len, YRES-22, old_ver_len+5, 13, 255, 216, 32, 255);
		}
		
		if (svf_messages)
		{
			sprintf(new_message_msg, "You have %d new message%s, Click to view", svf_messages, (svf_messages>1)?"s":"");
			new_message_len = textwidth(new_message_msg);
			
			clearrect(vid_buf, XRES-21-new_message_len, YRES-39, new_message_len+9, 17);
			drawtext(vid_buf, XRES-16-new_message_len, YRES-34, new_message_msg, 255, 186, 32, 255);
			drawrect(vid_buf, XRES-19-new_message_len, YRES-37, new_message_len+5, 13, 255, 186, 32, 255);
		}

		if (elapsedTime != currentTime && main_loop == 2)
		{
			frames = frames + 1;
			totalfps = totalfps + FPSB2;
			if (FPSB2 > maxfps)
				maxfps = FPSB2;
		}
		if (lastx == mousex && lasty == mousey)
		{
			if (!afk)
			{
				afk = 1;
				afkstart = currentTime;
			}
		}
		else if (afk)
		{
			afk = 0;
			totalafktime = totalafktime + afktime;
			afktime = 0;
		}
		if (afk && currentTime - afkstart > 30000)
			afktime = currentTime - afkstart - 30000;
		lastx = mousex;
		lasty = mousey;

		if (autosave > 0 && frames%autosave == 0)
		{
			tab_save(1, 0);
		}
		if (hud_enable)
		{
			SetLeftHudText(FPSB2, it);
			SetRightHudText(x, y);

			DrawHud(it);

			if (drawinfo)
				DrawRecordsInfo();

			DrawLuaLogs();
		}

		if (console_mode)
		{
			if (console_ui(vid_buf) == -1)
				break;
		}

		sdl_blit(0, 0, XRES+BARSIZE, YRES+MENUSIZE, vid_buf, XRES+BARSIZE);

		//Setting an element for the stick man
		if (player.spwn==0)
		{
			int sr = activeTools[1]->GetElementID();
			if ((sr>0 && sr<PT_NUM && ptypes[sr].enabled && ptypes[sr].falldown>0) || sr==SPC_AIR || sr == PT_NEUT || sr == PT_PHOT || sr == PT_LIGH)
				player.elem = sr;
			else
				player.elem = PT_DUST;
		}
		if (player2.spwn==0)
		{
			int sr = activeTools[1]->GetElementID();
			if ((sr>0 && sr<PT_NUM && ptypes[sr].enabled && ptypes[sr].falldown>0) || sr==SPC_AIR || sr == PT_NEUT || sr == PT_PHOT || sr == PT_LIGH)
				player2.elem = sr;
			else
				player2.elem = PT_DUST;
		}

#if !defined(DEBUG) && !defined(_DEBUG)
	if (!signal_hooks)
	{
		signal(SIGSEGV, SigHandler);
		signal(SIGFPE, SigHandler);
		signal(SIGILL, SigHandler);
		signal(SIGABRT, SigHandler);
		signal_hooks = 1;
	}
#endif
	}
	
	if (sound_enable)
		SDL_CloseAudio();
	SaveWindowPosition();
	http_done();
	gravity_cleanup();
	save_presets(0);
#ifdef LUACONSOLE
	luacon_close();
#endif
	ClearMenusections();
	delete currentBrush;
#ifdef PTW32_STATIC_LIB
    pthread_win32_thread_detach_np();
    pthread_win32_process_detach_np();
#endif
    if (part_vbuf_store)
    	free(part_vbuf_store);
	for (i = 1; i < 10; i++)
	{
		char name[30] = {0};
		sprintf(name,"tabs%s%d.stm",PATH_SEP,i);
		remove(name);
	}
#ifdef WIN32
	_rmdir("tabs");
#else
	rmdir("tabs");
#endif
	return 0;
}
#endif