	AREA arm7boot, CODE, READONLY, ALIGN=4
	ENTRY

	IMPORT |Image$$ZI$$Base|
	IMPORT |Image$$ZI$$Limit|

	IMPORT arm7main
	
	EXPORT bios_firmware_read
	EXPORT bios_SPI_write
;----------------------------------------------
arm7init ;load address is 2380000

	mov	r0,#0x04000000
	add	r0,r0,#0x208
	strh r0,[r0]						;IME=0

	msr	cpsr_c,#0x13
	ldr	sp,=0x3810000-0x40				;set SVC (swi) stack
	msr	cpsr_c,#0x12
	ldr	sp,=0x3810000-0x80				;set IRQ stack
	msr	cpsr_c,#0x1f
	ldr	sp,=0x3810000-0x100				;set user stack

	adr r1,arm7init					;src
	ldr r2,=arm7init				;dst
	ldr r3,=|Image$$ZI$$Base|		;end
_y	cmp r2,r3
	ldrcc r0,[r1],#4
	strcc r0,[r2],#4
	bcc _y

	mov r0,#0
	ldr r2,=|Image$$ZI$$Base|
	ldr r3,=|Image$$ZI$$Limit|
_2	cmp r2,r3
	strcc r0,[r2],#4
	bcc _2
 
	ldr r3,=arm7main
	bx r3
;---------------------------------------------
bios_firmware_read
	ldr r3,=0x2437
	bx r3
bios_SPI_write
	ldr r3,=0x2369
	bx r3
;---------------------------------------------
	LTORG
	EXPORT sfx_sampletable
sfx_sampletable
	dcd 0
	dcd sfx_barrel
	dcd sfx_jump
	dcd sfx_step
	dcd sfx_end
	ALIGN 16
sfx_barrel
	INCBIN sfx/barrel.pcm
	ALIGN 16
sfx_jump
	INCBIN sfx/jump.pcm
	ALIGN 16
sfx_step
	INCBIN sfx/step.pcm
sfx_end	
	END