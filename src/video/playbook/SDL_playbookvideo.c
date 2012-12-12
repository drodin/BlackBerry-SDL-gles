/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2009 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/* Dummy SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "PLAYBOOK" by Sam Lantinga.
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_syswm.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_playbookvideo.h"
#include "SDL_playbookevents_c.h"
#include "SDL_playbookyuv_c.h"

#include "SDL_playbookgl_c.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>

#define PLAYBOOKVID_DRIVER_NAME "playbook"

/* Initialization/Query functions */
static int PLAYBOOK_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **PLAYBOOK_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *PLAYBOOK_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int PLAYBOOK_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void PLAYBOOK_VideoQuit(_THIS);

/* Hardware surface functions */
static int PLAYBOOK_AllocHWSurface(_THIS, SDL_Surface *surface);
static int PLAYBOOK_LockHWSurface(_THIS, SDL_Surface *surface);
static void PLAYBOOK_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void PLAYBOOK_FreeHWSurface(_THIS, SDL_Surface *surface);
static int PLAYBOOK_FlipHWSurface(_THIS, SDL_Surface *surface);

static int PLAYBOOK_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *rect, Uint32 color);

/* etc. */
static void PLAYBOOK_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

static int PLAYBOOK_GetWMInfo(_THIS, SDL_SysWMinfo *info);

/* get pid as window group id */
char *
get_window_group_id()
{
    static char s_window_group_id[16] = "";

    if (s_window_group_id[0] == '\0') {
        snprintf(s_window_group_id, sizeof(s_window_group_id), "%d", getpid());
    }

    return s_window_group_id;
}

/* PLAYBOOK driver bootstrap functions */

static int PLAYBOOK_Available(void)
{
	return 1;
}

static void PLAYBOOK_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_VideoDevice *PLAYBOOK_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		SDL_memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			SDL_free(device);
		}
		return(0);
	}
	SDL_memset(device->hidden, 0, (sizeof *device->hidden));

	/* Set the function pointers */
	device->VideoInit = PLAYBOOK_VideoInit;
	device->ListModes = PLAYBOOK_ListModes;
	device->SetVideoMode = PLAYBOOK_SetVideoMode;
	device->CreateYUVOverlay = PLAYBOOK_CreateYUVOverlay;
	device->SetColors = PLAYBOOK_SetColors;
	device->UpdateRects = PLAYBOOK_UpdateRects;
	device->VideoQuit = PLAYBOOK_VideoQuit;
	device->AllocHWSurface = PLAYBOOK_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = PLAYBOOK_FillHWRect;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = PLAYBOOK_LockHWSurface;
	device->UnlockHWSurface = PLAYBOOK_UnlockHWSurface;
	device->FlipHWSurface = PLAYBOOK_FlipHWSurface;
	device->FreeHWSurface = PLAYBOOK_FreeHWSurface;
#if SDL_VIDEO_OPENGL
	device->GL_MakeCurrent = Playbook_GL_MakeCurrent;
	device->GL_SwapBuffers = Playbook_GL_SwapBuffers;
	device->GL_LoadLibrary = Playbook_GL_LoadLibrary;
	device->GL_GetProcAddress = Playbook_GL_GetProcAddress;
#endif	/* Have OpenGL */
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = PLAYBOOK_GetWMInfo;
	device->InitOSKeymap = PLAYBOOK_InitOSKeymap;
	device->PumpEvents = PLAYBOOK_PumpEvents;

	device->free = PLAYBOOK_DeleteDevice;

	return device;
}

VideoBootStrap PLAYBOOK_bootstrap = {
	PLAYBOOKVID_DRIVER_NAME, "SDL PlayBook (libscreen) video driver",
	PLAYBOOK_Available, PLAYBOOK_CreateDevice
};

