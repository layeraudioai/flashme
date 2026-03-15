#include <NDS/NDS.h>
#include <string.h>
#include <DS_misc.h>

extern u16 font;
extern u16 fontpal;

void gfx_init() {
	int i;
	u16 *p1=(u16*)0x6000000;
	u16 *p2=(u16*)0x2000000;
	
	POWER_CR=3;				//POWER LCDs, POWER MAIN 2D
	VRAM_CR=0x00000081;		//A->6000000
	
	for(i=0;i<32*24;i++) {
		*(p1++)=' ';	//screen clear
		*(p2++)=' ';	//screen buffer clear
	}

	// NOTE: The swiFastSet calls are disabled because the project is missing the
	// real font.bin and fontpal.bin assets. Linking the dummy data from data.c
	// causes an out-of-bounds read that crashes the ARM9.
	// swiFastSet(&font,(u16*)(CHAR_BASE_BLOCK(1)+32*32),1024*3,0);
	// swiFastSet(&fontpal,(u16*)BG_PALETTE,64,0);
 
	DISPLAY_CR=MODE_0_2D|DISPLAY_BG0_ACTIVE;
	BG0_CR=BG_TILE_BASE(1);
	BG0_X0=0;
	BG0_Y0=0;
	BLEND_CR=0;
}

int fifo_recv (u32 *data_buf) {
    if (REG_FIFO_CNT & FIFO_CNT_ERR ) {
		REG_FIFO_CNT = FIFO_CNT_SEND_CLEAR | FIFO_CNT_SEND_IRQ | FIFO_CNT_RECV_IRQ | FIFO_CNT_ENABLE |FIFO_CNT_ERR;
		return FAIL_RECV_ERR;
    }
    if(REG_FIFO_CNT & FIFO_CNT_RECV_EMPTY)
		return FAIL_RECV_EMPTY;
	*data_buf = REG_FIFO_RECV;
	return SUCCESS;
}

int loop_exit=0;
void FifoInterrupt() {
	int i;
	u16 *p1,*p2;
	u32 incoming;
	int res;
	do {
		res=fifo_recv(&incoming);
		if(res==SUCCESS && incoming<24) {
			p1=(u16*)(0x2000000+incoming*64);
			p2=(u16*)((u32)p1|0x6000000);
			for(i=0;i<32;i++) p2[i]=p1[i];
		}
		if(res==SUCCESS && incoming==99)
			loop_exit=1;
	} while(res==SUCCESS);
}

static void VblankInterrupt() {
	u16 *p;
	u32 d=*(u32*)0x2003ff8;
	if(!d) return;		//don't show anything til we get past 0
	p=(u16*)(SCREEN_BASE_BLOCK(0)+15*64+20);
	d=d/(0x3f800/100);
	if(!d) d++;			//show at least 1%
	if(d>99) p[0]='1';	//100% (extra digit)
	if(d>9) p[1]='0'+(d/10)%10;
	p[2]='0'+d%10;
	p[3]=(*(u32*)0x2003ffc)&1?' ':'%';	//blink (heartbeat)

}

static void interrupthandler() {
	u32 flags=IF&IE;
	VBLANK_INTR_WAIT_FLAGS|=flags;
	if(flags&IRQ_VBLANK)
		VblankInterrupt();
	if(flags&IRQ_FIFO_RECV)
		FifoInterrupt();
	IF=flags;				//irq ack
}

extern u8 firmware[];
extern u8 firmware_lite[];
void memdump(void);
int main(void) {

	IME=0;
	DISP_SR=DISP_VBLANK_IRQ;
	IRQ_HANDLER=interrupthandler;
	IE=IRQ_VBLANK|IRQ_FIFO_RECV;
	IF=~0;
	IME=IME_ENABLED;

	*(u8**)0x2004000=firmware;
	*(u8**)0x2004004=firmware_lite;
	
	gfx_init();
	fifo_init();
	FIFO_INTR=0x900;
	while((FIFO_INTR&15)!=7);	//wait for other cpu

	while(!loop_exit) swiSleep();
	memdump();
	return 0;
}
