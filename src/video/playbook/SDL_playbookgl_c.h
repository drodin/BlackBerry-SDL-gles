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

/* AGL implementation of SDL OpenGL support */

#include "SDL_config.h"

#if SDL_VIDEO_OPENGL
#include "SDL_opengl.h"
#endif /* SDL_VIDEO_OPENGL */

#include <EGL/egl.h>
#include <screen/screen.h>

#include "SDL_playbookvideo.h"

/* OpenGL functions */
extern int Playbook_GL_Init(_THIS);
extern void Playbook_GL_Quit(_THIS);
#if SDL_VIDEO_OPENGL
extern int Playbook_GL_MakeCurrent(_THIS);
extern void Playbook_GL_SwapBuffers(_THIS);
extern int Playbook_GL_LoadLibrary(_THIS, const char *location);
extern void Playbook_GL_UnloadLibrary(_THIS);
extern void* Playbook_GL_GetProcAddress(_THIS, const char *proc);
#endif