int PLAYBOOK_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	int i, rc;

	rc = screen_create_context(&_priv->screenContext, 0);
	if (rc) {
		SDL_SetError("Cannot create screen context: %s", strerror(errno));
		return -1;
	}

    rc = bps_initialize();
	if (rc) {
		SDL_SetError("Cannot initializes the BPS library: %s", strerror(errno));
		screen_destroy_context(_priv->screenContext);
		return -1;
	}

    rc = navigator_request_events(0);
	if (rc) {
		SDL_SetError("Cannot request navigator events: %s", strerror(errno));
		bps_shutdown();
		screen_destroy_context(_priv->screenContext);
		return -1;
	}

    rc = navigator_rotation_lock(true);
	if (rc) {
		SDL_SetError("Cannot set navigator rotation lock: %s", strerror(errno));
		bps_shutdown();
		screen_destroy_context(_priv->screenContext);
		return -1;
	}

    paymentservice_request_events(0);
#ifdef PAYMENT_LOCAL
    paymentservice_set_connection_mode(true);
#endif

    if (sensor_is_supported(SENSOR_TYPE_AZIMUTH_PITCH_ROLL)) {
        sensor_set_rate(SENSOR_TYPE_AZIMUTH_PITCH_ROLL, 25000);
        sensor_set_skip_duplicates(SENSOR_TYPE_AZIMUTH_PITCH_ROLL, true);
        sensor_request_events(SENSOR_TYPE_AZIMUTH_PITCH_ROLL);
    }

	_priv->screenWindow = 0;
	_priv->surface = 0;

	for ( i=0; i<SDL_NUMMODES; ++i ) {
		_priv->SDL_modelist[i] = SDL_malloc(sizeof(SDL_Rect));
		_priv->SDL_modelist[i]->x = _priv->SDL_modelist[i]->y = 0;
	}

	_priv->screenResolution[0] = atoi(getenv("WIDTH"));
	_priv->screenResolution[1] = atoi(getenv("HEIGHT"));
	/* Modes sorted largest to smallest */
	_priv->SDL_modelist[0]->w = _priv->screenResolution[0]; _priv->SDL_modelist[0]->h = _priv->screenResolution[1];
	_priv->SDL_modelist[1] = NULL;

	/* Determine the screen depth (use default 32-bit depth) */
	vformat->BitsPerPixel = 32;
	vformat->BytesPerPixel = 4;

	/* Hardware surfaces are not available */
	this->info.hw_available = 1;
	this->info.blit_fill = 1;
	this->info.blit_hw = 0;
	this->info.blit_hw_A = 0;
	this->info.blit_hw_CC = 0;

	/* There is no window manager to talk to */
	this->info.wm_available = 0;

	/* Full screen size */
	this->info.current_w = _priv->screenResolution[0];
	this->info.current_h = _priv->screenResolution[1];

	/* We're done! */
	return(0);
}

SDL_Rect **PLAYBOOK_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	if (flags & SDL_FULLSCREEN ) {
		return _priv->SDL_modelist;
	} else {
		return (SDL_Rect**)-1; // We only support full-screen video modes.
	}
}

struct private_hwdata {
	screen_pixmap_t pixmap;
	screen_window_t window;
	screen_buffer_t front;
	screen_buffer_t back;
};

