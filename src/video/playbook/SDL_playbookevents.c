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
#include "SDL.h"
#include "SDL_syswm.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "SDL_keysym.h"

#include "SDL_playbookvideo.h"
#include "SDL_playbookevents_c.h"

static SDL_keysym Playbook_Keycodes[256];
static SDLKey *Playbook_specialsyms;

static SDL_keysym navkey;

//#define TOUCHPAD_SIMULATE 1 // Still experimental
#ifdef TOUCHPAD_SIMULATE
struct TouchState {
	int oldPos[2];
	int pending[2];
	int mask;
	int moveId;
	int leftDown;
	int leftId;
	int middleDown;
	int middleId;
	int rightDown;
	int rightId;
};

static struct TouchState state = { {0, 0}, {0, 0}, 0, -1, 0, -1, 0, -1, 0, -1};
#endif
struct TouchEvent {
	int pending;
	int touching[3];
	int pos[2];
};
static struct TouchEvent moveEvent;


static void handlePointerEvent(screen_event_t event, screen_window_t window)
{
	int buttonState = 0;
	screen_get_event_property_iv(event, SCREEN_PROPERTY_BUTTONS, &buttonState);

	int coords[2];
	screen_get_event_property_iv(event, SCREEN_PROPERTY_SOURCE_POSITION, coords);

	int screen_coords[2];
	screen_get_event_property_iv(event, SCREEN_PROPERTY_POSITION, screen_coords);

	int wheel_delta;
	screen_get_event_property_iv(event, SCREEN_PROPERTY_MOUSE_WHEEL, &wheel_delta);

	if (coords[1] < 0) {
		fprintf(stderr, "Detected pointer swipe event: %d,%d\n", coords[0], coords[1]);
		return;
	}
	//fprintf(stderr, "Pointer: %d,(%d,%d),(%d,%d),%d\n", buttonState, coords[0], coords[1], screen_coords[0], screen_coords[1], wheel_delta);
	if (wheel_delta != 0) {
		int button;
		if ( wheel_delta > 0 )
			button = SDL_BUTTON_WHEELDOWN;
		else if ( wheel_delta < 0 )
			button = SDL_BUTTON_WHEELUP;
		SDL_PrivateMouseButton(
			SDL_PRESSED, button, 0, 0);
		SDL_PrivateMouseButton(
			SDL_RELEASED, button, 0, 0);
	}

	// FIXME: Pointer events have never been tested.
	static int lastButtonState = 0;
	if (lastButtonState == buttonState) {
		moveEvent.touching[0] = buttonState;
		moveEvent.pos[0] = coords[0];
		moveEvent.pos[1] = coords[1];
		moveEvent.pending = 1;
		return;
	}
	lastButtonState = buttonState;

	SDL_PrivateMouseButton(buttonState ? SDL_PRESSED : SDL_RELEASED, SDL_BUTTON_LEFT, coords[0], coords[1]); // FIXME: window
	moveEvent.pending = 0;
}

static int TranslateBluetoothKeyboard(int sym, int mods, int flags, int scan, int cap, SDL_keysym *keysym)
{
	if (flags == 32) {
		return 0; // No translation for this - this is an addition message sent
		// with arrow keys, right ctrl, right ctrl and pause
	}
	// FIXME: Figure out how to separate left and right modifiers
	if (scan > 128)
		scan -= 128; // Keyup events have the high bit set, but we want to have the same scan for down and up.
	keysym->scancode = scan;
	keysym->mod = 0;
	if (mods & (0x1))
		keysym->mod |= KMOD_LSHIFT;
	if (mods & (0x2))
		keysym->mod |= KMOD_LCTRL;
	if (mods & (0x4))
		keysym->mod |= KMOD_LALT;
	if (mods & (0x10000))
		keysym->mod |= KMOD_CAPS;
	if (mods & (0x20000)) // FIXME: guessing
		keysym->mod |= KMOD_NUM;
	//if (mods & (0x40000))
		//keysym.mod |= SCROLL LOCK; // SDL has no scroll lock

	if (sym & 0xf000) {
		sym = sym & 0xff;
		keysym->sym = Playbook_specialsyms[sym];
	} else {
		keysym->sym = Playbook_Keycodes[sym].sym;
	}

	return 1;
}

