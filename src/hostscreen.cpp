/*
 * $Header$
 *
 * STanda 2001
 */

#include "sysdeps.h"
#include "hardware.h"
#include "cpu_emulation.h"
#include "memory.h"
#include "hostscreen.h"
#include "parameters.h"

#define DEBUG 0
#include "debug.h"

/*
void SelectVideoMode()
{
	SDL_Rect **modes;
	uint32 i;

	// Get available fullscreen/hardware modes
	modes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);

	// Check is there are any modes available
	if (modes == (SDL_Rect **) 0) {
		printf("No modes available!\n");
		exit(-1);
	}

	// Check if or resolution is restricted
	if (modes == (SDL_Rect **) - 1) {
		printf("All resolutions available.\n");
	}
	else {
		// Print valid modes
		printf("Available Modes\n");
		for (i = 0; modes[i]; ++i)
			printf("  %d x %d\n", modes[i]->w, modes[i]->h);
	}
}
*/


void HostScreen::makeSnapshot()
{
	char filename[15];
	sprintf( filename, "snap%03d.bmp", snapCounter++ );

	SDL_SaveBMP( surf, filename );
}


void HostScreen::toggleFullScreen()
{
	bx_options.video.fullscreen = !bx_options.video.fullscreen;
	SDL_WM_ToggleFullScreen(mainSurface);
}

void HostScreen::saveBackground()
{
	if (backgroundSurf == NULL) {
		backgroundSurf = SDL_ConvertSurface(mainSurface, mainSurface->format, mainSurface->flags & ~SDL_HWSURFACE);
		surf = backgroundSurf;
	}
}

void HostScreen::restoreBackground()
{
	if (backgroundSurf != NULL) {
		SDL_BlitSurface(backgroundSurf, NULL, mainSurface, NULL);
		surf = mainSurface;
		update(true);
		SDL_FreeSurface(backgroundSurf);
		backgroundSurf = NULL;
	}
}

void HostScreen::setWindowSize( uint32 width, uint32 height, uint32 bpp )
{
	this->width	 = width;
	this->height = height;
	this->bpp = bpp;

	// backup the pallete settings
	SDL_Color paletteBackup[256];
	for (int i = 0; i < 256; i++)
		paletteBackup[i] = palette.standard[i];

	// SelectVideoMode();
	sdl_videoparams = SDL_HWSURFACE | SDL_HWPALETTE;
	if (bx_options.video.fullscreen)
		sdl_videoparams |= SDL_FULLSCREEN;

	mainSurface = SDL_SetVideoMode(width, height, bpp, sdl_videoparams);
	surf = mainSurface;
	SDL_WM_SetCaption(VERSION_STRING, "ARAnyM");

	// restore the pallete settings
	for (int i = 0; i < 256; i++)
		setPaletteColor( i, paletteBackup[i].r, paletteBackup[i].g, paletteBackup[i].b );
	updatePalette( 256 );

	D(bug("Surface Pitch = %d, width = %d, height = %d", surf->pitch, surf->w, surf->h));
	D(bug("Must Lock? %s", SDL_MUSTLOCK(surf) ? "YES" : "NO"));

	// is the SDL_update needed?
	doUpdate = ( surf->flags & SDL_HWSURFACE ) == 0;

	renderBegin();

	VideoRAMBaseHost = (uint8 *) surf->pixels;
	InitVMEMBaseDiff(VideoRAMBaseHost, VideoRAMBase);
	D(bug("VideoRAM starts at %p (%08x)", VideoRAMBaseHost, VideoRAMBase));
	D(bug("surf->pixels = %x, getVideoSurface() = %x",
			VideoRAMBaseHost, SDL_GetVideoSurface()->pixels));

	renderEnd();

	D(bug("Pixel format:bitspp=%d, tmasks r=%04x g=%04x b=%04x"
			", tshifts r=%d g=%d b=%d"
			", tlosses r=%d g=%d b=%d",
			surf->format->BitsPerPixel,
			surf->format->Rmask, surf->format->Gmask, surf->format->Bmask,
			surf->format->Rshift, surf->format->Gshift, surf->format->Bshift,
			surf->format->Rloss, surf->format->Gloss, surf->format->Bloss));
}


extern "C" {
	static void getBinary( uint16 data, char *buffer ) {
		for( uint16 i=0; i<=15; i++ ) {
			buffer[i] = (data & 1)?'1':' ';
			data>>=1;
		}
		buffer[16]='\0';
	}
}



