// #pragma arm section code="arm7code", rodata="arm7code", rwdata="arm7data", zidata="arm7zi"

#include <NDS/NDS.h>
#include <DS_misc.h>
#include <string.h>
#include "7.h"

int mini_sprintf(char *str, const char *fmt, ...);
#define sprintf mini_sprintf

void bios_firmware_read(u32 srcaddr,u8 *dstaddr,u32 size);
void bios_SPI_write(int device,int data,int select);
void smalldelay(void);

int fifo_send(u32 data) {
    if (REG_FIFO_CNT & FIFO_CNT_ERR) {
		REG_FIFO_CNT = FIFO_CNT_SEND_CLEAR | FIFO_CNT_SEND_IRQ | FIFO_CNT_RECV_IRQ | FIFO_CNT_ENABLE | FIFO_CNT_ERR;
		return FAIL_SEND_ERR;
    }
	if(REG_FIFO_CNT&FIFO_CNT_SEND_FULL)
		return FAIL_SEND_FULL;
	REG_FIFO_SEND = data;
	return SUCCESS;
}

int line;
void text(char *s) {
	u16 *p;
	p=(u16*)(0x2000000+line*64);
	while(*s)
		*(p++)=*(s++);
	while((u32)p&0x3f)
		*(p++)=' ';
	fifo_send(line++);
}

void textN(char *s,int line) {
	u16 *p;
	p=(u16*)(0x2000000+line*64);
	while(*s)
		*(p++)=*(s++);
	while((u32)p&0x3f)
		*(p++)=' ';
	fifo_send(line);
}

u8 writereadSPI(u8 data) {
	while (SERIAL_CR & SERIAL_BUSY);
	SERIAL_DATA = data;
	while (SERIAL_CR & SERIAL_BUSY);
	return SERIAL_DATA;
	
}

u8 writePM(u8 channel, u8 data) {
	u8 ret;
	SERIAL_CR = SPI_EN|SPI_SEL|SPI_POWER|2;
	writereadSPI(channel);
	SERIAL_CR = SPI_EN|SPI_POWER|2;
	ret=writereadSPI(data);	
	SERIAL_CR = 0;
	return ret;
}
#define readPM(chan) writePM((chan)|0x80,0)

void flash(u8 *src,u32 dst) {
	int i;
	
	//write enable
	SERIAL_CR = SPI_EN|SPI_SEL|SPI_FW;
	writereadSPI(6);
	SERIAL_CR = 0;
	
	//Wait for Write Enable Latch to be set
	SERIAL_CR = SPI_EN|SPI_SEL|SPI_FW;
	writereadSPI(5);
	while((writereadSPI(0)&0x02)==0); //Write Enable Latch
	SERIAL_CR = 0;
    
	//page write
	SERIAL_CR = SPI_EN|SPI_SEL|SPI_FW;
	writereadSPI(0x0A);
	writereadSPI((dst&0xff0000)>>16);
	writereadSPI((dst&0xff00)>>8);
	writereadSPI(0);
	for (i=0; i<256; i++) {
		writereadSPI(src[i]);
	}
	SERIAL_CR=0;
	// wait programming to finish
	SERIAL_CR = SPI_EN|SPI_SEL|SPI_FW;
	writereadSPI(0x05);
	while(writereadSPI(0)&0x01);	// WIP (Write In Progress)
	SERIAL_CR = 0;
}

int flash_verify(u8 *new,u8 *old,u32 fwaddr) {
	int i,cmp;	

	cmp=0;
	for(i=0;i<256;i++) {
		cmp|=(old[i]!=new[i]);
	}
	if(!cmp) return 1;	//already the same, don't bother flashing
	
	flash(new,fwaddr);
	bios_firmware_read(fwaddr,old,256);

	cmp=0;
	for(i=0;i<256;i++) {
		cmp|=(old[i]!=new[i]);
	}
	return !cmp;
}

void initialize_DS() {
	u8 lite;
	writePM(0,PM_BOTTOMLIGHT|PM_SOUND);

	lite=readPM(4);			//make sure lighting is set (in case of failsafe boot)
	if(lite&0x40) {
		lite&=0xfc;
		lite|=2;
		writePM(4,lite);
	}
}

void stop() {
	while(1) swiSleep();
}