SDL_Surface *PLAYBOOK_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	int usage;
	screen_window_t screenWindow;
	int rc = 0;
	fprintf(stderr, "SetVideoMode: %dx%d %dbpp\n", width, height, bpp);
	if (!_priv->screenWindow) {
		rc = screen_create_window(&screenWindow, _priv->screenContext);
		if (rc) {
			SDL_SetError("Cannot create window: %s", strerror(errno));
			return NULL;
		}
	} else {
		if (current->hwdata)
			SDL_free(current->hwdata);
		screen_destroy_window_buffers(_priv->screenWindow);
		screenWindow = _priv->screenWindow;
	}

	int position[2] = {0, 0};
	rc = screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_POSITION, position);
	if (rc) {
		SDL_SetError("Cannot position window: %s", strerror(errno));
		screen_destroy_window(screenWindow);
		return NULL;
	}

	int idle_mode = SCREEN_IDLE_MODE_KEEP_AWAKE; // TODO: Handle idle gracefully?
	rc = screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_IDLE_MODE, &idle_mode);
	if (rc) {
		SDL_SetError("Cannot disable idle mode: %s", strerror(errno));
		screen_destroy_window(screenWindow);
		return NULL;
	}

	rc = screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_SIZE, _priv->screenResolution);
	if (rc) {
		SDL_SetError("Cannot resize window: %s", strerror(errno));
		screen_destroy_window(screenWindow);
		return NULL;
	}

	rc = screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_BUFFER_SIZE, _priv->screenResolution);
	if (rc) {
		SDL_SetError("Cannot resize window buffer: %s", strerror(errno));
		screen_destroy_window(screenWindow);
		return NULL;
	}

	int format = 0;
	switch (bpp) {
	case 16:
		SDL_ReallocFormat(current, 16, 0x0000f800, 0x0000007e0, 0x0000001f, 0);
		format = SCREEN_FORMAT_RGB565;
		break;
	case 32:
		SDL_ReallocFormat(current, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0);
		format = SCREEN_FORMAT_RGBX8888;
		break;
	}
	rc = screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_FORMAT, &format);
	if (rc) {
		SDL_SetError("Cannot set window format: %s", strerror(errno));
		screen_destroy_window(screenWindow);
		return NULL;
	}

	if (flags & SDL_OPENGL)
		usage = SCREEN_USAGE_OPENGL_ES1 | SCREEN_USAGE_READ;
	else
		usage = SCREEN_USAGE_NATIVE | SCREEN_USAGE_READ | SCREEN_USAGE_WRITE;
	rc = screen_set_window_property_iv(screenWindow, SCREEN_PROPERTY_USAGE, &usage);
	if (rc) {
		SDL_SetError("Cannot set window usage: %s", strerror(errno));
		screen_destroy_window(screenWindow);
		return NULL;
	}

	int bufferCount = 1; // FIXME: (flags & SDL_DOUBLEBUF)?2:1; - Currently double-buffered surfaces are slow!
	rc = screen_create_window_buffers(screenWindow, bufferCount);
	if (rc) {
		SDL_SetError("Cannot create window buffer: %s", strerror(errno));
		return NULL;
	}

	_priv->windowGroup = get_window_group_id();
	rc = screen_create_window_group(screenWindow, _priv->windowGroup);
	if (rc) {
		SDL_SetError("Cannot create window group: %s", strerror(errno));
		return NULL;
	}

	_priv->screenWindow = screenWindow;

	if ( flags & SDL_OPENGL ) {
		if ( Playbook_GL_Init(this) == 0 ) {
			current->flags |= SDL_OPENGL;
		} else {
			current = NULL;
		}
	}

	screen_buffer_t windowBuffer[bufferCount];
	rc = screen_get_window_property_pv(screenWindow,
			SCREEN_PROPERTY_RENDER_BUFFERS, (void**)&windowBuffer);
	if (rc) {
		SDL_SetError("Cannot get window render buffers: %s", strerror(errno));
		return NULL;
	}

	rc = screen_get_buffer_property_pv(windowBuffer[0], SCREEN_PROPERTY_POINTER, &_priv->pixels);
	if (rc) {
		SDL_SetError("Cannot get buffer pointer: %s", strerror(errno));
		return NULL;
	}

	rc = screen_get_buffer_property_iv(windowBuffer[0], SCREEN_PROPERTY_STRIDE, &_priv->pitch);
	if (rc) {
		SDL_SetError("Cannot get stride: %s", strerror(errno));
		return NULL;
	}

	_priv->frontBuffer = windowBuffer[0];

	screen_request_events(_priv->screenContext);

	current->hwdata = SDL_malloc(sizeof(struct private_hwdata));
	current->hwdata->pixmap = 0;
	current->hwdata->window = screenWindow;
	current->hwdata->front = windowBuffer[0];
	if (bufferCount > 1) {
		current->flags |= SDL_DOUBLEBUF;
		current->hwdata->back = windowBuffer[1];
	} else {
		current->hwdata->back = 0;
	}
	current->flags &= ~SDL_RESIZABLE; /* no resize for Direct Context */
	current->flags |= SDL_FULLSCREEN;
	current->flags |= SDL_HWSURFACE;
	current->flags |= SDL_PREALLOC;
	current->w = width;
	current->h = height;
	current->pitch = _priv->pitch;
	current->pixels = _priv->pixels;
	_priv->surface = current;
	return current;
}

