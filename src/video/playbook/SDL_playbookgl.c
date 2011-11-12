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

#include "SDL_playbookgl_c.h"
#include "SDL_loadso.h"

static EGLDisplay egl_disp;
static EGLSurface egl_surf;
static EGLConfig egl_conf;
static EGLContext egl_ctx;
enum RENDERING_API {GL_ES_1 = EGL_OPENGL_ES_BIT, GL_ES_2 = EGL_OPENGL_ES2_BIT};

static void
egl_perror(const char *msg) {
    static const char *errmsg[] = {
        "function succeeded",
        "EGL is not initialized, or could not be initialized, for the specified display",
        "cannot access a requested resource",
        "failed to allocate resources for the requested operation",
        "an unrecognized attribute or attribute value was passed in an attribute list",
        "an EGLConfig argument does not name a valid EGLConfig",
        "an EGLContext argument does not name a valid EGLContext",
        "the current surface of the calling thread is no longer valid",
        "an EGLDisplay argument does not name a valid EGLDisplay",
        "arguments are inconsistent",
        "an EGLNativePixmapType argument does not refer to a valid native pixmap",
        "an EGLNativeWindowType argument does not refer to a valid native window",
        "one or more argument values are invalid",
        "an EGLSurface argument does not name a valid surface configured for rendering",
        "a power management event has occurred",
    };

    fprintf(stderr, "%s: %s\n", msg, errmsg[eglGetError() - EGL_SUCCESS]);
}

EGLConfig egl_choose_config(EGLDisplay egl_disp, enum RENDERING_API api) {
    EGLConfig egl_conf = (EGLConfig)0;
    EGLConfig *egl_configs;
    EGLint egl_num_configs;
    EGLint val;
    EGLBoolean rc;
    EGLint i;

    rc = eglGetConfigs(egl_disp, NULL, 0, &egl_num_configs);
    if (rc != EGL_TRUE) {
        return egl_conf;
    }
    if (egl_num_configs == 0) {
        fprintf(stderr, "eglGetConfigs: could not find a configuration\n");
        return egl_conf;
    }

    egl_configs = (EGLConfig *) malloc(egl_num_configs * sizeof(*egl_configs));
    if (egl_configs == NULL) {
        fprintf(stderr, "could not allocate memory for %d EGL configs\n", egl_num_configs);
        return egl_conf;
    }

    rc = eglGetConfigs(egl_disp, egl_configs,
        egl_num_configs, &egl_num_configs);
    if (rc != EGL_TRUE) {
        free(egl_configs);
        return egl_conf;
    }

    for (i = 0; i < egl_num_configs; i++) {
        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_SURFACE_TYPE, &val);
        if (!(val & EGL_WINDOW_BIT)) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_RENDERABLE_TYPE, &val);
        if (!(val & api)) {
        	continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_DEPTH_SIZE, &val);
        if ((api & (GL_ES_1|GL_ES_2)) && (val == 0)) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_RED_SIZE, &val);
        if (val != 8) {
            continue;
        }
        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_GREEN_SIZE, &val);
        if (val != 8) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_BLUE_SIZE, &val);
        if (val != 8) {
            continue;
        }

        eglGetConfigAttrib(egl_disp, egl_configs[i], EGL_BUFFER_SIZE, &val);
        if (val != 32) {
            continue;
        }

        egl_conf = egl_configs[i];
        break;
    }

    free(egl_configs);

    if (egl_conf == (EGLConfig)0) {
        fprintf(stderr, "egl_choose_config: could not find a matching configuration\n");
    }

    return egl_conf;
}