static int TranslateVKB(int sym, int mods, int flags, int scan, int cap, SDL_keysym *keysym)
{
	int shifted = 0;
	// FIXME: Keyboard handling (modifiers are currently ignored, some keys are as well)
	if (sym & 0xf000) {
		sym = sym & 0xff;
		keysym->sym = Playbook_specialsyms[sym];
	} else {
		keysym->sym = Playbook_Keycodes[sym].sym;
	}

	if (mods & (0x1))
		shifted = 1;

	keysym->scancode = keysym->sym;
	keysym->unicode = keysym->sym;

	/*
	switch (keysym->sym)
	{
	case SDLK_ASTERISK:
		keysym->unicode = '*';
		break;
	case SDLK_AT:
		keysym->unicode = '@';
		break;
	case SDLK_COLON:
		keysym->unicode = ':';
		break;
	case SDLK_COMMA:
		keysym->unicode = ',';
		break;
	case SDLK_DOLLAR:
		keysym->unicode = '$';
		break;
	case SDLK_EQUALS:
		keysym->unicode = '=';
		break;
	case SDLK_EXCLAIM:
		keysym->unicode = '!';
		break;
	case SDLK_HASH:
		keysym->unicode = '#';
		break;
	case SDLK_LEFTBRACKET:
		keysym->unicode = '(';
		break;
	case SDLK_MINUS:
		keysym->unicode = '-';
		break;
	case SDLK_PERIOD:
		keysym->unicode = '.';
		break;
	case SDLK_PLUS:
		keysym->unicode = '+';
		break;
	case SDLK_QUESTION:
		keysym->unicode = '?';
		break;
	case SDLK_QUOTE:
		keysym->unicode = '\'';
		break;
	case SDLK_QUOTEDBL:
		keysym->unicode = '"';
		break;
	case SDLK_RIGHTBRACKET:
		keysym->unicode = ')';
		break;
	case SDLK_SEMICOLON:
		keysym->unicode = ';';
		break;
	case SDLK_SLASH:
		keysym->unicode = '/';
		break;
	case SDLK_UNDERSCORE:
		keysym->unicode = '_';
		break;
	}
	*/

	keysym->mod = KMOD_NONE;
	return shifted;
}

static void handleKeyboardEvent(screen_event_t event)
{
	static const int KEYBOARD_TYPE_MASK = 0x20;
    int sym = 0;
    screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_SYM, &sym);
    int modifiers = 0;
    screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_MODIFIERS, &modifiers);
    int flags = 0;
    screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_FLAGS, &flags);
    int scan = 0;
    screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_SCAN, &scan);
    int cap = 0;
    screen_get_event_property_iv(event, SCREEN_PROPERTY_KEY_CAP, &cap);

	int shifted = 0;
	SDL_keysym keysym;
    if (flags & KEYBOARD_TYPE_MASK) {
    	if (!TranslateBluetoothKeyboard(sym, modifiers, flags, scan, cap, &keysym))
    	{
    		return; // No translation
    	}
    } else {
		shifted = TranslateVKB(sym, modifiers, flags, scan, cap, &keysym);
    }

    if (shifted) {
		SDL_keysym temp;
		temp.scancode = 42;
		temp.sym = SDLK_LSHIFT;
		SDL_PrivateKeyboard(SDL_PRESSED, &temp);
    }

    SDL_PrivateKeyboard((flags & 0x1)?SDL_PRESSED:SDL_RELEASED, &keysym);

    if (shifted) {
		SDL_keysym temp;
		temp.scancode = 42;
		temp.sym = SDLK_LSHIFT;
		SDL_PrivateKeyboard(SDL_RELEASED, &temp);
    }
}

