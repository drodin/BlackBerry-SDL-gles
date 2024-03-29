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

#ifndef _SDL_playbookvideo_h
#define _SDL_playbookvideo_h

#include "../SDL_sysvideo.h"
#include <math.h>
#include <screen/screen.h>
#include <bps/bps.h>
#include <bps/navigator.h>
#include <bps/paymentservice.h>
#include <bps/sensor.h>
#include <bps/screen.h>

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_VideoDevice *this
#define _priv   this->hidden

#define SDL_NUMMODES 1

/* Private display data */

struct SDL_PrivateVideoData {
    int w, h;
    void *buffer;
    screen_context_t screenContext;
    screen_event_t screenEvent;
    screen_window_t screenWindow;
    char* windowGroup;
    screen_buffer_t frontBuffer;
    SDL_Surface *surface;
    void* pixels;
    int pitch;
    int screenResolution[2];

    SDL_Rect *SDL_modelist[SDL_NUMMODES+1];

#if SDL_VIDEO_OPENGL
	void *libraryHandle;
#endif
};

#endif /* _SDL_playbookvideo_h */
