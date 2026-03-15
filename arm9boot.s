	AREA arm9boot, CODE, READONLY
	ENTRY

	IMPORT |Image$$ZI$$Base|
	IMPORT |Image$$ZI$$Limit|

	IMPORT arm9main	
;------------------------------------------------------------
	msr	cpsr_c,#0x13
	ldr	sp,=0x804000-0x40				;set SVC (swi) stack
	msr	cpsr_c,#0x12
	ldr	sp,=0x804000-0x80				;set IRQ stack
	msr	cpsr_c,#0x1f
	ldr	sp,=0x804000-0x100				;set user stack

	ldr r1,=0x00002078					;disable DTCM and protection unit
	mcr p15,0,r1,c1,c0
	ldr r0,=0x0080000A
	mcr p15,0,r0,c9,c1					;TCM base = 0x00800*4096, size = 16 KB
	mrc p15,0,r0,c1,c0					;throw-away read of cp15.c1
	orr r1,r1,#0x10000
	mcr p15,0,r1,c1,c0					;cp15.c1 = 0x00012078;

	mov r0,#0
	ldr r2,=|Image$$ZI$$Base|
	ldr r3,=|Image$$ZI$$Limit|
_1	cmp r2,r3
	strcc r0,[r2],#4
	bcc _1

	ldr pc,=arm9main
;-----------------
	END