void VBlankWait() {			//interrupts don't work for some fucking reason
	while(DISP_SR&DISP_IN_VBLANK);
	while(!(DISP_SR&DISP_IN_VBLANK));
}

#define KEY_X      (1<<10)
#define KEY_Y      (1<<11)

int getkey() {
	static int oldkey;
	int k,x;

	// READ_KEYS contains A, B, Select, Start, D-Pad, R, L (bits 0-9)
	k = READ_KEYS;

	// XKEYS (REG_KEYXY) contains X and Y on bits 1 and 0, active low.
	int xy_keys = ~XKEYS;
	if (xy_keys & 2) k |= KEY_X; // Bit 1 is X
	if (xy_keys & 1) k |= KEY_Y; // Bit 0 is Y

	x=(oldkey^k)&k;
	oldkey=k;
	return x;
}

/* 
   Pattern Matching for Hex Editing in RAM
   Scans the binary blob for a specific byte sequence.
*/
int find_pattern(u8 *data, u32 size, const u8 *pattern, u32 pat_len) {
	u32 i, j;
	int match;
	for (i = 0; i < size - pat_len; i++) {
		match = 1;
		for(j = 0; j < pat_len; j++) {
			if(data[i+j] != pattern[j]) {
				match = 0;
				break;
			}
		}
		if(match) return i;
	}
	return -1;
}

