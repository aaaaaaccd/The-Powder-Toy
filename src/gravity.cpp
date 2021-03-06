/**
 * Powder Toy - Newtonian gravity
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

#include <cmath>
#include <cstring>
#include <sys/types.h>
#include <iostream>
#include "common/tpt-thread.h"
#include "defines.h"
#include "gravity.h"
#include "powder.h"
#include "simulation/CoordStack.h"
#include "simulation/WallNumbers.h"

#ifdef GRAVFFT
#include <fftw3.h>
int grav_fft_status = 0;
#ifdef STATIC_LIBS
FILE _iob[3];
#endif
#endif

float *gravmap = NULL;//Maps to be used by the main thread
float *gravp = NULL;
float *gravy = NULL;
float *gravx = NULL;
unsigned *gravmask = NULL;
int gravity_cleared = 0;//used to signal when gravx and gravy have been cleared in clear_sim, meaning that th_gravx/y and th_ogravmap need to be cleared to match them

float *th_ogravmap = NULL;// Maps to be processed by the gravity thread
float *th_gravmap = NULL;
float *th_gravx = NULL;
float *th_gravy = NULL;
float *th_gravp = NULL;

int gravwl_timeout = 0;
int gravityMode = 0; // starts enabled in "vertical" mode...
bool ngrav_enable = false; //Newtonian gravity
#ifdef ANDROID
bool ngrav_completedisable = false;
#else
bool ngrav_completedisable = false;
#endif
int th_gravchanged = 0;

pthread_t gravthread;
pthread_mutex_t gravmutex;
pthread_cond_t gravcv;
int grav_ready = 0;
int gravthread_done = 0;

void bilinear_interpolation(float *src, float *dst, int sw, int sh, int rw, int rh)
{
	int y, x, fxceil, fyceil;
	float fx, fy, fyc, fxc;
	double intp;
	float tr, tl, br, bl;
	//Bilinear interpolation for upscaling
	for (y=0; y<rh; y++)
		for (x=0; x<rw; x++)
		{
			fx = ((float)x)*((float)sw)/((float)rw);
			fy = ((float)y)*((float)sh)/((float)rh);
			fxc = (float)modf(fx, &intp);
			fyc = (float)modf(fy, &intp);
			fxceil = (int)ceil(fx);
			fyceil = (int)ceil(fy);
			if (fxceil>=sw) fxceil = sw-1;
			if (fyceil>=sh) fyceil = sh-1;
			tr = src[sw*(int)floor(fy)+fxceil];
			tl = src[sw*(int)floor(fy)+(int)floor(fx)];
			br = src[sw*fyceil+fxceil];
			bl = src[sw*fyceil+(int)floor(fx)];
			dst[rw*y+x] = ((tl*(1.0f-fxc))+(tr*(fxc)))*(1.0f-fyc) + ((bl*(1.0f-fxc))+(br*(fxc)))*(fyc);				
		}
}

void gravity_init()
{
	//Allocate full size Gravmaps
	th_ogravmap = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	th_gravmap = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	th_gravy = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	th_gravx = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	th_gravp = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	gravmap = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	gravy = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	gravx = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	gravp = (float*)calloc((XRES/CELL)*(YRES/CELL), sizeof(float));
	gravmask = (unsigned*)calloc((XRES/CELL)*(YRES/CELL), sizeof(unsigned));
}

void gravity_cleanup()
{
	stop_grav_async();
#ifdef GRAVFFT
	grav_fft_cleanup();
#endif
}

void gravity_update_async()
{
	int result;
	if(ngrav_enable)
	{
		pthread_mutex_lock(&gravmutex);
		result = grav_ready;
		if(result) //Did the gravity thread finish?
		{
			if (!sys_pause||framerender){ //Only update if not paused
				//Switch the full size gravmaps, we don't really need the two above any more
				float *tmpf;

				if (gravity_cleared)
				{
					memset(th_gravx, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
					memset(th_gravy, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
					memset(th_gravp, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
					memset(th_ogravmap, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
					gravity_cleared = 0;
				}

				if(th_gravchanged)
				{
				#if !defined(GRAVFFT) && defined(GRAV_DIFF)
					memcpy(gravy, th_gravy, (XRES/CELL)*(YRES/CELL)*sizeof(float));
					memcpy(gravx, th_gravx, (XRES/CELL)*(YRES/CELL)*sizeof(float));
					memcpy(gravp, th_gravp, (XRES/CELL)*(YRES/CELL)*sizeof(float));
				#else
					tmpf = gravy;
					gravy = th_gravy;
					th_gravy = tmpf;
	
					tmpf = gravx;
					gravx = th_gravx;
					th_gravx = tmpf;
	
					tmpf = gravp;
					gravp = th_gravp;
					th_gravp = tmpf;
				#endif
				}

				tmpf = gravmap;
				gravmap = th_gravmap;
				th_gravmap = tmpf;

				grav_ready = 0; //Tell the other thread that we're ready for it to continue
				pthread_cond_signal(&gravcv);
			}
		}
		pthread_mutex_unlock(&gravmutex);
		//Apply the gravity mask
		membwand(gravy, gravmask, (XRES/CELL)*(YRES/CELL)*sizeof(float), (XRES/CELL)*(YRES/CELL)*sizeof(unsigned));
		membwand(gravx, gravmask, (XRES/CELL)*(YRES/CELL)*sizeof(float), (XRES/CELL)*(YRES/CELL)*sizeof(unsigned));
		memset(gravmap, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	}
}

TH_ENTRY_POINT void* update_grav_async(void* unused)
{
	int done = 0;
	int thread_done = 0;
	memset(th_ogravmap, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(th_gravmap, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(th_gravy, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(th_gravx, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(th_gravp, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	//memset(th_gravy, 0, XRES*YRES*sizeof(float));
	//memset(th_gravx, 0, XRES*YRES*sizeof(float));
	//memset(th_gravp, 0, XRES*YRES*sizeof(float));
#ifdef GRAVFFT
	if (!grav_fft_status)
		grav_fft_init();
#endif
	while(!thread_done){
		if(!done){
			update_grav();
			done = 1;
			pthread_mutex_lock(&gravmutex);
			
			grav_ready = done;
			thread_done = gravthread_done;
			
			pthread_mutex_unlock(&gravmutex);
		} else {
			pthread_mutex_lock(&gravmutex);
			pthread_cond_wait(&gravcv, &gravmutex);
		    
			done = grav_ready;
			thread_done = gravthread_done;
			
			pthread_mutex_unlock(&gravmutex);
		}
	}
	pthread_exit(NULL);
	return 0;
}

void start_grav_async()
{
	if (ngrav_completedisable)
		return;
	if(!ngrav_enable){
		gravthread_done = 0;
		grav_ready = 0;
		pthread_mutex_init (&gravmutex, NULL);
		pthread_cond_init(&gravcv, NULL);
		pthread_create(&gravthread, NULL, update_grav_async, NULL); //Start asynchronous gravity simulation
		ngrav_enable = true;
	}
	memset(gravy, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(gravx, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(gravp, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
}

void stop_grav_async()
{
	if (ngrav_completedisable)
		return;
	if(ngrav_enable){
		pthread_mutex_lock(&gravmutex);
		gravthread_done = 1;
		pthread_cond_signal(&gravcv);
		pthread_mutex_unlock(&gravmutex);
		pthread_join(gravthread, NULL);
		pthread_mutex_destroy(&gravmutex); //Destroy the mutex
		ngrav_enable = false;
	}
	//Clear the grav velocities
	memset(gravy, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(gravx, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(gravp, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
}

#ifdef GRAVFFT
float *th_ptgravx, *th_ptgravy, *th_gravmapbig, *th_gravxbig, *th_gravybig;
fftwf_complex *th_ptgravxt, *th_ptgravyt, *th_gravmapbigt, *th_gravxbigt, *th_gravybigt;
fftwf_plan plan_gravmap, plan_gravx_inverse, plan_gravy_inverse;

void grav_fft_init()
{
	int xblock2 = XRES/CELL*2;
	int yblock2 = YRES/CELL*2;
	int x, y, fft_tsize = (xblock2/2+1)*yblock2;
	float distance, scaleFactor;
	fftwf_plan plan_ptgravx, plan_ptgravy;
	if (grav_fft_status) return;

	//use fftw malloc function to ensure arrays are aligned, to get better performance
	th_ptgravx = (float*)fftwf_malloc(xblock2*yblock2*sizeof(float));
	th_ptgravy = (float*)fftwf_malloc(xblock2*yblock2*sizeof(float));
	th_ptgravxt = (fftwf_complex*)fftwf_malloc(fft_tsize*sizeof(fftwf_complex));
	th_ptgravyt = (fftwf_complex*)fftwf_malloc(fft_tsize*sizeof(fftwf_complex));
	th_gravmapbig = (float*)fftwf_malloc(xblock2*yblock2*sizeof(float));
	th_gravmapbigt = (fftwf_complex*)fftwf_malloc(fft_tsize*sizeof(fftwf_complex));
	th_gravxbig = (float*)fftwf_malloc(xblock2*yblock2*sizeof(float));
	th_gravybig = (float*)fftwf_malloc(xblock2*yblock2*sizeof(float));
	th_gravxbigt = (fftwf_complex*)fftwf_malloc(fft_tsize*sizeof(fftwf_complex));
	th_gravybigt = (fftwf_complex*)fftwf_malloc(fft_tsize*sizeof(fftwf_complex));

	//select best algorithm, could use FFTW_PATIENT or FFTW_EXHAUSTIVE but that increases the time taken to plan, and I don't see much increase in execution speed
	plan_ptgravx = fftwf_plan_dft_r2c_2d(yblock2, xblock2, th_ptgravx, th_ptgravxt, FFTW_MEASURE);
	plan_ptgravy = fftwf_plan_dft_r2c_2d(yblock2, xblock2, th_ptgravy, th_ptgravyt, FFTW_MEASURE);
	plan_gravmap = fftwf_plan_dft_r2c_2d(yblock2, xblock2, th_gravmapbig, th_gravmapbigt, FFTW_MEASURE);
	plan_gravx_inverse = fftwf_plan_dft_c2r_2d(yblock2, xblock2, th_gravxbigt, th_gravxbig, FFTW_MEASURE);
	plan_gravy_inverse = fftwf_plan_dft_c2r_2d(yblock2, xblock2, th_gravybigt, th_gravybig, FFTW_MEASURE);

	//(XRES/CELL)*(YRES/CELL)*4 is size of data array, scaling needed because FFTW calculates an unnormalized DFT
	scaleFactor = (float)(-M_GRAV/((XRES/CELL)*(YRES/CELL)*4));
	//calculate velocity map caused by a point mass
	for (y=0; y<yblock2; y++)
	{
		for (x=0; x<xblock2; x++)
		{
			if (x==XRES/CELL && y==YRES/CELL) continue;
			distance = sqrtf(pow((float)x-(XRES/CELL), 2.0f) + pow((float)y-(YRES/CELL), 2.0f));
			th_ptgravx[y*xblock2+x] = scaleFactor*(x-(XRES/CELL)) / pow(distance, 3);
			th_ptgravy[y*xblock2+x] = scaleFactor*(y-(YRES/CELL)) / pow(distance, 3);
		}
	}
	th_ptgravx[yblock2*xblock2/2+xblock2/2] = 0.0f;
	th_ptgravy[yblock2*xblock2/2+xblock2/2] = 0.0f;

	//transform point mass velocity maps
	fftwf_execute(plan_ptgravx);
	fftwf_execute(plan_ptgravy);
	fftwf_destroy_plan(plan_ptgravx);
	fftwf_destroy_plan(plan_ptgravy);
	fftwf_free(th_ptgravx);
	fftwf_free(th_ptgravy);

	//clear padded gravmap
	memset(th_gravmapbig,0,xblock2*yblock2*sizeof(float));

	grav_fft_status = 1;
}

void grav_fft_cleanup()
{
	if (!grav_fft_status) return;
	fftwf_free(th_ptgravxt);
	fftwf_free(th_ptgravyt);
	fftwf_free(th_gravmapbig);
	fftwf_free(th_gravmapbigt);
	fftwf_free(th_gravxbig);
	fftwf_free(th_gravybig);
	fftwf_free(th_gravxbigt);
	fftwf_free(th_gravybigt);
	fftwf_destroy_plan(plan_gravmap);
	fftwf_destroy_plan(plan_gravx_inverse);
	fftwf_destroy_plan(plan_gravy_inverse);
	grav_fft_status = 0;
}

void update_grav()
{
	int x, y;
	int xblock2 = XRES/CELL*2, yblock2 = YRES/CELL*2;
	int i, fft_tsize = (xblock2/2+1)*yblock2;
	float mr, mc, pr, pc, gr, gc;
	float *tmp;
	if (memcmp(th_ogravmap, th_gravmap, sizeof(float)*(XRES/CELL)*(YRES/CELL))!=0)
	{
		th_gravchanged = 1;

		membwand(th_gravmap, gravmask, (XRES/CELL)*(YRES/CELL)*sizeof(float), (XRES/CELL)*(YRES/CELL)*sizeof(unsigned));
		//copy gravmap into padded gravmap array
		for (y=0; y<YRES/CELL; y++)
		{
			for (x=0; x<XRES/CELL; x++)
			{
				th_gravmapbig[(y+YRES/CELL)*xblock2+XRES/CELL+x] = th_gravmap[y*(XRES/CELL)+x];
			}
		}
		//transform gravmap
		fftwf_execute(plan_gravmap);
		//do convolution (multiply the complex numbers)
		for (i=0; i<fft_tsize; i++)
		{
			mr = th_gravmapbigt[i][0];
			mc = th_gravmapbigt[i][1];
			pr = th_ptgravxt[i][0];
			pc = th_ptgravxt[i][1];
			gr = mr*pr-mc*pc;
			gc = mr*pc+mc*pr;
			th_gravxbigt[i][0] = gr;
			th_gravxbigt[i][1] = gc;
			pr = th_ptgravyt[i][0];
			pc = th_ptgravyt[i][1];
			gr = mr*pr-mc*pc;
			gc = mr*pc+mc*pr;
			th_gravybigt[i][0] = gr;
			th_gravybigt[i][1] = gc;
		}
		//inverse transform, and copy from padded arrays into normal velocity maps
		fftwf_execute(plan_gravx_inverse);
		fftwf_execute(plan_gravy_inverse);
		for (y=0; y<YRES/CELL; y++)
		{
			for (x=0; x<XRES/CELL; x++)
			{
				th_gravx[y*(XRES/CELL)+x] = th_gravxbig[y*xblock2+x];
				th_gravy[y*(XRES/CELL)+x] = th_gravybig[y*xblock2+x];
				th_gravp[y*(XRES/CELL)+x] = sqrtf(pow(th_gravxbig[y*xblock2+x],2)+pow(th_gravybig[y*xblock2+x],2));
			}
		}
	}
	else
	{
		th_gravchanged = 0;
	}
	tmp = th_ogravmap;
	th_ogravmap = th_gravmap;
	th_gravmap = tmp;
}

#else
// gravity without fast Fourier transforms

void update_grav(void)
{
	int x, y, i, j;
	float val, distance;
	th_gravchanged = 0;
#ifndef GRAV_DIFF
	int changed = 0;
	//Find any changed cells
	for (i=0; i<YRES/CELL; i++)
	{
		if(changed)
			break;
		for (j=0; j<XRES/CELL; j++)
		{
			if(th_ogravmap[i*(XRES/CELL)+j]!=th_gravmap[i*(XRES/CELL)+j]){
				changed = 1;
				break;
			}
		}
	}
	if(!changed)
		goto fin;
	memset(th_gravy, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
	memset(th_gravx, 0, (XRES/CELL)*(YRES/CELL)*sizeof(float));
#endif
	th_gravchanged = 1;
	membwand(th_gravmap, gravmask, (XRES/CELL)*(YRES/CELL)*sizeof(float), (XRES/CELL)*(YRES/CELL)*sizeof(unsigned));
	for (i = 0; i < YRES / CELL; i++) {
		for (j = 0; j < XRES / CELL; j++) {
#ifdef GRAV_DIFF
			if (th_ogravmap[i*(XRES/CELL)+j] != th_gravmap[i*(XRES/CELL)+j])
			{
#else
			if (th_gravmap[i*(XRES/CELL)+j] > 0.0001f || th_gravmap[i*(XRES/CELL)+j]<-0.0001f) //Only calculate with populated or changed cells.
			{
#endif
				for (y = 0; y < YRES / CELL; y++) {
					for (x = 0; x < XRES / CELL; x++) {
						if (x == j && y == i)//Ensure it doesn't calculate with itself
							continue;
						distance = sqrt(pow(j - x, 2) + pow(i - y, 2));
#ifdef GRAV_DIFF
						val = th_gravmap[i*(XRES/CELL)+j] - th_ogravmap[i*(XRES/CELL)+j];
#else
						val = th_gravmap[i*(XRES/CELL)+j];
#endif
						th_gravx[y*(XRES/CELL)+x] += M_GRAV * val * (j - x) / pow(distance, 3);
						th_gravy[y*(XRES/CELL)+x] += M_GRAV * val * (i - y) / pow(distance, 3);
						th_gravp[y*(XRES/CELL)+x] += M_GRAV * val / pow(distance, 2);
					}
				}
			}
		}
	}
#ifndef GRAV_DIFF
fin:
#endif
	memcpy(th_ogravmap, th_gravmap, (XRES/CELL)*(YRES/CELL)*sizeof(float));
}
#endif


bool grav_mask_r(int x, int y, char checkmap[YRES/CELL][XRES/CELL], char shape[YRES/CELL][XRES/CELL])
{
	int x1, x2;
	bool ret = false;
	try
	{
		CoordStack cs;
		cs.push(x, y);
		do
		{
			cs.pop(x, y);
			x1 = x2 = x;
			while (x1 >= 0)
			{
				if (x1 == 0)
				{
					ret = true;
					break;
				}
				else if (checkmap[y][x1-1] || bmap[y][x1-1] == WL_GRAV)
					break;
				x1--;
			}
			while (x2 <= XRES/CELL-1)
			{
				if (x2 == XRES/CELL-1)
				{
					ret = true;
					break;
				}
				else if (checkmap[y][x2+1] || bmap[y][x2+1] == WL_GRAV)
					break;
				x2++;
			}
			for (x = x1; x <= x2; x++)
			{
				shape[y][x] = 1;
				checkmap[y][x] = 1;
			}
			if (y >= 1)
				for (x=x1; x<=x2; x++)
					if (!checkmap[y-1][x] && bmap[y-1][x] != WL_GRAV)
					{
						if (y-1 == 0)
							ret = true;
						cs.push(x, y-1);
					}
			if (y < YRES/CELL-1)
				for (x=x1; x<=x2; x++)
					if (!checkmap[y+1][x] && bmap[y+1][x] != WL_GRAV)
					{
						if (y+1 == YRES/CELL-1)
							ret = true;
						cs.push(x, y+1);
					}
		} while (cs.getSize()>0);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		ret = false;
	}
	return ret;
}

struct mask_el;
typedef struct mask_el mask_el;
struct mask_el {
	char *shape;
	char shapeout;
	mask_el *next;
};
void mask_free(mask_el *c_mask_el){
	if(c_mask_el==NULL)
		return;
	if(c_mask_el->next!=NULL)
		mask_free(c_mask_el->next);
	free(c_mask_el->shape);
	free(c_mask_el);
}
void gravity_mask()
{
	char checkmap[YRES/CELL][XRES/CELL];
	int x = 0, y = 0;
	unsigned maskvalue;
	mask_el *t_mask_el = NULL;
	mask_el *c_mask_el = NULL;
	if(!gravmask || ngrav_completedisable)
		return;
	memset(checkmap, 0, sizeof(checkmap));
	for(x = 0; x < XRES/CELL; x++)
	{
		for(y = 0; y < YRES/CELL; y++)
		{
			if(bmap[y][x]!=WL_GRAV && checkmap[y][x] == 0)
			{
				//Create a new shape
				if(t_mask_el==NULL){
					t_mask_el = (mask_el*)malloc(sizeof(mask_el));
					t_mask_el->shape = (char*)malloc((XRES/CELL)*(YRES/CELL));
					memset(t_mask_el->shape, 0, (XRES/CELL)*(YRES/CELL));
					t_mask_el->shapeout = 0;
					t_mask_el->next = NULL;
					c_mask_el = t_mask_el;
				} else {
					c_mask_el->next = (mask_el*)malloc(sizeof(mask_el));
					c_mask_el = c_mask_el->next;
					c_mask_el->shape = (char*)malloc((XRES/CELL)*(YRES/CELL));
					memset(c_mask_el->shape, 0, (XRES/CELL)*(YRES/CELL));
					c_mask_el->shapeout = 0;
					c_mask_el->next = NULL;
				}
				//Fill the shape
				if (grav_mask_r(x, y, checkmap, (char(*)[XRES/CELL])c_mask_el->shape))
					c_mask_el->shapeout = 1;
			}
		}
	}
	c_mask_el = t_mask_el;
	memset(gravmask, 0, (XRES/CELL)*(YRES/CELL)*sizeof(unsigned));
	while(c_mask_el!=NULL)
	{
		char *cshape = c_mask_el->shape;
		for(x = 0; x < XRES/CELL; x++)
		{
			for(y = 0; y < YRES/CELL; y++)
			{
				if(cshape[y*(XRES/CELL)+x]){
					if(c_mask_el->shapeout)
						maskvalue = 0xFFFFFFFF;
					else
						maskvalue = 0x00000000;
					gravmask[y*(XRES/CELL)+x] = maskvalue;
				}
			}
		}
		c_mask_el = c_mask_el->next;	
	}
	mask_free(t_mask_el);
	gravity_cleared = 1;
}
