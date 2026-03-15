	AREA |.text|, CODE, READONLY
	
	EXPORT font
	EXPORT fontpal
	EXPORT firmware
	EXPORT firmware_lite
;--------------------------------
font
	INCBIN font.bin
fontpal
	INCBIN fontpal.bin
firmware
	INCBIN fwimage.bin
firmware_lite
	INCBIN fwimage_lite.bin
;--------------------------------
	END