static void handleMtouchEvent(screen_event_t event, screen_window_t window, int type)
{
#ifdef TOUCHPAD_SIMULATE

	unsigned buttonWidth = 200;
	unsigned buttonHeight = 200;

	int contactId;
	int pos[2];
	int screenPos[2];
	screen_get_event_property_iv(event, SCREEN_PROPERTY_TOUCH_ID, (int*)&contactId);
	screen_get_event_property_iv(event, SCREEN_PROPERTY_SOURCE_POSITION, pos);
	screen_get_event_property_iv(event, SCREEN_PROPERTY_POSITION, screenPos);

	int mouseX, mouseY;
	SDL_GetMouseState(&mouseX, &mouseY);

	if (state.leftId == contactId) {
		if (type == SCREEN_EVENT_MTOUCH_RELEASE) {
			state.leftId = -1;
			SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_LEFT, mouseX, mouseY);
		}
		return;
	}
	if (state.middleId == contactId) {
		if (type == SCREEN_EVENT_MTOUCH_RELEASE) {
			state.middleId = -1;
			SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_MIDDLE, mouseX, mouseY);
		}
		return;
	}
	if (state.rightId == contactId) {
		if (type == SCREEN_EVENT_MTOUCH_RELEASE) {
			state.rightId = -1;
			SDL_PrivateMouseButton(SDL_RELEASED, SDL_BUTTON_RIGHT, mouseX, mouseY);
		}
		return;
	}

	if (screenPos[0] < buttonWidth && contactId != state.moveId) {
		// Special handling for buttons

		if (screenPos[1] < buttonHeight) {
			// Middle button
			if (type == SCREEN_EVENT_MTOUCH_TOUCH) {
				if (state.middleId == -1 && contactId != state.rightId && contactId != state.leftId) {
					SDL_PrivateMouseButton(SDL_PRESSED, SDL_BUTTON_MIDDLE, mouseX, mouseY);
					state.middleId = contactId;
				}
			}
		} else if (screenPos[1] < buttonHeight*2){
			// Right button
			if (type == SCREEN_EVENT_MTOUCH_TOUCH) {
				if (state.rightId == -1 && contactId != state.leftId && contactId != state.middleId) {
					SDL_PrivateMouseButton(SDL_PRESSED, SDL_BUTTON_RIGHT, mouseX, mouseY);
					state.rightId = contactId;
				}
			}
		} else {
			// Left button
			if (type == SCREEN_EVENT_MTOUCH_TOUCH) {
				if (state.leftId == -1 && contactId != state.rightId && contactId != state.middleId) {
					SDL_PrivateMouseButton(SDL_PRESSED, SDL_BUTTON_LEFT, mouseX, mouseY);
					state.leftId = contactId;
				}
			}
		}
	} else {
		// Can only generate motion events
		int doMove = 0;
		switch (type) {
		case SCREEN_EVENT_MTOUCH_TOUCH:
			if (state.moveId == -1 || state.moveId == contactId) {
				state.moveId = contactId;
			}
			break;
		case SCREEN_EVENT_MTOUCH_RELEASE:
			if (state.moveId == contactId) {
				doMove = 1;
				state.moveId = -1;
			}
			break;
		case SCREEN_EVENT_MTOUCH_MOVE:
			if (state.moveId == contactId) {
				doMove = 1;
			}
			break;
		}

		if (doMove) {
			int mask = 0;
			if (state.leftDown)
				mask |= SDL_BUTTON_LEFT;
			if (state.middleDown)
				mask |= SDL_BUTTON_MIDDLE;
			if (state.rightDown)
				mask |= SDL_BUTTON_RIGHT;
			state.mask = mask;
			state.pending[0] += pos[0] - state.oldPos[0];
			state.pending[1] += pos[1] - state.oldPos[1];
			//SDL_PrivateMouseMotion(mask, 1, pos[0] - state.oldPos[0], pos[1] - state.oldPos[1]);
		}
		state.oldPos[0] = pos[0];
		state.oldPos[1] = pos[1];
	}