/*
   Inject Homebrew Wifi Menu Entry
   This attempts to hijack an existing menu slot or add a new one.
*/
void patch_firmware_ui(u8 *fw) {
	// Get ARM9 offset and size from NDS Header
	u32 arm9_off = *(u32*)(fw + 0x20);
	u32 arm9_ram_addr = *(u32*)(fw + 0x24);
	u32 arm9_size = *(u32*)(fw + 0x2C);
	u8 *arm9_ptr = fw + arm9_off;
	u32 payload_buffer[256]; // Buffer to construct our splitter code (1KB)

	// --- Step 1: Find a location for our code payload ---
    // We'll search for a block of 0xFF padding to inject our code.
    // We need more space now (~500 bytes for strings + code)
    // Let's look for a larger block to be safe
    u8 padding_sig[256];
    memset(padding_sig, 0xFF, 256); 
    
    int payload_offset = find_pattern(arm9_ptr, arm9_size, padding_sig, 256);

    if (payload_offset == -1) {
        text("Could not find padding for payload!");
        smalldelay();
        return;
    }
    

	// --- Step 2: Find the UI table to patch ---
	// Automated Detection: Find "PictoChat" string (UTF-16LE)
    const u8 picto_sig[] = { 
        0x50, 0x00, 0x69, 0x00, 0x63, 0x00, 0x74, 0x00, 0x6F, 0x00, 
        0x43, 0x00, 0x68, 0x00, 0x61, 0x00, 0x74, 0x00 
    }; 

    // "DS Download Play" (UTF-16LE)
    const u8 dlplay_sig[] = {
        0x44,0,0x53,0,0x20,0,0x44,0,0x6F,0,0x77,0,0x6E,0,0x6C,0,0x6F,0,0x61,0,0x64,0,0x20,0,0x50,0,0x6C,0,0x61,0,0x79,0
    };

    // "There is no DS..." (UTF-16LE) - Proxy for DS Game button when empty
    const u8 no_ds_sig[] = {
        0x54,0,0x68,0,0x65,0,0x72,0,0x65,0,0x20,0,0x69,0,0x73,0,0x20,0,0x6E,0,0x6F,0,0x20,0,0x44,0,0x53,0
    };

    // "There is no GBA..." (UTF-16LE) - Proxy for GBA button when empty
    const u8 no_gba_sig[] = {
        0x54,0,0x68,0,0x65,0,0x72,0,0x65,0,0x20,0,0x69,0,0x73,0,0x20,0,0x6E,0,0x6F,0,0x20,0,0x47,0,0x42,0,0x41,0
    };

	int str_offset = find_pattern(arm9_ptr, arm9_size, picto_sig, sizeof(picto_sig));
    int dl_offset  = find_pattern(arm9_ptr, arm9_size, dlplay_sig, sizeof(dlplay_sig));
    int ds_offset  = find_pattern(arm9_ptr, arm9_size, no_ds_sig, sizeof(no_ds_sig));
    int gba_offset = find_pattern(arm9_ptr, arm9_size, no_gba_sig, sizeof(no_gba_sig));

    u32 addr_picto  = (str_offset != -1) ? (arm9_ram_addr + str_offset) : 0;
    u32 addr_dl     = (dl_offset  != -1) ? (arm9_ram_addr + dl_offset)  : 0;
    u32 addr_ds     = (ds_offset  != -1) ? (arm9_ram_addr + ds_offset)  : 0;
    u32 addr_gba    = (gba_offset != -1) ? (arm9_ram_addr + gba_offset) : 0;

	if (str_offset != -1) {
		// --- Step 2a: Patch the UI string for visual feedback ---
		text("Splitting PictoChat UI...");
		// "Picto / Wifi" (UTF-16LE) - Fits within original length
		const u8 new_label[] = {
			'P',0,'i',0,'c',0,'t',0,'o',0,' ',0,'/',0,' ',0,'W',0,'i',0,'f',0,'i',0, 0,0
		};
		// Be careful not to overflow original string buffer
		if (sizeof(new_label) <= sizeof(picto_sig)) {
			memcpy(arm9_ptr + str_offset, new_label, sizeof(new_label));
		}

		u32 str_ram_addr = arm9_ram_addr + str_offset;
		
        // Find the reference to this string in the code (The Menu Entry Structure)
        int ref_offset = find_pattern(arm9_ptr, arm9_size, (u8*)&str_ram_addr, 4);

        if (ref_offset != -1) {
            text("Menu Entry Found.");
            
            // Heuristic Scan: The function pointer is usually within 16 bytes of the string pointer.
            // We scan forward from the string pointer for a value that looks like a RAM address.
            int i;
            int found = 0;
            for (i = 4; i <= 16; i += 4) {
                 u32 *ptr = (u32*)(arm9_ptr + ref_offset + i);
                 // Check if it's a pointer to Main RAM (0x02xxxxxx)
                 if ((*ptr & 0xFF000000) == 0x02000000) {
                     u32 old_func_addr = *ptr;
                     u32 payload_ram_addr = arm9_ram_addr + payload_offset;

                     // --- Splitter Logic ---
                     int p = 0;
                     // 0: LDR R0, [PC, #offset] ; Load address of IPC Touch Y data
                     payload_buffer[p++] = 0xE59F0000; // To be patched
                     // 1: LDRH R1, [R0]         ; Load the 16-bit Y-coordinate
                     payload_buffer[p++] = 0xE1D010B0;
                     // 2: CMP R1, #96           ; Compare Y with 96 (screen midpoint)
                     payload_buffer[p++] = 0xE3510060;
                     // 3: BGT to Wifi Logic     ; If greater (bottom half), branch
                     payload_buffer[p++] = 0xCA000001; // Jumps to index 6
                     // 4: LDR R12, [PC, #offset]; Load original PictoChat function address
                     payload_buffer[p++] = 0xE59FC000; // To be patched
                     // 5: BX R12                ; Jump to original PictoChat function
                     payload_buffer[p++] = 0xE12FFF1C;
                     
                     // --- Wifi Cfg Logic ---
                     // 6: Update Top Screen "WIFI SETTINGS"
                     payload_buffer[p++] = 0xE59F0000; // To be patched
                     payload_buffer[p++] = 0xE59F1000; // To be patched
                     payload_buffer[p++] = 0xE59F2000; // To be patched
                     payload_buffer[p++] = 0xEF00000C; // SWI 0x0C

                     // 10: Copy "Connection A" (DS Game)
                     payload_buffer[p++] = 0xE59F0000; // R0 = &str_conn_a
                     payload_buffer[p++] = 0xE59F1000; // R1 = addr_ds
                     payload_buffer[p++] = 0xE3A0200D; // R2 = 13 (26 bytes / 2)
                     payload_buffer[p++] = 0xEF00000B; // SWI 0x0B (CpuSet)

                     // 14: Copy "Connection B" (PictoChat)
                     payload_buffer[p++] = 0xE59F0000; // R0 = &str_conn_b
                     payload_buffer[p++] = 0xE59F1000; // R1 = addr_picto
                     payload_buffer[p++] = 0xE3A0200D; // R2 = 13
                     payload_buffer[p++] = 0xEF00000B;

                     // 18: Copy "Connection C" (Download Play)
                     payload_buffer[p++] = 0xE59F0000; // R0 = &str_conn_c
                     payload_buffer[p++] = 0xE59F1000; // R1 = addr_dl
                     payload_buffer[p++] = 0xE3A0200D; // R2 = 13
                     payload_buffer[p++] = 0xEF00000B;

                     // 22: Copy "WFC USB" (GBA)
                     payload_buffer[p++] = 0xE59F0000; // R0 = &str_wfc_usb
                     payload_buffer[p++] = 0xE59F1000; // R1 = addr_gba
                     payload_buffer[p++] = 0xE3A02008; // R2 = 8 (16 bytes / 2)
                     payload_buffer[p++] = 0xEF00000B;

                     // --- SSID Entry State ---
                     // 26: Call Keyboard for SSID
                     payload_buffer[p++] = 0xE59F0000; // LDR R0, [PC, #offset_to_ssid_buffer]
                     payload_buffer[p++] = 0xE3A01020; // MOV R1, #32 (max length)
                     payload_buffer[p++] = 0xE59FC000; // LDR R12, [PC, #offset_to_keyboard_func]
                     payload_buffer[p++] = 0xE12FFF3C; // BLX R12

                     // --- Password Entry State ---
                     // 30: Call Keyboard for Password
                     payload_buffer[p++] = 0xE59F0000; // LDR R0, [PC, #offset_to_pass_buffer]
                     payload_buffer[p++] = 0xE3A0103F; // MOV R1, #63 (max length)
                     payload_buffer[p++] = 0xE59FC000; // LDR R12, [PC, #offset_to_keyboard_func]
                     payload_buffer[p++] = 0xE12FFF3C; // BLX R12
                     
                     // --- Save State ---
                     // 34: Save SSID buffer to internal RAM
                     payload_buffer[p++] = 0xE59F0000; // LDR R0, [PC, #offset_to_ssid_buffer]
                     payload_buffer[p++] = 0xE59F1000; // LDR R1, [PC, #offset_to_save_loc_ssid]
                     payload_buffer[p++] = 0xE3A02008; // MOV R2, #8 (32 bytes / 4 words)
                     payload_buffer[p++] = 0xEF00000B; // SWI CpuSet (word copy)

                     // 38: Save Password buffer to internal RAM
                     payload_buffer[p++] = 0xE59F0000; // LDR R0, [PC, #offset_to_pass_buffer]
                     payload_buffer[p++] = 0xE59F1000; // LDR R1, [PC, #offset_to_save_loc_pass]
                     payload_buffer[p++] = 0xE3A02010; // MOV R2, #16 (64 bytes / 4 words)
                     payload_buffer[p++] = 0xEF00000B; // SWI CpuSet (word copy)
                     
                     // Infinite loop (End)
                     payload_buffer[p++] = 0xEAFFFFFE;
                     
                     // --- Data Constants ---
                     int data_idx = p;
                     // Address of IPC Touch Y pixel
                     payload_buffer[p++] = 0x027FF00A;
                     // Address of Original Function
                     payload_buffer[p++] = old_func_addr;
                     // Top Screen String Ptr (WIFI SETTINGS)
                     payload_buffer[p++] = 0; 
                     // VRAM address
                     payload_buffer[p++] = 0x06000000;
                     // Top Screen Size (32 halfwords = 16 words)
                     payload_buffer[p++] = 16;
                     // Save Locations (Internal Memory)
                     payload_buffer[p++] = 0x027FF800;
                     payload_buffer[p++] = 0x027FF820; // SSID Save + 32 bytes

                     // Address of the firmware's keyboard function.
                     // This MUST be found via reverse engineering and replaced.
                     const u32 KEYBOARD_FUNC_ADDR_PLACEHOLDER = 0xDEADBEEF;
                     payload_buffer[p++] = KEYBOARD_FUNC_ADDR_PLACEHOLDER;
                     
                     // Addr DS
                     payload_buffer[p++] = addr_ds;
                     // Addr Picto
                     payload_buffer[p++] = addr_picto;
                     // Addr DL
                     payload_buffer[p++] = addr_dl;
                     // Addr GBA
                     payload_buffer[p++] = addr_gba;

                     // Strings Data
                     int str_base_idx = p;
                     
                     // "WIFI SETTINGS" (32 chars)
                     u16 wifi_str_u16[] = {
                         'W','I','F','I',' ','S','E','T','T','I','N','G','S',' ',' ',' ',
                         ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '
                     };
                     memcpy(&payload_buffer[p], wifi_str_u16, sizeof(wifi_str_u16));
                     p += sizeof(wifi_str_u16) / 4;

                     // "Connection A"
                     u16 conn_a[] = {'C','o','n','n','e','c','t','i','o','n',' ','A',0};
                     memcpy(&payload_buffer[p], conn_a, sizeof(conn_a));
                     int idx_str_a = p; p += (sizeof(conn_a)+3)/4;

                     // "Connection B"
                     u16 conn_b[] = {'C','o','n','n','e','c','t','i','o','n',' ','B',0};
                     memcpy(&payload_buffer[p], conn_b, sizeof(conn_b));
                     int idx_str_b = p; p += (sizeof(conn_b)+3)/4;

                     // "Connection C"
                     u16 conn_c[] = {'C','o','n','n','e','c','t','i','o','n',' ','C',0};
                     memcpy(&payload_buffer[p], conn_c, sizeof(conn_c));
                     int idx_str_c = p; p += (sizeof(conn_c)+3)/4;

                     // "WFC USB"
                     u16 wfc_usb[] = {'W','F','C',' ','U','S','B',0};
                     memcpy(&payload_buffer[p], wfc_usb, sizeof(wfc_usb));
                     int idx_str_d = p; p += (sizeof(wfc_usb)+3)/4;

                     // Buffers for keyboard input
                     u8 ssid_buffer[32] = {0};
                     memcpy(&payload_buffer[p], ssid_buffer, sizeof(ssid_buffer));
                     int idx_ssid_buf = p; p += sizeof(ssid_buffer)/4;

                     u8 pass_buffer[64] = {0};
                     memcpy(&payload_buffer[p], pass_buffer, sizeof(pass_buffer));
                     int idx_pass_buf = p; p += sizeof(pass_buffer)/4;

                     // --- Back-patching addresses and offsets ---
                     // String Pointers (RAM addresses)
                     payload_buffer[data_idx + 2] = payload_ram_addr + (str_base_idx * 4); // Top Screen Str

                     // LDR offsets
                     payload_buffer[0] |= ((data_idx + 0) * 4) - (0 * 4 + 8); // Touch Y Addr
                     payload_buffer[4] |= ((data_idx + 1) * 4) - (4 * 4 + 8); // Orig Func
                     
                     // Top Screen Update
                     payload_buffer[6] |= ((data_idx + 2) * 4) - (6 * 4 + 8); // R0 = Top Str
                     payload_buffer[7] |= ((data_idx + 3) * 4) - (7 * 4 + 8); // R1 = VRAM
                     payload_buffer[8] |= ((data_idx + 4) * 4) - (8 * 4 + 8); // R2 = Size

                     // Connection A (DS)
                     payload_buffer[10] = 0xE59F0000 | (((idx_str_a * 4) - (10 * 4 + 8)) & 0xFFF); // R0 = Str A
                     payload_buffer[11] |= ((data_idx + 5) * 4) - (11 * 4 + 8); // R1 = Addr DS

                     // Connection B (Picto)
                     payload_buffer[14] = 0xE59F0000 | (((idx_str_b * 4) - (14 * 4 + 8)) & 0xFFF);
                     payload_buffer[15] |= ((data_idx + 6) * 4) - (15 * 4 + 8); // R1 = Addr Picto

                     // Connection C (DL)
                     payload_buffer[18] = 0xE59F0000 | (((idx_str_c * 4) - (18 * 4 + 8)) & 0xFFF);
                     payload_buffer[19] |= ((data_idx + 7) * 4) - (19 * 4 + 8); // R1 = Addr DL

                     // WFC USB (GBA)
                     payload_buffer[22] = 0xE59F0000 | (((idx_str_d * 4) - (22 * 4 + 8)) & 0xFFF);
                     payload_buffer[23] |= ((data_idx + 8) * 4) - (23 * 4 + 8); // R1 = Addr GBA

                     // SSID Keyboard Call
                     payload_buffer[26] |= ((idx_ssid_buf * 4) - (26 * 4 + 8)) & 0xFFF; // R0 = &ssid_buffer
                     payload_buffer[28] |= ((data_idx + 7) * 4) - (28 * 4 + 8); // R12 = &keyboard_func

                     // Password Keyboard Call
                     payload_buffer[30] |= ((idx_pass_buf * 4) - (30 * 4 + 8)) & 0xFFF; // R0 = &pass_buffer
                     payload_buffer[32] |= ((data_idx + 7) * 4) - (32 * 4 + 8); // R12 = &keyboard_func

                     // Save SSID
                     payload_buffer[34] |= ((idx_ssid_buf * 4) - (34 * 4 + 8)) & 0xFFF; // R0 = &ssid_buffer
                     payload_buffer[35] |= ((data_idx + 5) * 4) - (35 * 4 + 8); // R1 = &save_loc_ssid

                     // Save Password
                     payload_buffer[38] |= ((idx_pass_buf * 4) - (38 * 4 + 8)) & 0xFFF; // R0 = &pass_buffer
                     payload_buffer[39] |= ((data_idx + 6) * 4) - (39 * 4 + 8); // R1 = &save_loc_pass

                     // Write payload to padding area
                     memcpy(arm9_ptr + payload_offset, payload_buffer, p * 4);

                     // Update Firmware Pointer
                     *ptr = payload_ram_addr;
                     found = 1;
                     text("Button Split Logic Installed.");
                     break;
                 }
            }
            if (!found) text("Func Ptr not found.");
        } else {
            text("UI Ref not found.");
        }
	} else {
		text("UI String not found."); 
		text("Skipping UI Patch.");
	}
}