static int PLAYBOOK_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	if (surface->hwdata != NULL) {
		fprintf(stderr, "Surface already has hwdata\n");
		return -1;
	}

	surface->hwdata = SDL_malloc(sizeof(struct private_hwdata));
	if (surface->hwdata == NULL) {
		SDL_OutOfMemory();
		return -1;
	}

	int rc = screen_create_pixmap( &surface->hwdata->pixmap, _priv->screenContext);
	if (rc) {
		fprintf(stderr, "Failed to create HW surface: screen_create_pixmap returned %s\n", strerror(errno));
		goto fail1;
	}

	int size[2] = {surface->w, surface->h};
	rc = screen_set_pixmap_property_iv(surface->hwdata->pixmap, SCREEN_PROPERTY_BUFFER_SIZE, size);
	if (rc) {
		fprintf(stderr, "Failed to set SCREEN_PROPERTY_BUFFER_SIZE: screen_set_pixmap_property_iv returned %s\n", strerror(errno));
		goto fail1;
	}

	int format = SCREEN_FORMAT_RGBA8888;
	rc = screen_set_pixmap_property_iv(surface->hwdata->pixmap, SCREEN_PROPERTY_FORMAT, &format);
	if (rc) {
		fprintf(stderr, "Failed to set SCREEN_PROPERTY_FORMAT: screen_set_pixmap_property_iv returned %s\n", strerror(errno));
		goto fail1;
	}

	rc = screen_create_pixmap_buffer(surface->hwdata->pixmap);
	if (rc) {
		fprintf(stderr, "Failed to allocate HW surface: screen_create_pixmap_buffer returned %s\n", strerror(errno));
		goto fail2;
	}

	surface->flags |= SDL_HWSURFACE;
	surface->flags |= SDL_PREALLOC;

	return 0;

fail2:
	screen_destroy_pixmap(surface->hwdata->pixmap);
fail1:
	SDL_free(surface->hwdata);
	surface->hwdata = 0;

	return -1;
}
static void PLAYBOOK_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	if (surface->hwdata) {
		screen_destroy_pixmap_buffer(surface->hwdata->pixmap);
		screen_destroy_pixmap(surface->hwdata->pixmap);
	}
	return;
}

static int PLAYBOOK_LockHWSurface(_THIS, SDL_Surface *surface)
{
	/* Currently does nothing */
	return(0);
}

static void PLAYBOOK_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	/* Currently does nothing */
	return;
}

static int PLAYBOOK_FlipHWSurface(_THIS, SDL_Surface *surface)
{
	// FIXME: This doesn't work properly yet. It flashes black, I think the new render buffers are wrong.
	static int fullRect[] = {0, 0, 0, 0};
	fullRect[2] = _priv->screenResolution[0];
	fullRect[3] = _priv->screenResolution[1];

	//screen_flush_blits(_priv->screenContext, 0);
	int result = screen_post_window(_priv->screenWindow, surface->hwdata->front, 1, fullRect, 0);

	screen_buffer_t windowBuffer[2];
	int rc = screen_get_window_property_pv(_priv->screenWindow,
			SCREEN_PROPERTY_RENDER_BUFFERS, (void**)&windowBuffer);
	if (rc) {
		SDL_SetError("Cannot get window render buffers: %s", strerror(errno));
		return NULL;
	}

	rc = screen_get_buffer_property_pv(windowBuffer[0], SCREEN_PROPERTY_POINTER, &_priv->pixels);
	if (rc) {
		SDL_SetError("Cannot get buffer pointer: %s", strerror(errno));
		return NULL;
	}
	surface->hwdata->front = windowBuffer[0];
	surface->pixels = _priv->pixels;
	return 0;
}

static int PLAYBOOK_FillHWRect(_THIS, SDL_Surface *dst, SDL_Rect *rect, Uint32 color)
{
	if (dst->flags & SDL_HWSURFACE) {
		int attribs[] = {SCREEN_BLIT_DESTINATION_X, rect->x,
						SCREEN_BLIT_DESTINATION_Y, rect->y,
						SCREEN_BLIT_DESTINATION_WIDTH, rect->w,
						SCREEN_BLIT_DESTINATION_HEIGHT, rect->h,
						SCREEN_BLIT_COLOR, color,
						SCREEN_BLIT_END};
		screen_fill(_priv->screenContext, _priv->frontBuffer, attribs);
	}
	return 0;
}

