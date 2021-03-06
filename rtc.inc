;=========================================================================
; rtc.inc - RTC/CMOS read and write functions
;-------------------------------------------------------------------------
;
; Compiles with NASM 2.07, might work with other versions
;
; Copyright (C) 2011 - 2014 Sergey Kiselev.
; Provided for hobbyist use on the Xi 8088 board.
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;
;=========================================================================

;-------------------------------------------------------------------------
; RTC ports
rtc_addr_reg	equ	70h	; RTC address port
rtc_data_reg	equ	71h	; RTC data port

;-------------------------------------------------------------------------
; locations in RTC and NVRAM
cmos_seconds	equ	00h	; seconds location in RTC
cmos_alarm_secs	equ	01h	; alarm seconds location in RTC
cmos_minutes	equ	02h	; minutes location in RTC
cmos_alarm_mins	equ	03h	; alarm minutes location in RTC
cmos_hours	equ	04h	; hours locaiton in RTC
cmos_alarm_hrs	equ	05h	; alarm hours location in RTC
cmos_day	equ	06h	; day location in RTC
cmos_date	equ	07h	; date location in RTC
cmos_month	equ	08h	; month location in RTC
cmos_year	equ	09h	; year location in RTC
cmos_floppy	equ	10h	; floppy type byte
cmos_equip	equ	14h	; equipment byte
cmos_sum_hi	equ	2Eh	; checksum of bytes 10h - 20h - high byte
cmos_sum_lo	equ	2Fh	; checksum of bytes 10h - 20h - low byte 
cmos_century	equ	32h	; centry location in RTC (DS12C887 only)

;-------------------------------------------------------------------------
; RTC control register and their bits
cmos_control_a	equ	0Ah	; RTC control A register
cmos_uip	equ	80h	; RTC update in progress bit
cmos_control_b	equ	0Bh	; RTC control B register
cmos_dse	equ	01h	; RTC daylight savings enable bit
cmos_24hours	equ	02h	; RTC 24 hours format (1 = 24 hours, 0 = 12)
cmos_uie	equ	10h	; RTC update ended interrupt enable bit
cmos_aie	equ	20h	; RTC alarm interrupt enable bit
cmos_pie	equ	40h	; RTC periodic interrupt enable bit
cmos_set	equ	80h	; RTC set bit (0 = normal operation, 1 = set)
cmos_control_c	equ	0Ch	; RTC control C register
cmos_uf		equ	20h	; RTC update ended interrupt flag
cmos_af		equ	40h	; RTC alarm interrupt flag
cmos_pf		equ	80h	; RTC periodic interrupt flag
cmos_control_d	equ	0Dh	; RTC control D register
cmos_vrt	equ	80h	; RTC vrt bit (1 = battery is OK)

;-------------------------------------------------------------------------
; NMI flag
nmi_disable	equ	7Fh	; disable NMI AND mask (bit 7 = 0)
nmi_enable	equ	80h	; enable NMI OR mask (bit 7 = 1)

;=========================================================================
; rtc_read - Read byte from RTC or CMOS memory
; Input:
;	AL - address and NMI enable bit
;		bits 6-0 - address of byte to read
;		bit 7    - 0 = disable NMI, 1 = enable NMI
; Output:
;	AL = byte from RTC
;-------------------------------------------------------------------------
rtc_read:
	cli
	out	rtc_addr_reg,al
	jmp	$+2
	jmp	$+2
	in	al,rtc_data_reg
	sti
	ret

;=========================================================================
; rtc_write - Read byte to RTC or CMOS memory
; Input:
;	AL - address and NMI enable bit
;		bits 6-0 - address of byte to read
;		bit 7    - 0 = disable NMI, 1 = enable NMI
;	AH = byte to write to RTC
;-------------------------------------------------------------------------
rtc_write:
	cli
	out	rtc_addr_reg,al
	jmp	$+2
	jmp	$+2
	xchg	ah,al
	out	rtc_data_reg,al
	xchg	ah,al
	sti
	ret

