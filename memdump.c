#include <NDS/NDS.h>
#include <string.h>
#include <DS_misc.h>

#define vramSetBanks(a,b,c,d) (VRAM_CR=0x80808080|(a)|(b<<8)|(c<<16)|(d<<24))

extern u16 font;
extern u16 fontpal;

static void interrupthandler(void);

static int getkb() {
	static int oldkey=0;
	int k,x;
	k=READ_KEYS;
	x=(oldkey^k)&k;
	oldkey=k;
	return x;
}

static void gfx_init() {
	vramSetBanks(VRAM_A_MAIN_BG,0,VRAM_C_SUB_BG,0);

	DISPLAY_CR=MODE_0_2D|DISPLAY_BG0_ACTIVE;
	BG0_CR=BG_TILE_BASE(1);
	BG0_X0=0;
	BG0_Y0=0;
	BLEND_CR=0;
	SUB_DISPLAY_CR=MODE_0_2D|DISPLAY_BG0_ACTIVE;
	SUB_BG0_CR=BG_TILE_BASE(1);
	SUB_BG0_X0=0;
	SUB_BG0_Y0=0;
	BLEND_CR=0;

	// NOTE: The swiFastSet calls are disabled because the project is missing the
	// real font.bin and fontpal.bin assets. Linking the dummy data from data.c
	// causes an out-of-bounds read that crashes the ARM9.
	// swiFastSet(&font,(u16*)(CHAR_BASE_BLOCK(1)+32*32),1024*3,0);
	// swiFastSet(&fontpal,(u16*)BG_PALETTE,64,0);
	// swiFastSet(&font,(u16*)(CHAR_BASE_BLOCK_SUB(1)+32*32),1024*3,0);
	// swiFastSet(&fontpal,(u16*)BG_PALETTE_SUB,64,0);
}

static void hex32(int row,int d,int hi) {
	u16 *p;
	u16 c;
	int i;
	
	p=(u16*)(SCREEN_BASE_BLOCK_SUB(0)+row*64);
	for(i=7;i>=0;i--) {
		c=d&0x0f;
		if(c<10) c+='0';
		else c=c-10+'A';
		if(!hi) c|=0x1000;
		d>>=4;
		p[i]=c;
		hi--;
	}
}

static void dumpmem(u8 *addr) {
	u16 *p;
	u16 c;
	int i,j;
	u32 d;
	int hi=0;
	
	p=(u16*)(SCREEN_BASE_BLOCK_SUB(0)+64);
	for(j=0;j<23*16;j++) {
		d=addr[j];
		if(!(j&15)) hi++;
		for(i=1;i>=0;i--) {
			c=d&0x0f;
			if(c<10) c+='0';
			else c=c-10+'A';
			if(hi&1) c|=0x1000;
			d>>=4;
			p[i]=c;
		}
		p++;
		p++;
		hi++;
	}
}

static void clear_line(int row) {
	u16* p = (u16*)(SCREEN_BASE_BLOCK_SUB(0) + row * 64);
	int i;
	for (i = 0; i < 32; i++) {
		p[i] = ' ';
	}
}

static void print_text(int row, int col, const char* str) {
	u16* p = (u16*)(SCREEN_BASE_BLOCK_SUB(0) + row * 64 + col * 2);
	while (*str) {
		*p++ = (*str++);
	}
}

