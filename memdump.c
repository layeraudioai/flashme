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

	swiFastSet(&font,(u16*)(CHAR_BASE_BLOCK(1)+32*32),1024*3,0);
	swiFastSet(&fontpal,(u16*)BG_PALETTE,64,0);
	swiFastSet(&font,(u16*)(CHAR_BASE_BLOCK_SUB(1)+32*32),1024*3,0);
	swiFastSet(&fontpal,(u16*)BG_PALETTE_SUB,64,0);
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

void memdump() {
	int i;
	int k;
	int sel=0;
	int addr=0x2200000;
	
	powerON(POWER_ALL);

	*(vu8*)0x4000247=0;	//give arm9 wram access
		
	IME=0;
	DISP_SR=DISP_VBLANK_IRQ;
	IRQ_HANDLER=interrupthandler;
	IE=IRQ_VBLANK;//|IRQ_FIFO_RECV;
	IF=~0;
	IME=IME_ENABLED;

	gfx_init();
	k=KEY_UP;	
	while(1) {
		swiWaitForVBlank();
		if(k) {
			switch(k) {
				case KEY_A:
					i=sel?1:4;
					addr+=i<<((sel&7)*4);
					break;
				case KEY_B:
					i=sel?1:4;
					addr-=i<<((sel&7)*4);
					break;
				case KEY_LEFT:
					sel++;
					if(sel>=8) sel--;
					break;
				case KEY_RIGHT:
					sel--;
					if(sel<0) sel=0;
					break;
			}
			hex32(0,addr,sel);
			dumpmem((u8*)addr);
		}
		k=getkb();
	}
}

static void interrupthandler() {
	u32 flags=IF&IE;
	VBLANK_INTR_WAIT_FLAGS|=flags;
//	if(flags&IRQ_VBLANK)
//		VblankInterrupt();
	IF=flags;				//irq ack
}
