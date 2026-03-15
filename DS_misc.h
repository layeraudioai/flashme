#if defined(__thumb__)
	static inline void swiFastSet(const void* src, void* dst, u32 size, u32 fill) {
		u32 r2 = (size/4) | (fill << 24);
		__asm__ volatile (
			"mov r0, %0\n\t"
			"mov r1, %1\n\t"
			"mov r2, %2\n\t"
			"swi 0x0C\n\t"
			: /* no outputs */
			: "r"(src), "r"(dst), "r"(r2) 
			: "r0", "r1", "r2", "r3", "memory"
		);
	}

	static inline void swiSleep(void) {
		__asm__ volatile ("swi 0x06" ::: "r0", "r1", "r2", "r3", "memory");
	}

	static inline void swiWaitForVBlank(void) {
		__asm__ volatile ("swi 0x05" ::: "r0", "r1", "r2", "r3", "memory");
	}

	static inline void swiDelay(u32 duration) {
		__asm__ volatile ("mov r0, %0; swi 0x03" :: "r"(duration) : "r0", "r1", "r2", "r3", "memory");
	}

	static inline u16 swiCRC(u16 crc, void * data, u32 size) {
		u16 result;
		__asm__ volatile (
			"mov r0, %1\n\t"
			"mov r1, %2\n\t"
			"mov r2, %3\n\t"
			"swi 0x0E\n\t"
			"mov %0, r0"
			: "=r"(result)
			: "r"(crc), "r"(data), "r"(size)
			: "r0", "r1", "r2", "r3", "memory"
		);
		return result;
	}
#endif

#define swap(a,b) {__asm{swp a,a,[b]}}
#define vramSetBanks(a,b,c,d) (VRAM_CR=0x80808080|(a)|(b<<8)|(c<<16)|(d<<24))

////////////////////////////
// SPI
////////////////////////////

#define SPI_EN		0x8000
#define SPI_I		0x4000
#define SPI_BUSY	0x0080
#define SPI_SEL		0x0800
#define SPI_POWER	0
#define SPI_FW		0x0100
#define SPI_TOUCH	0x0200
#define SPI_MIC		0x0300

#define PM_SOUND		0x01
#define PM_BOTTOMLIGHT	0x04
#define PM_TOPLIGHT		0x08
#define PM_WIFI			0x10
#define PM_OFF			0x40

/////////////////////////
//FIFO
//////////////////////////

#define IRQ_FIFO_SEND 0x20000
#define IRQ_FIFO_RECV 0x40000

#define FIFO_INTR				*(vu16*)0x4000180
#define REG_FIFO_CNT            *(vu16*)0x4000184
#define REG_FIFO_SEND           *(vu32*)0x4000188
#define REG_FIFO_RECV           *(vu32*)0x4100000

#define FIFO_INTR_I                  0x4000
#define FIFO_INTR_IREQ               0x2000
#define FIFO_INTR_STATUS_OUT         0x0f00
#define FIFO_INTR_STATUS_IN          0x000f

#define FIFO_CNT_ENABLE                  0x8000
#define FIFO_CNT_ERR                     0x4000
#define FIFO_CNT_RECV_RI                 0x0400
#define FIFO_CNT_RECV_FULL               0x0200
#define FIFO_CNT_RECV_EMPTY              0x0100
#define FIFO_CNT_SEND_CLEAR              0x0008
#define FIFO_CNT_SEND_TI                 0x0004
#define FIFO_CNT_SEND_FULL               0x0002
#define FIFO_CNT_SEND_EMPTY              0x0001

#define FIFO_CNT_SEND_IRQ FIFO_CNT_SEND_TI
#define FIFO_CNT_RECV_IRQ FIFO_CNT_RECV_RI

enum errmsg {FAIL_SEND_FULL,FAIL_SEND_ERR,FAIL_RECV_EMPTY,FAIL_RECV_ERR,SUCCESS};

static inline void fifo_init() {
	//(void)OS_SetIrqFunction( OS_IE_FIFO_SEND, PXIi_HandlerSendFifoEmpty );
	//(void)OS_EnableIrqMask( OS_IE_FIFO_SEND );
	//(void)OS_SetIrqFunction( OS_IE_FIFO_RECV, fifo_recv_handler);
	//(void)OS_EnableIrqMask( OS_IE_FIFO_RECV );

	REG_FIFO_CNT =	FIFO_CNT_SEND_CLEAR | FIFO_CNT_RECV_RI | FIFO_CNT_ENABLE | FIFO_CNT_ERR;	//unknown
}
/*
static inline int fifo_send(u32 data) {
    if (REG_FIFO_CNT & FIFO_CNT_ERR) {
		REG_FIFO_CNT = FIFO_CNT_SEND_CLEAR | FIFO_CNT_SEND_TI | FIFO_CNT_RECV_RI | FIFO_CNT_ENABLE | FIFO_CNT_ERR;
		return FAIL_SEND_ERR;
    }
	if(REG_FIFO_CNT&FIFO_CNT_SEND_FULL)
		return FAIL_SEND_FULL;
	REG_FIFO_SEND = data;
	return SUCCESS;
}

static inline int fifo_recv (u32 *data_buf) {
    if (REG_FIFO_CNT & FIFO_CNT_ERR ) {
		REG_FIFO_CNT = FIFO_CNT_SEND_CLEAR | FIFO_CNT_SEND_TI | FIFO_CNT_RECV_RI | FIFO_CNT_ENABLE |FIFO_CNT_ERR;
		return FAIL_RECV_ERR;
    }
    if(REG_FIFO_CNT & FIFO_CNT_RECV_EMPTY)
		return FAIL_RECV_EMPTY;
	*data_buf = REG_FIFO_RECV;
	return SUCCESS;
}
*/
//////////////
// MISC
/////////////

#define EXCEPTION_HANDLER	*(u32*)0x27ffd9c

#define IWRAM_CNT *(u32*)0x40000247
#define IWRAM_99 0
#define IWRAM_77 3

///////////////
// CARD
///////////////

#define MCCNT0 *(vu16*)0x40001a0	//card / SPI ctrl
#define MCD *(vu16*)0x40001a2		//SPI data

/////////////
// SOUND
////////////

#define SOUNDCNT 			*(vu16*)0x4000500
#define SOUND0_VOL			*(vu16*)0x4000400
#define SOUND0_CR			*(vu16*)0x4000402
#define SOUND0_SRC			*(vu32*)0x4000404
#define SOUND0_TIMER		*(vu16*)0x4000408
#define SOUND0_REP_PT		*(vu16*)0x400040a
#define SOUND0_REP_LEN		*(vu32*)0x400040c