#else
    int contactId;
    int pos[2];
    /*
    int screenPos[2];
    int orientation;
    int pressure;
    long long timestamp;
    int sequenceId;
    */

    screen_get_event_property_iv(event, SCREEN_PROPERTY_TOUCH_ID, (int*)&contactId);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_SOURCE_POSITION, pos);
    /*
    screen_get_event_property_iv(event, SCREEN_PROPERTY_POSITION, screenPos);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_TOUCH_ORIENTATION, (int*)&orientation);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_TOUCH_PRESSURE, (int*)&pressure); // Pointless - always 1 if down
    screen_get_event_property_llv(event, SCREEN_PROPERTY_TIMESTAMP, (long long*)&timestamp);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_SEQUENCE_ID, (int*)&sequenceId);
    */

    if (contactId>2 || pos[0] < 0 || pos[1] < 0)
    	return;

    int SDLButton = contactId+1;

    if (type == SCREEN_EVENT_MTOUCH_TOUCH) {
    	if (!moveEvent.touching[contactId])
    		SDL_PrivateMouseButton(SDL_PRESSED, SDLButton, pos[0], pos[1]);
    	moveEvent.touching[contactId] = 1;
    } else if (type == SCREEN_EVENT_MTOUCH_RELEASE) {
    	if (moveEvent.touching[contactId])
    		SDL_PrivateMouseButton(SDL_RELEASED, SDLButton, pos[0], pos[1]);
    	moveEvent.touching[contactId] = 0;
    } else if (type == SCREEN_EVENT_MTOUCH_MOVE) {
    	if (moveEvent.touching[contactId])
    		SDL_PrivateMouseMotion(SDLButton, 0, pos[0], pos[1]);
    }
#endif
}

void handleNavigatorEvent (bps_event_t *bps_event) {
    switch(bps_event_get_code(bps_event)) {
    case (NAVIGATOR_EXIT):
    	SDL_PrivateQuit();
    	break;
    case (NAVIGATOR_BACK):
		navkey.sym = SDLK_ESCAPE;
    	SDL_PrivateKeyboard(SDL_PRESSED, &navkey);
    	break;
    case (NAVIGATOR_SWIPE_DOWN):
		navkey.sym = SDLK_MENU;
		SDL_PrivateKeyboard(SDL_PRESSED, &navkey);
    	break;
    case (NAVIGATOR_WINDOW_STATE):
		switch(navigator_event_get_window_state(bps_event)) {
		case NAVIGATOR_WINDOW_FULLSCREEN:
			SDL_PrivateAppActive(1, (SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS));
			SDL_PauseAudio(0);
			break;
		case NAVIGATOR_WINDOW_THUMBNAIL:
			SDL_PrivateAppActive(0, (SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS));
			SDL_PauseAudio(1);
			break;
		case NAVIGATOR_WINDOW_INVISIBLE:
			SDL_PrivateAppActive(0, (SDL_APPACTIVE|SDL_APPINPUTFOCUS|SDL_APPMOUSEFOCUS));
			SDL_PauseAudio(1);
			break;
		}
    	break;
    default:
    	break;
    }
}

void handlePaymentEvent (bps_event_t *bps_event)
{
    if (paymentservice_event_get_response_code(bps_event) == SUCCESS_RESPONSE) {
        if (bps_event_get_code(bps_event) == PURCHASE_RESPONSE) {
        	SDL_SysWMinfo info;
    		SDL_GetWMInfo(&info);

    		unsigned request_id;
    		paymentservice_get_existing_purchases_request(false, info.group, &request_id);
        } else {
            int purchases = paymentservice_event_get_number_purchases(bps_event);

            int i = 0;
            for (i = 0; i<purchases; i++) {
            	SDL_Event event;
            	event.type = SDL_USEREVENT;
            	event.user.code = 1;
            	event.user.data1 = (void *)paymentservice_event_get_digital_good_id(bps_event, i);
            	event.user.data2 = (void *)paymentservice_event_get_digital_good_sku(bps_event, i);
            	SDL_PushEvent(&event);
            }
        }
    } else {
        unsigned request_id = paymentservice_event_get_request_id(bps_event);
        int error_id = paymentservice_event_get_error_id(bps_event);
        const char* error_text = paymentservice_event_get_error_text(bps_event);

        fprintf(stderr, "Payment System error. Request ID: %d  Error ID: %d  Text: %s\n",
                request_id, error_id, error_text ? error_text : "N/A");
    }
}