static void PLAYBOOK_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{
	static int dirtyRects[256*4];
	int index = 0, i = 0;
	for (i=0; i<numrects; i++) {
		dirtyRects[index] = rects[i].x;
		dirtyRects[index+1] = rects[i].y;
		dirtyRects[index+2] = rects[i].x + rects[i].w;
		dirtyRects[index+3] = rects[i].y + rects[i].h;
		index += 4;
	}
	screen_post_window(_priv->screenWindow, _priv->frontBuffer, numrects, dirtyRects, 0);
#if 0
	static int dirtyRects[256*4];
	int index = 0, i = 0;
	int y = 0, x = 0, ptr = 0, srcPtr = 0;
	unsigned char* pixels = (unsigned char*)_priv->pixels;
	SDL_Surface* src = _priv->surface;
	if (!_priv->surface) {
		if (_priv->screenWindow && _priv->frontBuffer) {
			memset(pixels, 0, _priv->pitch * 600);
			screen_post_window(_priv->screenWindow, _priv->frontBuffer, numrects, dirtyRects, 0);
		}
		return;
	}
	unsigned char* srcPixels = (unsigned char*)src->pixels;
	if (!srcPixels) {
		fprintf(stderr, "Can't handle palette yet\n");
		return; // FIXME: Handle palette
	}

	int rc = screen_get_buffer_property_pv(_priv->frontBuffer, SCREEN_PROPERTY_POINTER, &_priv->pixels);
	if (rc) {
		fprintf(stderr, "Cannot get buffer pointer: %s\n", strerror(errno));
		return;
	}

	rc = screen_get_buffer_property_iv(_priv->frontBuffer, SCREEN_PROPERTY_STRIDE, &_priv->pitch);
	if (rc) {
		fprintf(stderr, "Cannot get stride: %s\n", strerror(errno));
		return;
	}

	// FIXME: Bounds, sanity checking, resizing
	// TODO: Use screen_blit?
	for (i=0; i<numrects; i++) {
		dirtyRects[index] = rects[i].x;
		dirtyRects[index+1] = rects[i].y;
		dirtyRects[index+2] = rects[i].x + rects[i].w;
		dirtyRects[index+3] = rects[i].y + rects[i].h;

		for (y = dirtyRects[index+1]; y<dirtyRects[index+3]; y++) {
			for (x = dirtyRects[index]; x < dirtyRects[index+2]; x++) {
				ptr = y * _priv->pitch + x * 4;
				srcPtr = y * src->pitch + x * src->format->BytesPerPixel;
				pixels[ptr] = srcPixels[srcPtr];
				pixels[ptr+1] = srcPixels[srcPtr+1];
				pixels[ptr+2] = srcPixels[srcPtr+2];
				pixels[ptr+3] = 0xff;
			}
		}
		index += 4;
	}

	if (_priv->screenWindow && _priv->frontBuffer)
		screen_post_window(_priv->screenWindow, _priv->frontBuffer, numrects, dirtyRects, 0);
#endif
}

int PLAYBOOK_GetWMInfo(_THIS, SDL_SysWMinfo *info)
{
        SDL_VERSION(&(info->version));
        info->window = _priv->screenWindow;
        info->group = _priv->windowGroup;

        return 1;
}

int PLAYBOOK_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	/* do nothing of note. */
	// FIXME: Do we need to handle palette?
	return(1);
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
// FIXME: Fix up cleanup process
void PLAYBOOK_VideoQuit(_THIS)
{
//	if (_priv->buffer) {
//		SDL_free(_priv->buffer);
//		_priv->buffer = 0;
//	}
	Playbook_GL_Quit(this);
	if (_priv->screenWindow) {
		screen_destroy_window_buffers(_priv->screenWindow);
		screen_destroy_window(_priv->screenWindow);
	}
	screen_stop_events(_priv->screenContext);
	bps_shutdown();
	screen_destroy_context(_priv->screenContext);
}