inline void HostScreen::putPixel( int16 x, int16 y, uint16 pattern, uint32 fgColor, uint32 bgColor, uint16 logOp )
{
	switch (logOp) {
		case 1:
			gfxFastPixelColorNolock( x, y, pattern ? fgColor : bgColor );
			break;
		case 2:
			if ( pattern )
				gfxFastPixelColorNolock( x, y, fgColor );
			break;
		case 3:
			if ( pattern )
				gfxFastPixelColorNolock( x, y, ~ gfxGetPixel( x, y ) );
			break;
		case 4:
			if ( ! pattern )
				gfxFastPixelColorNolock( x, y, fgColor );
			break;
	}
}


void HostScreen::gfxHLineColor ( int16 x1, int16 x2, int16 y, uint16 pattern, uint32 fgColor, uint32 bgColor, uint16 logOp )
{
	uint8 *pixel,*pixellast;
	int dx;
	int pixx, pixy;
	int16 w;
	int16 xtmp;
	uint8 ppos;

	/* Swap x1, x2 if required */
	if (x1>x2) {
		xtmp=x1; x1=x2; x2=xtmp;
	}

	/* Calculate width */
	w=x2-x1;

	/* Sanity check on width */
	if (w<0)
		return;

	/* More variable setup */
	dx=w+1;
	pixx = surf->format->BytesPerPixel;
	pixy = surf->pitch;
	pixel = ((uint8*)surf->pixels) + pixx * (int)x1 + pixy * (int)y;
	ppos = 0;

	D(bug("HLn %3d,%3d,%3d", x1, x2, y));

	/* Draw */
	switch(surf->format->BytesPerPixel) {
		case 1:
			pixellast = pixel + dx;
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixx)
						*(uint8*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint8*)pixel = fgColor; // STanda
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint8*)pixel = ~(*(uint8*)pixel);
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint8*)pixel = fgColor; // STanda
					break;
			}
			break;
		case 2:
			pixellast = pixel + dx + dx;
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixx)
						*(uint16*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint16*)pixel = fgColor; // STanda
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint16*)pixel = ~(*(uint16*)pixel);
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint16*)pixel = fgColor; // STanda
					break;
			}
			break;
		case 3:
			pixellast = pixel + dx + dx + dx;
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixx)
						putBpp24Pixel( pixel, (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor ) );
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							putBpp24Pixel( pixel, fgColor );
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							putBpp24Pixel( pixel, ~ getBpp24Pixel( pixel ) );
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							putBpp24Pixel( pixel, fgColor );
					break;
			}
			break;
		default: /* case 4*/
			dx = dx + dx;
			pixellast = pixel + dx + dx;
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixx)
						*(uint32*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint32*)pixel = fgColor; // STanda
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint32*)pixel = ~(*(uint32*)pixel);
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixx)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint32*)pixel = fgColor; // STanda
					break;
			}
			break;
	}
}


void HostScreen::gfxVLineColor( int16 x, int16 y1, int16 y2,
								uint16 pattern, uint32 fgColor, uint32 bgColor, uint16 logOp )
{
	uint8 *pixel, *pixellast;
	int dy;
	int pixx, pixy;
	int16 h;
	int16 ytmp;
	uint8 ppos;

	/* Swap y1, y2 if required */
	if (y1>y2) {
		ytmp=y1; y1=y2; y2=ytmp;
	}

	/* Calculate height */
	h=y2-y1;

	/* Sanity check on height */
	if (h<0)
		return;

	ppos = 0;

	D(bug("VLn %3d,%3d,%3d", x, y1, y2));

	/* More variable setup */
	dy=h+1;
	pixx = surf->format->BytesPerPixel;
	pixy = surf->pitch;
	pixel = ((uint8*)surf->pixels) + pixx * (int)x + pixy * (int)y1;
	pixellast = pixel + pixy*dy;

	/* Draw */
	switch(surf->format->BytesPerPixel) {
		case 1:
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixy)
						*(uint8*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint8*)pixel = fgColor; // STanda
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint8*)pixel = ~(*(uint8*)pixel);
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint8*)pixel = fgColor; // STanda
					break;
			}
			break;
		case 2:
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixy)
						*(uint16*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint16*)pixel = fgColor; // STanda
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint16*)pixel = ~(*(uint16*)pixel);
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint16*)pixel = fgColor; // STanda
					break;
			}
			break;
		case 3:
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixy)
						putBpp24Pixel( pixel, (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor ) );
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							putBpp24Pixel( pixel, fgColor );
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							putBpp24Pixel( pixel, ~ getBpp24Pixel( pixel ) );
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							putBpp24Pixel( pixel, fgColor );
					break;
			}
			break;
		default: /* case 4*/
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixy)
						*(uint32*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint32*)pixel = fgColor; // STanda
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint32*)pixel = ~(*(uint32*)pixel);
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixy)
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint32*)pixel = fgColor; // STanda
					break;
			}
			break;
	}
}