/* krat: adding OpenGL support */
int Playbook_GL_Init(_THIS)
{
#if SDL_VIDEO_OPENGL
	int rc;
	int api = GL_ES_1;
	/* load the gl driver from a default path */
	if ( ! this->gl_config.driver_loaded ) {
		/* no driver has been loaded, use default (ourselves) */
		if ( Playbook_GL_LoadLibrary(this, NULL) < 0 ) {
			return(-1);
		}
	}

    egl_disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_disp == EGL_NO_DISPLAY)
        return -1;

    rc = eglInitialize(egl_disp, NULL, NULL);
    if (rc != EGL_TRUE)
        return -1;

    rc = eglBindAPI(EGL_OPENGL_ES_API);
    if (rc != EGL_TRUE)
        return -1;

    egl_conf = egl_choose_config(egl_disp, api);
    if (egl_conf == (EGLConfig)0)
        return -1;

    egl_ctx = eglCreateContext(egl_disp, egl_conf, EGL_NO_CONTEXT, NULL);
    if (egl_ctx == EGL_NO_CONTEXT)
        return -1;
    
    egl_surf = eglCreateWindowSurface(egl_disp, egl_conf, this->hidden->screenWindow, NULL);
    if (egl_surf == EGL_NO_SURFACE) {
        return -1;
    }

	return(0);
#else
	SDL_SetError("OpenGL support not configured");
	return(-1);
#endif
}

void Playbook_GL_Quit(_THIS)
{
#if SDL_VIDEO_OPENGL
    if (egl_disp != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_surf != EGL_NO_SURFACE) {
            eglDestroySurface(egl_disp, egl_surf);
            egl_surf = EGL_NO_SURFACE;
        }
        if (egl_ctx != EGL_NO_CONTEXT) {
            eglDestroyContext(egl_disp, egl_ctx);
            egl_ctx = EGL_NO_CONTEXT;
        }
        eglTerminate(egl_disp);
        egl_disp = EGL_NO_DISPLAY;
    }
    eglReleaseThread();
#endif
}

#if SDL_VIDEO_OPENGL

/* Make the current context active */
int Playbook_GL_MakeCurrent(_THIS)
{
	int rc;
    rc = eglMakeCurrent(egl_disp, egl_surf, egl_surf, egl_ctx);
    if (rc != EGL_TRUE) {
        return -1;
    }

	EGLint interval = 1;
    rc = eglSwapInterval(egl_disp, interval);
    if (rc != EGL_TRUE) {
    	egl_perror("eglSwapInterval");
        return -1;
    }

	return 0;
}

void Playbook_GL_SwapBuffers(_THIS)
{
	eglSwapBuffers(egl_disp, egl_surf);
}

int Playbook_GL_LoadLibrary(_THIS, const char *location)
{
	if (location == NULL)
#ifdef USE_GLES_WRAPPER
		location = "libGLESwrapper.so";
#else
		location = "libGLESv1_CM.so";
#endif

	this->hidden->libraryHandle = SDL_LoadObject(location);

	this->gl_config.driver_loaded = 1;
	return (this->hidden->libraryHandle != NULL) ? 0 : -1;
}

void Playbook_GL_UnloadLibrary(_THIS)
{
	SDL_UnloadObject(this->hidden->libraryHandle);
	this->hidden->libraryHandle = NULL;
	this->gl_config.driver_loaded = 0;
}

void Playbook_GL_NoProcWarning() {
	printf("Warning: GL proc undefined!\n");
	flushall();
}

void* Playbook_GL_GetProcAddress(_THIS, const char *proc)
{
	void* adr = NULL;
#ifdef USE_GLES_WRAPPER
	size_t len = 1+SDL_strlen(proc)+1;
	char *xproc = SDL_stack_alloc(char, len);
	xproc[0] = 'x';
	SDL_strlcpy(&xproc[1], proc, len);
	adr = SDL_LoadFunction( this->hidden->libraryHandle, xproc );
	if (adr == NULL) {
		SDL_ClearError();
		adr = SDL_LoadFunction( this->hidden->libraryHandle, proc );
	}
	SDL_stack_free(xproc);
#else
	adr = SDL_LoadFunction( this->hidden->libraryHandle, proc );
	if (adr == NULL) {
		SDL_ClearError();
		adr = *Playbook_GL_NoProcWarning;
	}
#endif
	return adr;
}

#endif /* SDL_VIDEO_OPENGL */