;=========================================================================
; rtc_init - Initialize RTC
; Notes:
;	- makes sure RTC battery is OK, resets time if not
;	- disables RTC interrupts
;	- validates NVRAM checksum, loads default values if invalid
;-------------------------------------------------------------------------
rtc_init:
	push	ax
	push	bx
	push	cx
	push	dx
	push	si
	push	di
	mov	al,cmos_control_a	; select control A register
	mov	ah,26h			; turn on oscillator and time keeping
					; set SQW frequency to 1.024 KHz
	call	rtc_write		; write control register A

	mov	al,cmos_control_b
	mov	ah,cmos_24hours		; 24 hours, BCD format, DSE disabled
					; interrupts disabled
	call	rtc_write		; write control register B

	mov	al,cmos_control_c
	call	rtc_read		; read control register C - reset
					; interrupt flags

	mov	al,cmos_control_d
	call	rtc_read		; read control register D
	test	al,cmos_vrt
	jnz	.1			; RTC battery is OK
	mov	si,msg_rtc_bad
	call	print
; RTC is bad, set initial time
	mov	ah,03h			; int 1Ah, function 03h - set RTC time
	xor	cx,cx
	xor	dx,dx
	int	1Ah
	mov	ah,05h			; int 1Ah, function 05h - set RTC date
	mov	cx,2010h		; year 2010
	mov	dx,0101h		; January 1st
	int	1Ah

.1:
; set timer variables to RTC time
	mov	ah,02h			; int 1Ah, function 02h - get RTC time
	int	1Ah

; convert time to ticks * 2^11

; ticks = seconds * 37287
	mov	al,dh
	call	bcd_to_binary		; convert seconds to binary

	mov	dx,37287
	mul	dx			; DX:AX = seconds * 37287

	mov	si,ax
	mov	di,dx

; ticks += minutes * 2237216 = minutes * 8992 + minutes * 34 * 2^16
	mov	al,cl
	call	bcd_to_binary		; convert minutes to binary

	mov	bx,ax
	mov	dx,8992
	mul	dx			; DX:AX = minutes * 8992

	add	si,ax
	adc	di,dx

	mov	ax,bx
	mov	dx,34
	mul	dx

	add	di,ax

; ticks += hours * 134232938 = hours * 15210 + hours * 2048 * 2^16
	mov	al,ch
	call	bcd_to_binary		; convert hours to binary

	mov	bx,ax
	mov	dx,15210
	mul	dx			; DX:AX = hours * 15210

	add	si,ax
	adc	di,dx

	mov	ax,bx
	mov	dx,2048
	mul	dx			; AX = hours * 2048

	add	di,ax

; CX:DX = DI:SI / 2048
	mov	cl,11
	shr	si,cl
	mov	dx,di
	mov	cl,5
	shl	dx,cl
	or	dx,si

	mov	cl,11
	shr	di,cl
	mov	cx,di

					; CX = high word of tick count
					; DX = low word of tick count
	
	mov	ah,01h			; int 1Ah, function 01h - set time
	int	1Ah	


; compare NVRAM checksum with stored value

	call	rtc_checksum

	mov	al,cmos_sum_hi
	call	rtc_read
	mov	ah,al
	mov	al,cmos_sum_lo
	call	rtc_read
	cmp	bx,ax
	je	.update_equipment

	mov	si,msg_rtc_sum
	call	print

; clear NVRAM

	mov	ax,0010h		; start from 10h, load 00h

.nvram_clear_loop:
	call	rtc_write
	inc	al
	cmp	al,20h			; last address is 20h
	jbe	.nvram_clear_loop

	mov	ax,4010h		; one 1.44M floppy disk
	call	rtc_write

	mov	ax,2114h		; color 80x25, one floppy disk
	call	rtc_write

; update checksum

	call	rtc_checksum

	mov	al,cmos_sum_hi
	mov	ah,bh
	call	rtc_write

	inc	al			; AL = cmos_sum_lo
	mov	ah,bl
	call	rtc_write

; read equipment byte from NVRAM and set it in BIOS data area

