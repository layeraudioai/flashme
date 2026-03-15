// This file provides dummy implementations for libnds functions
// that the modern devkitPro crt0 expects, but which this
// very old, self-contained project does not use or provide.
// This allows the project to link against a modern toolchain.

#include <NDS/jtypes.h>

void __libnds_mpu_setup(void) {}
void __dsimode(void) {}
void __secure_area__(void) {}
void initSystem(void) {}
void __libnds_exit(void) { while(1); }

#ifdef ARM7
// ARM7 Specific Stubs

// Dummy implementation for reading firmware (normally does a BIOS call or SPI read)
void bios_firmware_read(u32 srcaddr, u8 *dstaddr, u32 size) {
    // Implementation missing in source port
}

// Dummy table for sound effects to satisfy the linker
const u8 dummy_sfx[4] = {0};
const u8 * const sfx_sampletable[10] = {
    dummy_sfx, dummy_sfx, dummy_sfx, dummy_sfx, dummy_sfx, 
    dummy_sfx, dummy_sfx, dummy_sfx, dummy_sfx, dummy_sfx
};
#endif