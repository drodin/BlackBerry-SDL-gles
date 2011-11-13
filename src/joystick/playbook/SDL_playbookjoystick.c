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

#if defined(SDL_JOYSTICK_PLAYBOOK)

#include <bps/accelerometer.h>
#include <bps/orientation.h>

/* This is the system specific header for the SDL joystick API */

#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#define AXIS_MAX 32767
#define JDELTA 0.005f

static int joystickReset = 0;

orientation_direction_t direction;
int orientation_angle;

/* Function to scan the system for joysticks.
 * This function should set SDL_numjoysticks to the number of available
 * joysticks.  Joystick 0 should be the system default joystick.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int SDL_SYS_JoystickInit(void)
{
    orientation_get(&direction, &orientation_angle);

    accelerometer_set_update_frequency(FREQ_40_HZ);

	SDL_numjoysticks = 1;
	return(SDL_numjoysticks);
}

/* Function to get the device-dependent name of a joystick */
const char *SDL_SYS_JoystickName(int index)
{
	return("accelerometer");
}

/* Function to open a joystick for use.
   The joystick to open is specified by the index field of the joystick.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int SDL_SYS_JoystickOpen(SDL_Joystick *joystick)
{
	joystick->nbuttons = 0;
	joystick->naxes = 2;
	return(0);
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void SDL_SYS_JoystickUpdate(SDL_Joystick *joystick)
{
	static double next_jx, next_jy, prev_jx, prev_jy;

	if (joystickReset) {
		next_jx = 0.0f;
		next_jy = 0.0f;
		prev_jx = 0.0f;
		prev_jy = 0.0f;
		joystickReset = 0;
	}

	double x, y, z;
	accelerometer_read_forces(&x, &y, &z);

	double roll = ACCELEROMETER_CALCULATE_ROLL(x, y, z)/90;
	double pitch = ACCELEROMETER_CALCULATE_PITCH(x, y, z)/90;

	//Account for axis change due to different starting orientations
    if (orientation_angle == 180) {
        next_jx = -roll;
    	next_jy = -pitch;
    } else {
        next_jx = roll;
        next_jy = pitch;
    }

//#define JOYKEYB_SIMULATE 1
#ifdef JOYKEYB_SIMULATE
	static SDL_keysym last_xkeysym;
	static SDL_keysym last_ykeysym;
	const double jkdelta = 0.02f;
	const unsigned jkxfixed = 1;
	const unsigned jkyfixed = 0;

	SDL_keysym xkeysym;
	if (jkxfixed && (next_jx > jkdelta))
		xkeysym.sym = SDLK_RIGHT;
	else if (jkxfixed && (next_jx < -jkdelta))
		xkeysym.sym = SDLK_LEFT;
	else if (!jkxfixed && (next_jx > jkdelta + prev_jx))
		xkeysym.sym = SDLK_RIGHT;
	else if (!jkxfixed && (next_jx < -jkdelta + prev_jx))
		xkeysym.sym = SDLK_LEFT;
	else
		xkeysym.sym = 0;

	if (last_xkeysym.sym && last_xkeysym.sym != xkeysym.sym) {
		SDL_PrivateKeyboard(SDL_RELEASED, &last_xkeysym);
		last_xkeysym.sym = 0;
	}

	if (xkeysym.sym) {
		SDL_PrivateKeyboard(SDL_PRESSED, &xkeysym);
		last_xkeysym = xkeysym;
	}

	SDL_keysym ykeysym;
	if (jkyfixed && (next_jy > jkdelta))
		ykeysym.sym = SDLK_DOWN;
	else if (jkyfixed && (next_jy < -jkdelta))
		ykeysym.sym = SDLK_UP;
	else if (!jkyfixed && (next_jy > jkdelta + prev_jy))
		ykeysym.sym = SDLK_DOWN;
	else if (!jkyfixed && (next_jy < -jkdelta + prev_jy))
		ykeysym.sym = SDLK_UP;
	else
		ykeysym.sym = 0;

	if (last_ykeysym.sym && last_ykeysym.sym != ykeysym.sym) {
		SDL_PrivateKeyboard(SDL_RELEASED, &last_ykeysym);
		last_ykeysym.sym = 0;
	}

	if (ykeysym.sym) {
		SDL_PrivateKeyboard(SDL_PRESSED, &ykeysym);
		last_ykeysym = ykeysym;
	}
#else
	if (fabs(next_jx) > (fabs(prev_jx)+JDELTA) || fabs(next_jx) < (fabs(prev_jx)-JDELTA)) {
		SDL_PrivateJoystickAxis(joystick, 0, next_jx * AXIS_MAX);
	}
	if (fabs(next_jy) > (fabs(prev_jy)+JDELTA) || fabs(next_jy) < (fabs(prev_jy)-JDELTA)) {
		SDL_PrivateJoystickAxis(joystick, 1, next_jy * AXIS_MAX);
	}
#endif

	prev_jy = next_jy;
	prev_jx = next_jx;
}

/* Function to close a joystick after use */
void SDL_SYS_JoystickClose(SDL_Joystick *joystick)
{
	return;
}

/* Function to perform any system-specific joystick related cleanup */
void SDL_SYS_JoystickQuit(void)
{
	joystickReset = 1;
	return;
}

#endif /* SDL_JOYSTICK_DUMMY || SDL_JOYSTICK_DISABLED */