.update_equipment:
	mov	al,cmos_equip
	call	rtc_read
	and	al,~(equip_video|equip_mouse)   ; these are autodetected
	and	byte [equipment_list],equip_video|equip_mouse
	or	byte [equipment_list],al

	mov	al,e_rtc_init_ok	; RTC initialzied successfully
	out	post_reg,al

	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	ret

bcd_to_binary:
	push	cx
	mov	ch,al
	and	ch,0Fh
	mov	cl,4
	shr	al,cl
	mov	cl,10
	mul	cl
	add	al,ch
	pop	cx
	ret

;=========================================================================
; rtc_checksum - calculate NVRAM checksum
; Input:
;	none
; Output:
;	BX = NVRAM checksum
;-------------------------------------------------------------------------
rtc_checksum:
	push	ax
	xor	bx,bx
	mov	ah,10h			; start from 10h

.checksum_loop:
	mov	al,ah
	call	rtc_read
	add	bl,al			; BX += AL
	adc	bh,0
	inc	ah
	cmp	ah,20h			; last address is 20h
	jbe	.checksum_loop
	pop	ax
	ret

;=========================================================================
; rtc_setup - NVRAM configuration utility
; Input:
;	none
; Output:
;	none
;-------------------------------------------------------------------------
rtc_setup:
	push	ax
	push	bx
	push	cx
	push	dx
	push	si

	mov	al,cmos_floppy
	call	rtc_read		; read currently configured floppies
	mov	cl,al			; save it to CL
	mov	si,msg_set_welcome
	call	print

.menu_loop:
	mov	si,msg_set_prompt
	call	print

.menu_input:
	mov	ah,00h
	int	16h
	or	al,20h			; convert to lower case
	mov	dl,0			; floppy 0
	cmp	al,'f'
	je	.set_floppy
	inc	dl			; floppy 1
	cmp	al,'g'
	je	.set_floppy
	cmp	al,'p'
	je	.print_settings
	cmp	al,'t'
	je	.set_time
	cmp	al,'d'
	je	.set_date
	cmp	al,'w'
	je	.save
	cmp	al,'q'
	je	.exit
	cmp	al,'h'
	je	.help
	jmp	.menu_input

.help:
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h
	mov	si,msg_set_help
	call	print
	jmp	.menu_loop

.set_floppy:
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h

.set_floppy_prompt:
	mov	si,msg_set_fd_prmt
	call	print

.floppy_input:
	mov	ah,00h
	int	16h
	or	al,20h			; convert to lower case
	cmp	al,'r'
	je	.floppy_exit
	cmp	al,'h'
	je	.floppy_help
	cmp	al,'0'
	jb	.floppy_input
	cmp	al,'6'
	ja	.floppy_input
	cmp	al,'5'
	je	.floppy_input
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h
	sub	al,'0'
	mov	dh,0F0h
	or	dl,dl			; drive	0?
	jnz	.drive_1
	shl	al,1
	shl	al,1
	shl	al,1
	shl	al,1
	mov	dh,0Fh
.drive_1:
	and	cl,dh
	or	cl,al
	jmp	.menu_loop

.floppy_exit:
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h
	jmp	.menu_loop

.floppy_help:
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h
	mov	si,msg_set_fd_help
	call	print
	jmp	.set_floppy_prompt

.print_settings:
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h
	mov	si,msg_crlf
	call	print
	call	print_rtc		; print current RTC time
	mov	al,cl			; floppy bytes to AL
	call	print_floppy
	jmp	.menu_loop

.set_time:
	push	cx
	push	di
	push	es
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h
	mov	si,msg_crlf
	call	print

	mov	si,msg_set_time
	call	print

	mov	ah,02h
	int	1Ah			; get current time

	mov 	ax,07C0h		; use 07C0:0000 as a buffer
	mov	es,ax
	xor	di,di
	mov	ax,8			; length of the time string

	call	get_line

	or	ax,ax			; empty string?
	jz	.set_time_exit

	call	atoi

	cmp	ax,23			; hours are greater than 23?
	ja	.set_time_invalid

	call	bin_to_bcd		; convert to BCD

	mov	ch,al			; CH = hours

    es	cmp	byte [di],':'		; ':' delimiter?
	jne	.set_time_invalid

	inc	di

	call	atoi

	cmp	ax,59			; minutes are greater than 59?
	ja	.set_time_invalid

	call	bin_to_bcd

	mov	cl,al			; CL = minutes

    es	cmp	byte [di],':'		; ':' delimiter?
	jne	.set_time_invalid

	inc	di

	call	atoi

	cmp	ax,59			; seconds are greater than 59?
	ja	.set_time_invalid

	call	bin_to_bcd

	mov	dh,al			; DH = seconds

    es	cmp	byte [di],00h		; end of line?
	jne	.set_time_invalid

	mov	ah,03h			; set RTC time
	int	1Ah

