// #pragma arm section code="arm7code", rodata="arm7code", rwdata="arm7data", zidata="arm7zi"

#include <NDS/NDS.h>
#include <DS_misc.h>
#include <string.h>
#include "7.h"

int mini_sprintf(char *str, const char *fmt, ...);
#define sprintf mini_sprintf

void bios_firmware_read(u32 srcaddr,u8 *dstaddr,u32 size);
void bios_SPI_write(int device,int data,int select);

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

#define KEY_X 0x400
#define KEY_Y 0x800
int getkey() {
	static int oldkey;
	int k,x;
	k=READ_KEYS;
	k|=(~XKEYS&3)<<10;
	x=(oldkey^k)&k;
	oldkey=k;
	return x;
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

	text("Push X B X B to continue.");
	text("");
	seq=0;
	do {
		VBlankWait();
		k=getkey();
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