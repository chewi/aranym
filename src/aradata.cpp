/* Joy 2001 */

#include "sysdeps.h"
#include "hardware.h"
#include "cpu_emulation.h"
#include "memory.h"
#include "aradata.h"
#include "parameters.h"

ARADATA::ARADATA(memptr addr, uint32 size) : BASE_IO(addr, size) {
	mouseDriver = false;
	mouse_x = -1;
	mouse_y = -1;
}

uae_u8 ARADATA::handleRead(uaecptr addr) {
	addr -= getHWoffset();
	switch(addr) {
		case 0: return '_';
		case 1: return 'A';
		case 2: return 'R';
		case 3: return 'A';
		case 4: return 0;	/* VERSION_MAJOR */
		case 5: return 0;	/* VERSION_MINOR */
		case 6: return FastRAMSize >> 24;
		case 7: return FastRAMSize >> 16;
		case 8: return FastRAMSize >> 8;
		case 9: return FastRAMSize;
	}

	return 0;
}

void ARADATA::handleWrite(uaecptr addr, uae_u8 value) {
	addr -= getHWoffset();
	switch(addr) {
		case 14: mouse_x = (mouse_x & 0xff) | (value << 8); break;
		case 15: mouse_x = (mouse_x & 0xff00) | value; break;
		case 16: mouse_y = (mouse_y & 0xff) | (value << 8); break;
		case 17: mouse_y = (mouse_y & 0xff00) | value; break;
	}
	mouseDriver = true;
}