.set_time_exit:
	pop	es
	pop	di
	pop	cx
	jmp	.menu_loop

.set_time_invalid:
	mov	si,msg_time_inval
	call	print
	jmp	.set_time_exit

.set_date:
	push	cx
	push	di
	push	es
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h
	mov	si,msg_crlf
	call	print
	mov	si,msg_set_date
	call	print

	mov 	ax,07C0h		; use 07C0:0000 as a buffer
	mov	es,ax
	xor	di,di
	mov	ax,10			; length of the date string

	call	get_line

	or	ax,ax			; empty string?
	jz	.set_date_exit

	call	atoi

	cmp	ax,9999			; year is greater than 9999?
	ja	.set_date_invalid

	mov	bx,ax			; store binary year in BX

	call	bin_to_bcd		; convert to BCD

	mov	cx,ax			; CH = century, CL = year
	
    es	cmp	byte [di],'-'		; '-' delimiter?
	jne	.set_date_invalid

	inc	di

	call	atoi

	cmp	ax,1			; month is lower than 1?
	jb	.set_date_invalid

	cmp	ax,12			; month is greater than 12?
	ja	.set_date_invalid

	call	bin_to_bcd

	mov	dh,al			; DH = month

    es	cmp	byte [di],'-'		; '-' delimiter?
	jne	.set_date_invalid

	inc	di

	call	atoi

	cmp	ax,1			; day is lower than 1?
	jb	.set_date_invalid

	cmp	dh,02h			; February
	je	.set_date_february

	cmp	dh,04h			; April
	je	.set_date_30_days

	cmp	dh,06h			; July
	je	.set_date_30_days

	cmp	dh,09h			; September
	je	.set_date_30_days

	cmp	dh,11h			; November
	je	.set_date_30_days

.set_date_31_days:
	cmp	ax,31			; day of month is greater than 31?
	ja	.set_date_invalid
	jmp	.set_date_set_day

.set_date_30_days:
	cmp	ax,30			; day of month is greater than 30?
	ja	.set_date_invalid
	jmp	.set_date_set_day

.set_date_29_days:
	cmp	ax,29			; day of month is greater than 28?
	ja	.set_date_invalid
	jmp	.set_date_set_day

.set_date_28_days:
	cmp	ax,28			; day of month is greater than 28?
	ja	.set_date_invalid
	jmp	.set_date_set_day

.set_date_february:
	test	bx,0003h		; check if years divides by 4
	jnz	.set_date_28_days	; not a leap year

	cmp	cl,00h			; year doesn't end with 00
	jne	.set_date_29_days	; a leap year

	test	ch,03h			; check if century divides by 4
	jz	.set_date_29_days	; year divides by 400
	jmp	.set_date_28_days	; year divides by 100 but not by 400

.set_date_set_day:
	call	bin_to_bcd

	mov	dl,al			; DL = date (day of month)

    es	cmp	byte [di],00h		; end of line?
	jne	.set_date_invalid

	mov	ah,05h			; set RTC date
	int	1Ah

.set_date_exit:
	pop	es
	pop	di
	pop	cx
	jmp	.menu_loop

.set_date_invalid:
	mov	si,msg_date_inval
	call	print
	jmp	.set_date_exit

.save:
	push	ax

	mov	al,cmos_floppy
	mov	ah,cl
	call	rtc_write

	mov	ah,byte [equipment_list]
	and	ah,03Eh			; mask floppy bits

	test	cl,70h
	jz	.second_floppy		; jump if first floppy is not installed
	or	ah,01h			; first floppy is installed