void handleScreenEvent (bps_event_t *bps_event)
{
	int type;
	screen_event_t screen_event = screen_event_get_event(bps_event);
	screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_TYPE, &type);

	screen_window_t window;
	screen_get_event_property_pv(screen_event, SCREEN_PROPERTY_WINDOW, (void **)&window);

	SDL_SysWMinfo info;

	switch (type) {
	case SCREEN_EVENT_CLOSE:
		SDL_GetWMInfo(&info);
		if (info.window == window)
			SDL_PrivateQuit(); // We can't stop it from closing anyway
		break;
	case SCREEN_EVENT_POINTER:
		handlePointerEvent(screen_event, window);
		break;
	case SCREEN_EVENT_KEYBOARD:
		handleKeyboardEvent(screen_event);
		break;
	case SCREEN_EVENT_MTOUCH_TOUCH:
	case SCREEN_EVENT_MTOUCH_MOVE:
	case SCREEN_EVENT_MTOUCH_RELEASE:
		handleMtouchEvent(screen_event, window, type);
		break;
	default:
		break;
	}
}

void
PLAYBOOK_PumpEvents(_THIS)
{
	if (navkey.sym) {
		SDL_PrivateKeyboard(SDL_RELEASED, &navkey);
		navkey.sym = 0;
	}

    bps_event_t *bps_event = NULL;
    bps_get_event(&bps_event, 0);

    while (bps_event) {
		int domain = bps_event_get_domain(bps_event);

		if (domain == screen_get_domain())
			handleScreenEvent(bps_event);
		else if (domain == navigator_get_domain())
			handleNavigatorEvent(bps_event);
		else if (domain == paymentservice_get_domain())
			handlePaymentEvent(bps_event);

	    bps_get_event(&bps_event, 0);
    }

#ifdef TOUCHPAD_SIMULATE
	if (state.pending[0] || state.pending[1]) {
		SDL_PrivateMouseMotion(state.mask, 1, state.pending[0], state.pending[1]);
		state.pending[0] = 0;
		state.pending[1] = 0;
	}
#endif
	if (moveEvent.pending) {
		SDL_PrivateMouseMotion((moveEvent.touching[0]?SDL_BUTTON_LEFT:0), 0, moveEvent.pos[0], moveEvent.pos[1]);
		moveEvent.pending = 0;
	}
}

