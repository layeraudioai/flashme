// #pragma arm section code="arm7code", rodata="arm7code", rwdata="arm7data", zidata="arm7zi"

#include <NDS/NDS.h>
#include <DS_misc.h>
#include "7.h"

extern const u8 * const sfx_sampletable[];

inline void stop() {
	SOUND0_CR=0;
}

void play(int i) {
	const u8 *sfx_start,*sfx_end;
	stop();

	sfx_start=sfx_sampletable[i];
	sfx_end=sfx_sampletable[i+1];
	
	SOUND0_VOL = 0x13F;
	SOUND0_TIMER = 0x10000 - 760;	//22050Hz
	SOUND0_SRC = (u32)sfx_start;
	SOUND0_REP_PT = 0;
	SOUND0_REP_LEN = ((u32)sfx_end-(u32)sfx_start)/4;
	SOUND0_CR=0x9004;
}

void playsound(u32 sfx) {
	if(sfx) {
		if(sfx==SFX_STOP)
			stop();
		else
			play(sfx);
	}
}

void soundinit() {
	POWER_CR = 0x0001;
	SOUNDCNT = 0x807F;
	SOUND0_VOL = 0x7F;
	SOUND0_TIMER = 0x10000 - 760;	//22050Hz
}