.second_floppy:
	test	cl,07h
	jz	.save_equipment		; jump if second floppy is not installed
	or	ah,41h			; indicate two floppies
					; (even if the first one is missing)

.save_equipment:
	mov	byte [equipment_list],ah
	mov	al,cmos_equip
	call	rtc_write

	call	rtc_checksum

	mov	al,cmos_sum_hi
	mov	ah,bh
	call	rtc_write

	inc	al			; AL = cmos_sum_lo
	mov	ah,bl
	call	rtc_write
	
	pop	ax

.exit:
	mov	ah,0Eh			; echo
	mov	bx,0007h
	int	10h
	mov	si,msg_crlf
	call	print
	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	ret

;=========================================================================
; print_rtc - print current RTC date and time
; Input:
;	none
; Output:
;	date and time are printed in YYYY-MM-DD hh:mm:ss format
; Notes:
;	There is a slight probability of getting inconsistent printout.
;	In case the function is called just before midnight, it could
;	print the previos day's date and next day's time.
;-------------------------------------------------------------------------
print_rtc:
	push	ax
	push	bx
	push	cx
	push	dx
	push	si
	mov	si,msg_rtc
	call	print

; print date
	mov	ah,04h
	int	1Ah			; read RTC date
					; CH = BCD century
					; CL = BCD year
					; DH = BCD month
					; DL = BCD date (day of month)
	mov	ax,cx
	call	print_hex		; print 4-digit year
	mov	ax,(0Eh << 8) + '-'
	mov	bx,0007h
	int	10h			; print dash (-)

	mov	al,dh
	call	print_byte		; print 2-digit month
	mov	ax,(0Eh << 8) + '-'
	mov	bx,0007h
	int	10h			; print dash (-)

	mov	al,dl
	call	print_byte		; print 2-digit date (day of month)

	mov	ax,(0Eh << 8) + ' '
	mov	bx,0007h
	int	10h			; print space ( )

; print time
	mov	ah,02h
	int	1Ah			; read RTC time
					; CH = BCD hours
					; CL = BCD minutes
					; DH = BCD seconds
					; DL = daylight saving flag (ignored)
	mov	al,ch
	call	print_byte		; print 2-digit hours
	
	mov	ax,(0Eh << 8) + ':'
	mov	bx,0007h
	int	10h			; print colon (:)

	mov	al,cl
	call	print_byte		; print 2-digit minutes
	
	mov	ax,(0Eh << 8) + ':'
	mov	bx,0007h
	int	10h			; print colon (:)

	mov	al,dh
	call	print_byte		; print 2-digit seconds

	mov	si,msg_crlf
	call	print
	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	ret

;=========================================================================
; delay_15us - delay for multiplies of 15 microseconds
; Input:
;	CX = time to delay (in 15 microsecond units)
; Notes:
;	1.  Actual delay will be between (CX - 1) * 15us and CX * 15us
;	2.  This relies on the "refresh" bit of port 61h and therefore on
;	    timer channel 1. Will not function properly if timer gets
;	    reprogrammed by an application or if it was not initialized yet
;-------------------------------------------------------------------------
delay_15us:
	push	ax
	push	cx
.zero:
	in	al,port_b_reg
	test	al,refresh_flag
	jz	.zero
	dec	cx
	jz	.exit
.one:
	in	al,port_b_reg
	test	al,refresh_flag
	jnz	.one
	dec	cx
	jnz	.zero
.exit:
	pop	cx
	pop	ax
	ret

%if 0
;=========================================================================
; divide_32 - divide 64-bit argument by 32-bit, return 64-bit result
; Input:
;	DX:AX	- dividend
;	CX	- divisor
; Output:
;	DX:AX	- quotient
;-------------------------------------------------------------------------
divide_32:
	or	dx,dx
	jnz	.1
	div	cx
	xor	dx,dx
	ret
.1:
	push	bx
	mov	bx,ax
	mov	ax,dx
	xor	dx,dx
	div	cx
	xchg	bx,ax
	div	cx
	mov	dx,bx
	pop	bx
	ret
%endif ; 0