void PLAYBOOK_InitOSKeymap(_THIS)
{
	{
		// We match perfectly from 32 to 64
		int i = 32;
		for (; i<65; i++)
		{
			Playbook_Keycodes[i].sym = i;
		}
		// Capital letters
		for (; i<91; i++)
		{
			Playbook_Keycodes[i].sym = i+32;
			Playbook_Keycodes[i].mod = KMOD_LSHIFT;
		}
		// Perfect matches again from 91 to 122
		for (; i<123; i++)
		{
			Playbook_Keycodes[i].sym = i;
		}
	}

	{
		Playbook_specialsyms = (SDLKey *)malloc(256 * sizeof(SDLKey));
		Playbook_specialsyms[SDLK_BACKSPACE] = SDLK_BACKSPACE;
		Playbook_specialsyms[SDLK_TAB] = SDLK_TAB;
		Playbook_specialsyms[SDLK_RETURN] = SDLK_RETURN;
		Playbook_specialsyms[SDLK_PAUSE] = SDLK_PAUSE;
		Playbook_specialsyms[SDLK_ESCAPE] = SDLK_ESCAPE;
		Playbook_specialsyms[0xff] = SDLK_DELETE;
		Playbook_specialsyms[0x52] = SDLK_UP;
		Playbook_specialsyms[0x54] = SDLK_DOWN;
		Playbook_specialsyms[0x53] = SDLK_RIGHT;
		Playbook_specialsyms[0x51] = SDLK_LEFT;
		Playbook_specialsyms[0x63] = SDLK_INSERT;
		Playbook_specialsyms[0x50] = SDLK_HOME;
		Playbook_specialsyms[0x57] = SDLK_END;
		Playbook_specialsyms[0x55] = SDLK_PAGEUP;
		Playbook_specialsyms[0x56] = SDLK_PAGEDOWN;
		Playbook_specialsyms[0xbe] = SDLK_F1;
		Playbook_specialsyms[0xbf] = SDLK_F2;
		Playbook_specialsyms[0xc0] = SDLK_F3;
		Playbook_specialsyms[0xc1] = SDLK_F4;
		Playbook_specialsyms[0xc2] = SDLK_F5;
		Playbook_specialsyms[0xc3] = SDLK_F6;
		Playbook_specialsyms[0xc4] = SDLK_F7;
		Playbook_specialsyms[0xc5] = SDLK_F8;
		Playbook_specialsyms[0xc6] = SDLK_F9;
		Playbook_specialsyms[0xc7] = SDLK_F10;
		Playbook_specialsyms[0xc8] = SDLK_F11;
		Playbook_specialsyms[0xc9] = SDLK_F12;
		Playbook_specialsyms[0xe5] = SDLK_CAPSLOCK;
		Playbook_specialsyms[0x14] = SDLK_SCROLLOCK;
		Playbook_specialsyms[0xe2] = SDLK_RSHIFT;
		Playbook_specialsyms[0xe1] = SDLK_LSHIFT;
		Playbook_specialsyms[0xe4] = SDLK_RCTRL;
		Playbook_specialsyms[0xe3] = SDLK_LCTRL;
		Playbook_specialsyms[0xe8] = SDLK_RALT;
		Playbook_specialsyms[0xe9] = SDLK_LALT;
		Playbook_specialsyms[0xbe] = SDLK_MENU;
		Playbook_specialsyms[0x61] = SDLK_SYSREQ;
		Playbook_specialsyms[0x6b] = SDLK_BREAK;
	}

#if 0 // Possible further keycodes that are available on the VKB
	Playbook_Keycodes[123].sym = SDLK_SPACE; /* { */
	Playbook_Keycodes[124].sym = SDLK_SPACE; /* | */
	Playbook_Keycodes[125].sym = SDLK_SPACE; /* } */
	Playbook_Keycodes[126].sym = SDLK_SPACE; /* ~ */
	Playbook_Keycodes[161].sym = SDLK_SPACE; /* upside down ! */
	Playbook_Keycodes[163].sym = SDLK_SPACE; /* British pound */
	Playbook_Keycodes[164].sym = SDLK_SPACE; /* odd circle/x */
	Playbook_Keycodes[165].sym = SDLK_SPACE; /* Japanese yen */
	Playbook_Keycodes[171].sym = SDLK_SPACE; /* small << */
	Playbook_Keycodes[177].sym = SDLK_SPACE; /* +/- */
	Playbook_Keycodes[183].sym = SDLK_SPACE; /* dot product */
	Playbook_Keycodes[187].sym = SDLK_SPACE; /* small >> */
	Playbook_Keycodes[191].sym = SDLK_SPACE; /* upside down ? */
	Playbook_Keycodes[247].sym = SDLK_SPACE; /* division */
	/*
	 * Playbook_Keycodes[8220] = smart double quote (top)
	 * Playbook_Keycodes[8221] = smart double quote (bottom)
	 * Playbook_Keycodes[8364] = euro
	 * Playbook_Keycodes[61448] = backspace
	 * Playbook_Keycodes[61453] = return
	 */
#endif
}

/* end of SDL_playbookevents.c ... */