static int find_pattern(const u8 *data, u32 size, const u8 *pattern, u32 pat_len) {
	u32 i, j;
	if (!pat_len || !pattern || pat_len > size) return -1;
	for (i = 0; i <= size - pat_len; i++) {
		int match = 1;
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

static int hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_to_bytes(const char* hex_str, u8* out_buf, int max_bytes) {
    int len = strlen(hex_str);
    int i;
    int count = 0;
    if (len % 2 != 0) return 0; // Must be even length

    for (i = 0; i < len; i += 2) {
        int hi = hex_char_to_val(hex_str[i]);
        int lo = hex_char_to_val(hex_str[i+1]);
        if (hi == -1 || lo == -1) return 0; // Invalid hex char
        if (count >= max_bytes) break;
        out_buf[count++] = (hi << 4) | lo;
    }
    return count;
}

static int hex_input(char* buffer, int max_len) {
    const char hex_chars[] = "0123456789ABCDEF";
    int pos = 0;
    int len = 0;
    int k;

    buffer[0] = '\0';
    clear_line(3);

    while(1) {
        swiWaitForVBlank();

        // Draw UI
        print_text(3, 0, buffer);
        // Draw cursor
        u16* p = (u16*)(SCREEN_BASE_BLOCK_SUB(0) + 3 * 64 + pos * 2);
        *p |= 0x1000; // Highlight color

        k = getkb();

        if (k & (KEY_A | KEY_START)) return len;
        if (k & KEY_B) return 0;

        if (k & KEY_RIGHT) {
            if (pos < max_len - 1) pos++;
            if (pos >= len) { // Extend string if moving past end
                buffer[pos] = '\0';
                buffer[pos-1] = '0';
                len = pos;
            }
        }
        if (k & KEY_LEFT) {
            if (pos > 0) pos--;
        }

        if (k & KEY_UP) {
            int val = hex_char_to_val(buffer[pos]);
            if (val == -1) val = 0;
            val = (val + 1) % 16;
            buffer[pos] = hex_chars[val];
        }
        if (k & KEY_DOWN) {
            int val = hex_char_to_val(buffer[pos]);
            if (val == -1) val = 0;
            val = (val - 1 + 16) % 16;
            buffer[pos] = hex_chars[val];
        }

        // Clear old cursor and text
        clear_line(3);
    }
}

void memdump() {
	u32 addr=0x02000000; // Start at main RAM
	int k_pressed, k_held;
	
	char search_str[65] = {0};
	u8 search_pattern[32];
	int search_len_bytes = 0;
	
	powerON(POWER_ALL);

	*(vu8*)0x4000247=0;	//give arm9 wram access
		
	IME=0;
	DISP_SR=DISP_VBLANK_IRQ;
	IRQ_HANDLER=interrupthandler;
	IE=IRQ_VBLANK;
	IF=~0;
	IME=IME_ENABLED;

	gfx_init();
	print_text(2, 0, "Up/Dn/L/R=Scroll B+Scroll=Fast");
	print_text(3, 0, "L/R Trig=Page  Start=Search");
	
	// Initial Draw
	hex32(0,addr,8);
	dumpmem((u8*)addr);

	while(1) {
		swiWaitForVBlank();
		k_pressed = getkb();
		k_held = READ_KEYS;
		
		int needs_redraw = 0;
		u32 old_addr = addr;

		// --- Search ---
		if (k_pressed & KEY_START) {
			clear_line(2); clear_line(3);
			print_text(2, 0, "Search (Hex):");
			int input_len = hex_input(search_str, 64);
			if (input_len > 0 && (input_len % 2 == 0)) {
				search_len_bytes = hex_to_bytes(search_str, search_pattern, 32);
				// Immediately do first search
				k_pressed |= KEY_SELECT;
			} else {
				search_len_bytes = 0;
			}
			needs_redraw = 1;
		}

		if ((k_pressed & KEY_SELECT) && search_len_bytes > 0) {
			clear_line(2); clear_line(3);
			print_text(2, 0, "Searching...");
			swiWaitForVBlank();
			int offset = find_pattern((u8*)(addr + 1), 0x08000000 - (addr + 1), search_pattern, search_len_bytes);
			if (offset != -1) {
				addr = addr + 1 + offset;
				print_text(2, 0, "Found. Press SELECT for next.");
			} else {
				print_text(2, 0, "Not found from this address.");
			}
			needs_redraw = 1;
		}

		// --- Navigation ---
		if (k_held) {
			u32 step = (k_held & KEY_B) ? 0x100 : 1;
			if (k_held & KEY_UP) addr += 16 * step;
			if (k_held & KEY_DOWN) addr -= 16 * step;
			if (k_held & KEY_RIGHT) addr += 1 * step;
			if (k_held & KEY_LEFT) addr -= 1 * step;
		}
		if (k_pressed & KEY_R) addr += 23 * 16;
		if (k_pressed & KEY_L) addr -= 23 * 16;

		if (addr != old_addr) needs_redraw = 1;

		if(needs_redraw) {
			if (!((k_pressed & KEY_START) || (k_pressed & KEY_SELECT))) {
				clear_line(2); clear_line(3);
				print_text(2, 0, "Up/Dn/L/R=Scroll B+Scroll=Fast");
				print_text(3, 0, "L/R Trig=Page  Start=Search");
			}
			hex32(0,addr,8);
			dumpmem((u8*)addr);
		}
	}
}

static void interrupthandler() {
	u32 flags=IF&IE;
	VBLANK_INTR_WAIT_FLAGS|=flags;
//	if(flags&IRQ_VBLANK)
//		VblankInterrupt();
	IF=flags;				//irq ack
}