/* ----- Line */

/* Non-alpha line drawing code adapted from routine			 */
/* by Pete Shinners, pete@shinners.org						 */
/* Originally from pygame, http://pygame.seul.org			 */

void HostScreen::gfxLineColor( int16 x1, int16 y1, int16 x2, int16 y2,
                               uint16 pattern, uint32 fgColor, uint32 bgColor, uint16 logOp,
                               bool last_pixel )
{
	int16 pixx, pixy;
	int16 x, y;
	int16 dx, dy;
	int16 sx, sy;
	int16 swaptmp;
	uint8 *pixel;
	uint8 ppos;

	/* Test for special cases of straight lines or single point */
	if (x1 == x2) {
		if (y1 < y2) {
			gfxVLineColor(x1, y1, y2 - !last_pixel, pattern, fgColor, bgColor, logOp);
			return;
		} else if (y1 > y2) {
			gfxVLineColor(x1, y2 + !last_pixel, y1, pattern, fgColor, bgColor, logOp);
			return;
		} else if (last_pixel) {
			putPixel( x1, y1, pattern & 0x8000, fgColor, bgColor, logOp );
		}
	}
	if (y1 == y2) {
		if (x1 < x2) {
			gfxHLineColor(x1, x2 - !last_pixel, y1, pattern, fgColor, bgColor, logOp);
			return;
		} else if (x1 > x2) {
			gfxHLineColor(x2 + !last_pixel, x1, y1, pattern, fgColor, bgColor, logOp);
			return;
		}
	}

	D(bug("CLn %3d,%3d,%3d,%3d", x1, x2, y1, y2));

	/* Variable setup */
	dx = x2 - x1;
	dy = y2 - y1;
	sx = (dx >= 0) ? 1 : -1;
	sy = (dy >= 0) ? 1 : -1;
	ppos = 0;

	/* More variable setup */
	dx = sx * dx + 1;
	dy = sy * dy + 1;
	pixx = surf->format->BytesPerPixel;
	pixy = surf->pitch;
	pixel = ((uint8*)surf->pixels) + pixx * (uint32)x1 + pixy * (uint32)y1;
	pixx *= sx;
	pixy *= sy;
	if (dx < dy) {
		swaptmp = dx; dx = dy; dy = swaptmp;
		swaptmp = pixx; pixx = pixy; pixy = swaptmp;
	}

	//	D2(bug("ln pix pixx, pixy: %d,%d : %d,%d : %x, %d", sx, sy, dx, dy, pixx, pixy));

	/* Draw */
	x = !last_pixel;	// 0 if last pixel should be drawn, else 1
	y = 0;
	switch(surf->format->BytesPerPixel) {
		case 1:
			switch (logOp) {
				case 1:
					for (; x < dx; x++, pixel += pixx) {
						*(uint8*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 2:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint8*)pixel = fgColor; // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 3:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint8*)pixel = ~(*(uint8*)pixel);

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 4:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint8*)pixel = fgColor; // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
			}
			break;
		case 2:
			switch (logOp) {
				case 1:
					for (; x < dx; x++, pixel += pixx) {
						*(uint16*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 2:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint16*)pixel = fgColor; // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 3:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint16*)pixel = ~(*(uint16*)pixel);

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 4:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint16*)pixel = fgColor; // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
			}
			break;
		case 3:
			switch (logOp) {
				case 1:
					for (; x < dx; x++, pixel += pixx) {
						putBpp24Pixel( pixel, (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor) ); // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 2:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							putBpp24Pixel( pixel, fgColor );

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 3:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							putBpp24Pixel( pixel, ~ getBpp24Pixel( pixel ) );

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 4:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							putBpp24Pixel( pixel, fgColor );

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
			}
			break;
		default: /* case 4 */
			switch (logOp) {
				case 1:
					for (; x < dx; x++, pixel += pixx) {
						*(uint32*)pixel = (( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 2:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint32*)pixel = fgColor; // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 3:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) != 0 )
							*(uint32*)pixel = ~(*(uint32*)pixel);

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
				case 4:
					for (; x < dx; x++, pixel += pixx) {
						if ( ( pattern & ( 1 << ( (ppos++) & 0xf ) )) == 0 )
							*(uint32*)pixel = fgColor; // STanda

						y += dy;
						if (y >= dx) {
							y -= dx;
							pixel += pixy;
						}
					}
					break;
			}
			break;
	}
}




/**
 * Derived from the SDL_gfxPrimitives::boxColor(). The colors are in the destination surface format here.
 * The trivial cases optimalization removed.
 *
 * @author STanda
 **/
void HostScreen::gfxBoxColorPattern (int16 x, int16 y, int16 w, int16 h,
									 uint16 *areaPattern, uint32 fgColor, uint32 bgColor, uint16 logOp)
{
	uint8 *pixel, *pixellast;
	int16 pixx, pixy;
	int16 i;
	int16 dx=w;
	int16 dy=h;

	/* More variable setup */
	pixx = surf->format->BytesPerPixel;
	pixy = surf->pitch;
	pixel = ((uint8*)surf->pixels) + pixx * (int32)x + pixy * (int32)y;
	pixellast = pixel + pixy*dy;

	// STanda // FIXME here the pattern should be checked out of the loops for performance
			  // but for now it is good enough (if there is no pattern -> another switch?)

	/* Draw */
	switch(surf->format->BytesPerPixel) {
		case 1:
			pixy -= (pixx*dx);
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							*(uint8*)pixel = (( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
							pixel += pixx;
						}
					}
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 )
								*(uint8*)pixel = fgColor; // STanda
							pixel += pixx;
						}
					}
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 )
								*(uint8*)pixel = ~(*(uint8*)pixel);
							pixel += pixx;
						};
					}
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) == 0 )
								*(uint8*)pixel = fgColor; // STanda
							pixel += pixx;
						}
					}
					break;
			}
			break;
		case 2:
			pixy -= (pixx*dx);
			//				D2(bug("bix pix: %d, %x, %d", y, pixel, pixy));

			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							*(uint16*)pixel = (( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
							pixel += pixx;
						}
					}
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 )
								*(uint16*)pixel = fgColor; // STanda
							pixel += pixx;
						}
					}
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 )
								*(uint16*)pixel = ~(*(uint16*)pixel);
							pixel += pixx;
						};
					}
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) == 0 )
								*(uint16*)pixel = fgColor; // STanda
							pixel += pixx;
						}
					}
					break;
			}
			break;
		case 3:
			pixy -= (pixx*dx);
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							putBpp24Pixel( pixel, (( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 ) ? fgColor : bgColor) ); // STanda
							pixel += pixx;
						}
					}
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 )
								putBpp24Pixel( pixel, fgColor );
							pixel += pixx;
						}
					}
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 )
								putBpp24Pixel( pixel, ~ getBpp24Pixel( pixel ) );
							pixel += pixx;
						};
					}
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) == 0 )
								putBpp24Pixel( pixel, fgColor );
							pixel += pixx;
						}
					}
					break;
			}
			break;
		default: /* case 4*/
			pixy -= (pixx*dx);
			switch (logOp) {
				case 1:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							*(uint32*)pixel = (( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 ) ? fgColor : bgColor); // STanda
							pixel += pixx;
						}
					}
					break;
				case 2:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 )
								*(uint32*)pixel = fgColor; // STanda
							pixel += pixx;
						}
					}
					break;
				case 3:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) != 0 )
								*(uint32*)pixel = ~(*(uint32*)pixel);
							pixel += pixx;
						};
					}
					break;
				case 4:
					for (; pixel<pixellast; pixel += pixy) {
						uint16 pattern = areaPattern ? areaPattern[ y++ & 0xf ] : 0xffff; // STanda

						for (i=0; i<dx; i++) {
							if ( ( pattern & ( 1 << ( (x+i) & 0xf ) )) == 0 )
								*(uint32*)pixel = fgColor; // STanda
							pixel += pixx;
						}
					}
					break;
			}
			break;
	}  // switch
}


