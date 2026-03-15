//////////////////////////////////////////////////////////////////////
//
// NDS.h -- Master include (includes the rest of the library)
//
// version 0.1, February 14, 2005
//
//  Copyright (C) 2005 Michael Noland (joat) and Jason Rogers (dovoto)
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any
//  damages arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any
//  purpose, including commercial applications, and to alter it and
//  redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you
//     must not claim that you wrote the original software. If you use
//     this software in a product, an acknowledgment in the product
//     documentation would be appreciated but is not required.
//  2. Altered source versions must be plainly marked as such, and
//     must not be misrepresented as being the original software.
//  3. This notice may not be removed or altered from any source
//     distribution.
//
// Changelog:
//   0.1: First version
//
//////////////////////////////////////////////////////////////////////

#ifndef NDS_INCLUDE
#define NDS_INCLUDE

//////////////////////////////////////////////////////////////////////

#ifndef ARM7
#ifndef ARM9
#error Either ARM7 or ARM9 must be defined
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

#include <NDS/jtypes.h>

#ifdef ARM9
#include <NDS/ARM9/video.h>
#include <NDS/ARM9/cache.h>
#include <NDS/ARM9/BIOS.h>
#include <NDS/ARM9/trig_lut.h>
#include <NDS/ARM9/math.h>
#include <NDS/ARM9/keys.h>
#endif

#ifdef ARM7
#include <NDS/ARM7/touch.h>
#include <NDS/ARM7/BIOS.h>
#include <NDS/ARM7/clock.h>
#include <NDS/ARM7/audio.h>
#include <NDS/ARM7/wifi.h>
#include <NDS/ARM7/serial.h>
#endif

#include <NDS/card.h>

#include <NDS/memory.h>
#include <NDS/DMA.h>
#include <NDS/timers.h>
#include <NDS/system.h>
#include <NDS/interrupts.h>
#include <NDS/ipc.h>

// print a string to dualis's output window
/*static inline void dPrint(char *s)
{
    // warning! will crash on hardware!
    asm volatile ("mov r0, %0;" "swi 0xff0000;":
                  :"r" (s):"r0");
}*/
//////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

//////////////////////////////////////////////////////////////////////

#endif

//////////////////////////////////////////////////////////////////////