void smalldelay() {
	int i;
	for(i=0;i<5;i++)
		VBlankWait();
}

#define NDS_VER1 0x2c7a
#define NDS_VER2 0xe0ce
#define NDS_VER3 0xbfba
#define NDS_VER4 0xdfc7
#define NDS_VER5 0x73b3
#define NDS_VER6 0xe843
#define NDS_VER7 0x0f1f

#define BACKUPHEADER 0x3f680
#define MAJOR_VER 0x17c
#define MINOR_VER (BACKUPHEADER+0x17c)
const u16 keysequence[]={KEY_X,KEY_B,KEY_X,KEY_B};

#define PROGRESS (*(u32*)0x2003ff8)
#define HEARTBEAT (*(u32*)0x2003ffc)

int main(void) {
	int lite;
	char s[32];
	char origstr[16];
	char oldstr[16];
	int orig_fw_ver;
	int new_fw_ver;
	int old_fw_ver;
	u8 *oldFW=(u8*)0x2300000;
	u8 *newFW;
	int i;
	u16 crc;
	u16 k;
	int seq;
	int sfx_timeout=0;

	soundinit();
	
	PROGRESS=0;
	HEARTBEAT=0;
	fifo_init();
	FIFO_INTR=0x700;
	while((FIFO_INTR&15)!=9);	//wait for arm9 ready

	lite=(readPM(4)&0x40)==0x40;	//DS or DSLite?
	if(!lite)			//dumb... firmware image is attached to arm9, so it tells us where to find it
		newFW=*(u8**)0x2004000;
	else
		newFW=*(u8**)0x2004004;

	initialize_DS();
	
	if(swiCRC(0xffff,0,0x4000)!=0x4695) {
		text("Unknown BIOS.");		//bios calls safe to use?
		stop();
	}

	crc=swiCRC(0xffff,(u32*)newFW,0x3fdfe);
	if(crc!=*(u16*)(newFW+0x3fdfe)) {
		text("NDS file is corrupt.");
		sprintf(s,"%x %x %x %x",(u32)newFW,*(u32*)newFW,*(u32*)(newFW+0x3fdfc),crc);
		text(s);
		stop();
	}

	// Attempt to patch the firmware buffer before calculating final headers or flashing
	patch_firmware_ui(newFW);

	bios_firmware_read(0,oldFW,0x40000);

	//check original FW version
	crc=*(u16*)(oldFW+0x17e);
	if(crc==0xffff)
		crc=*(u16*)(oldFW+6);
	switch(crc) {
		case NDS_VER1: orig_fw_ver=1; break;
		case NDS_VER2: orig_fw_ver=2; break;
		case NDS_VER3: orig_fw_ver=3; break;
		case NDS_VER4: orig_fw_ver=4; break;
		case NDS_VER5: orig_fw_ver=5; break;
		case NDS_VER6: orig_fw_ver=6; break;
		case NDS_VER7: orig_fw_ver=7; break;
		default: orig_fw_ver=0; break;
	}
	if(!orig_fw_ver) 
	{
		text("----------   WARNING   ---------");
		text("Your firmware is not recognized.");
		text("If you continue, it may not");
		text("be possible to restore your");
		text("firmware to its original state.");
		sprintf(s,"(%04X)",crc);
		text(s);
		
		for(i=0;i<60*5;i++)
			VBlankWait();
//		text("");
		text("Push START to continue anyway.");
		do { k=getkey(); } while(k!=KEY_START);
		line=0;
	}

	if(!lite)
		text("FLASHME INSTALLER");
	else
		text("FLASHME INSTALLER (DS Lite)");
	text("--------------------------------");
	text("If power is lost before flashing");
	text("completes, your NDS may be");
	text("permanently damaged. You should");
	text("plug in a power source before");
	text("proceeding.");
	text("");

	//print version info
	if(orig_fw_ver) {
		sprintf(origstr,"NDS-V%i",orig_fw_ver);
	} else {
		strcpy(origstr,"?");
	}
	if(oldFW[MAJOR_VER]==2) {
		old_fw_ver=oldFW[MINOR_VER]+3;
		sprintf(oldstr,"FM-V%i",old_fw_ver);
	} else if(oldFW[MAJOR_VER]==1) {
		strcpy(oldstr,"FM-??");
	} else if(oldFW[MAJOR_VER]==0xff) {
		strcpy(oldstr,origstr);
	} else if(oldFW[MAJOR_VER]==3) {
		strcpy(oldstr,"HB");
	} else {
		strcpy(oldstr,"?");
	}
	new_fw_ver=newFW[MINOR_VER]+3;
	textN("--------------------------------",22);
	sprintf(s,"New:FM-V%i Cur:%s Orig:%s",new_fw_ver,oldstr,origstr);
	textN(s,23);
	
//copy header stuff
	for(i=0;i<(0x170-0x28);i++) {
		newFW[i+0x28]=oldFW[i+0x28];
		newFW[i+BACKUPHEADER+0x28]=oldFW[i+0x28];
	}
	*(u16*)(newFW+0x17e)=crc;
	*(u16*)(newFW+BACKUPHEADER+0x17e)=crc;

//prompt, etc

	text("Push X B X B to Install.");
	text("Push SELECT for Mem Viewer.");
	seq=0;
	do {
		VBlankWait();
		k=getkey();

		if (k & KEY_SELECT) {
			text("Starting Memory Viewer...");
			smalldelay();
			fifo_send(99); // Tell ARM9 to start memdump
			stop();        // Halt ARM7
		}

		if(k) {
			if(k==keysequence[seq])
				seq++;
			else
				seq=0;
		}
	} while(seq<4);	
	text("If progress stops, the SL1");
	text("terminal needs to be shorted.");
	text("Please keep it shorted until");
	text("flashing is finished.");
	text("");
	text("Progress:   0%");

//start the magic
	i=0;
	do {
		if(!flash_verify(newFW+i,oldFW+i,i)) {
			smalldelay();
			HEARTBEAT++;
			if(++sfx_timeout>6) {
				playsound(SFX_STEP);
				sfx_timeout=0;
			}				
		} else {
			i+=0x100;
			HEARTBEAT=0;
			if(++sfx_timeout>3) {
				playsound(SFX_STEP);
				sfx_timeout=0;
			}				
		}
		PROGRESS=i;
	} while(i<0x3f800);

//all done
	smalldelay(); 	playsound(SFX_BARREL);
	smalldelay();	playsound(SFX_STEP);
	smalldelay(); 	playsound(SFX_JUMP);
	smalldelay(); 	playsound(SFX_BARREL);
	smalldelay();	playsound(SFX_STEP);
	smalldelay(); 	playsound(SFX_JUMP);
	smalldelay(); 	playsound(SFX_BARREL);

	text("");
	text("Firmware flashing completed");
	text("successfully. It is safe to");
	text("turn off your DS.");

//	fifo_send(99);
//	bios_SPI_write(SPI_POWER,0,1);
//	bios_SPI_write(SPI_POWER,PM_BOTTOMLIGHT|PM_TOPLIGHT|PM_SOUND,0);
	stop();
	return 0;
}