/*
 * $Log$
 * Revision 1.27  2002/06/09 20:08:31  joy
 * save_bkg/restore_bkg added (used in SDL GUI)
 * the save to mem worked for a while and then it started failing for no apparent reason so I had to write saving to file. It's a hack. Proper solution would be to create a complete surface by cloning the current one and blit the screen there, I think.
 *
 * Revision 1.26  2002/06/07 20:56:56  joy
 * added window/fullscreen mode switch
 *
 * Revision 1.25  2002/01/08 21:20:57  standa
 * fVDI driver palette store on res change implemented.
 *
 * Revision 1.24  2001/12/22 18:13:24  joy
 * most video related parameters moved to bx_options.video struct.
 * --refresh <x> added
 *
 * Revision 1.23  2001/12/03 20:56:07  standa
 * The gfsprimitives library files removed. All the staff was moved and
 * adjusted directly into the HostScreen class.
 *
 * Revision 1.22  2001/11/29 23:51:56  standa
 * Johan Klockars <rand@cd.chalmers.se> fVDI driver changes.
 *
 * Revision 1.21  2001/11/21 13:29:51  milan
 * cleanning & portability
 *
 * Revision 1.20  2001/11/19 01:37:35  standa
 * PaletteInversIndex search. Bugfix in fillArea in 8bit depth.
 *
 * Revision 1.19  2001/11/04 23:17:08  standa
 * 8bit destination surface support in VIDEL. Blit routine optimalization.
 * Bugfix in compatibility modes palette copying.
 *
 * Revision 1.18  2001/10/30 22:59:34  standa
 * The resolution change is now possible through the fVDI driver.
 *
 * Revision 1.17  2001/10/29 23:14:17  standa
 * The HostScreen support for arbitrary destination BPP (8,16,24,32bit).
 *
 * Revision 1.16  2001/10/25 19:56:01  standa
 * The Log and Header CVS tags in the Log removed. Was recursing.
 *
 * Revision 1.15  2001/10/24 17:55:01  standa
 * The fVDI driver fixes. Finishing the functionality tuning.
 *
 * Revision 1.14  2001/10/23 21:28:49  standa
 * Several changes, fixes and clean up. Shouldn't crash on high resolutions.
 * hostscreen/gfx... methods have fixed the loop upper boundary. The interface
 * types have changed quite havily.
 *
 * Revision 1.13  2001/10/16 19:06:55  standa
 * The uint32 changed to int16 to make the gfxLineColor work.
 * Now it seems not to segfault anywhere.
 *
 * Revision 1.12  2001/10/08 21:46:05  standa
 * The Header and Log CVS tags added.
 *
 * Revision 1.11  2001/10/03 06:37:41  standa
 * General cleanup. Some constants added. Better "to screen" operation
 * recognition (the videoram address is checked too - instead of only the
 * MFDB == NULL || MFDB->address == NULL)
 *
 * Revision 1.10  2001/09/30 23:09:23  standa
 * The line logical operation added.
 * The first version of blitArea (screen to screen only).
 *
 * Revision 1.9  2001/09/24 23:16:28  standa
 * Another minor changes. some logical operation now works.
 * fvdidrv/fillArea and fvdidrv/expandArea got the first logOp handling.
 *
 * Revision 1.8  2001/09/20 18:12:09  standa
 * Off by one bug fixed in fillArea.
 * Separate functions for transparent and opaque background.
 * gfxPrimitives methods moved to the HostScreen
 *
 * Revision 1.7  2001/09/05 15:06:41  joy
 * SelectVideoMode() commented out.
 *
 * Revision 1.6  2001/09/04 13:51:45  joy
 * debug disabled
 *
 * Revision 1.5  2001/08/30 14:04:59  standa
 * The fVDI driver. mouse_draw implemented. Partial pattern fill support.
 * Still buggy.
 *
 * Revision 1.4  2001/08/28 23:26:09  standa
 * The fVDI driver update.
 * VIDEL got the doRender flag with setter setRendering().
 *       The host_colors_uptodate variable name was changed to hostColorsSync.
 * HostScreen got the doUpdate flag cleared upon initialization if the HWSURFACE
 *       was created.
 * fVDIDriver got first version of drawLine and fillArea (thanks to SDL_gfxPrimitives).
 *
 * Revision 1.3  2001/08/13 22:29:06  milan
 * IDE's params from aranymrc file etc.
 *
 * Revision 1.2  2001/07/24 09:36:51  joy
 * D(bug) macro replaces fprintf
 *
 * Revision 1.1  2001/06/18 13:21:55  standa
 * Several template.cpp like comments were added.
 * HostScreen SDL encapsulation class.
 *
 *
 */
