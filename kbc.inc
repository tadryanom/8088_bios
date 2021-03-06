;=========================================================================
; kbc.inc - Keyboard controller support
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

;=========================================================================
; Keyboard controller defines - ports, commands, and flags
;-------------------------------------------------------------------------
kbc_data_reg	equ	60h
kbc_input_reg	equ	60h
kbc_output_reg	equ	60h
; keyboard controller return codes
kbc_ret_test	equ	0AAh	; keyboard controller test passed
; keyboard controller status register and its bits
kbc_status_reg	equ	64h
kbc_stat_obf	equ	01h	; output buffer full flag
kbc_stat_ibf	equ	02h	; input buffer full flag
kbc_stat_aobf	equ	20h	; auxiliary output buffer full flag
; keyboard contoller command register and commands
kbc_command_reg	equ	64h
kbc_cmd_wr_ctr	equ	60h	; write control register command
kbc_cmd_aux_dis	equ	0A7h	; disable auxiliary interface command
kbc_cmd_aux_ena	equ	0A8h	; enable auxiliary interface command
kbc_cmd_aux_tst	equ	0A9h	; test auxiliary interface command
kbc_cmd_test	equ	0AAh	; keyboard controller self-test command
kbc_cmd_kbd_tst	equ	0ABh	; test keyboard interface command
kbc_cmd_kbd_dis	equ	0ADh	; disable keyboard interface command
kbc_cmd_kbd_ena	equ	0AEh	; enable keyboard interface command
kbc_cmd_rd_in	equ	0C0h	; read keyboard input port
kbc_cmd_aux_snd	equ	0D4h	; send command byte to auxiliary device command
; keyboard controller control register bits
kbc_ctr_kbd_int	equ	01h	; enable keyboard OBF interrupt
kbc_ctr_aux_int	equ	02h	; enable auxiliary OBF interrupt
kbc_ctr_no_lock	equ	08h	; ignore keyboard inhibit (keyboard lock)
kbc_ctr_kbd_dis	equ	10h	; disable keyboard interface
kbc_ctr_aux_dis equ	20h	; disable auxiliary interface
kbc_ctr_xlat	equ	40h	; enable keyboard scancode translation
; keyboard controller input port bits
kbc_in_display	equ	40h	; input port bit 6: 0 = MDA, 1 = CGA

;=========================================================================
; kbc_kb_send - send command to keyboard, wait for acknowledge
; Input:
;	AL = command
; Output:
;	none
;-------------------------------------------------------------------------
kbc_kb_send:
	push	ax
	push	cx
	mov	ah,al			; save command to AH
	mov	cx,3			; try 3 times

.1:
	push	cx
	cli
; clear the Error, Acknowledge received, and resend received flags
	and	byte [kbd_flags_4],4Fh
	xor	cx,cx

.2:					; wait for KBC to empty input buffer
	in	al,kbc_status_reg
	test	al,kbc_stat_ibf
	loopnz	.2

	mov	al,ah
	out	kbc_output_reg,al	; send command to the keyboard
	sti
	xor	cx,cx
.3:					; wait for acknowledge (set by IRQ1 ISR)
	test	byte [kbd_flags_4], 10h	; acknowledge bit set?
	loopz	.3
	pop	cx
	jnz	.4
	loop	.1			; try again
; if the operation failed after 3 retries, set the error bit and quit
	or	byte [kbd_flags_4], 80h
.4:
	pop	cx
	pop	ax
	ret

;=========================================================================
; kbc_send_cmd - send command + argument to keyboard controller
; Input:
;	AL - command byte
;	AH = argument
; Output:
;	ZF == 0 - success
;	ZF == 1 - error
;-------------------------------------------------------------------------
kbc_send_cmd:
	push	cx
	mov	ch,al			; save command byte to CH
	mov	cl,30			; 30 retries
.1:
	cli
	mov	al,ch
	call	kbc_send_cmd_byte
	jnz	.exit			; time out
	in	al,kbc_status_reg
	test	al,kbc_stat_obf
	jz	.3			; output buffer is empty
	test	al,kbc_stat_aobf
	jz	.2			; output buffer is full, not aux data
	in	al,kbc_output_reg	; clean up auxiliary data from buffer
%ifdef PS2_MOUSE
	and	byte [mouse_flags_1],0F8h ; reset the mouse data index
%endif ; PS2_MOUSE
	jmp	.3
.2:
	sti
	dec	cl
	jnz	.1
	jmp	.exit			; note: ZF=1
.3:
	mov	al,ah
	out	kbc_input_reg,al
	sti
	or	cl,1			; set ZF=0
.exit:
	mov	al,ch			; restore AL
	pop	cx
	ret

;=========================================================================
; kbc_send_cmd_byte - send command byte to keyboard controller
; Input:
;	AL - command byte
; Output:
;	ZF == 1 - success
;	ZF == 0 - time out
; Note:
;	XXX - function should be reused in keyboard code
;-------------------------------------------------------------------------
kbc_send_cmd_byte:
	push	cx
	push	ax
	xor	cx,cx
				; wait for KBC to empty input buffer
.1:
	in	al,kbc_status_reg
	test	al,kbc_stat_ibf
	loopnz	.1
	jnz	.exit		; time out

	pop	ax
	out	kbc_command_reg,al

	push	ax
	xor	cx,cx
				; wait for KBC to empty input buffer
.2:
	in	al,kbc_status_reg
	test	al,kbc_stat_ibf
	loopnz	.2
.exit:
	pop	ax
	pop	cx
	ret

;=========================================================================
; kbc_wait_output_full - wait for data in keyboard controller output buffer
; Input:
;	none
; Output:
;	AL = keyboard status register
;	ZF == 0 - data is available
;	ZF == 1 - timed out
; Note:
;	XXX - if this function won't be reused anywhere else, it should
;	      be merged with kbc_wait_aux_full
;-------------------------------------------------------------------------
kbc_wait_output_full:
	push	cx
	xor	cx,cx
.1:
	in	al,kbc_status_reg
	test	al,kbc_stat_obf
	loopz	.1
	or	cx,cx
	pop	cx
	ret

%ifdef PS2_MOUSE
;=========================================================================
; kbc_aux_read - read data from auxiliary device
; Input:
;	none
; Output:
;	AL = data
;	CF == 0 - data is available
;	CF == 1 - time out
;-------------------------------------------------------------------------
kbc_aux_read:
	push	cx
;	xor	cx,cx			; XXX too much?!
	mov	cx,20			; retry 20 times
.1:
	call	kbc_wait_aux_full
	jnz	.2			; if ZF=0 - data is available
	loopz	.1
	jmp	.error			; time out
.2:
	mov	cx,1
	call	delay_15us
	in	al,kbc_output_reg
	clc
	jmp	.exit
.error:
	stc
.exit:
	pop	cx
	ret

;=========================================================================
; kbc_aux_send - send command to auxiliary device, wait for acknowledge
; Input:
;	AL = command
; Output:
;	AH - status:
;		00h - success
;		03h - interface error (time out)
;		04h - resend requested
;	CF == 0 - no error
;	CF == 1 - error
;-------------------------------------------------------------------------
kbc_aux_send:
	push	cx
	mov	ah,al			; store command to AH
	mov	al,kbc_cmd_aux_snd	; write byte to auxiliary device
	call	kbc_send_cmd
	jz	.timeout		; kbc_send_cmd timed out

					; wait for acknowledge
	mov	cx,10			; retry 10 times
.1:
	call	kbc_wait_aux_full
	jnz	.2			; if ZF=0 - data is available
	loop	.1
	jmp	.timeout		; no reply - timeout
.2:
	in	al,kbc_output_reg
	cmp	al,0FAh			; ACK?
	je	.ok
	cmp	al,0FEh			; resend?
	je	.resend
	cmp	al,0FCh			; error?
	je	.timeout		; treat as timeout/interface error
	loop	.1
	jmp	.timeout

.ok:
	xor	al,al			; success - ACK received
	clc
	jmp	.exit

.timeout:
	mov	al,03h			; interface error
	jmp	.error

.resend:
	mov	al,04h			; resend

.error:
	stc

.exit:
	xchg	ah,al			; status to AH, original command to AL
	pop	cx
	ret

;=========================================================================
; kbc_aux_enable - enable auxiliary device
; Input:
;	none
; Output:
;	ZF = 0 - no error
;	ZF = 1 - error
;-------------------------------------------------------------------------
kbc_aux_enable:
	push	ax
	mov	al,kbc_cmd_wr_ctr	; write controller command byte
	mov	ah,01000111b		; pc compatible, enable aux
					; enable keyboard, enable aux obf
					; interrupt, enable obf interrupt
	call	kbc_send_cmd
	pop	ax
	ret

;=========================================================================
; kbc_aux_disable - disable auxiliary device
; Input:
;	none
; Output:
;	ZF = 0 - no error
;	ZF = 1 - error
;-------------------------------------------------------------------------
kbc_aux_disable:
	push	ax
	mov	al,kbc_cmd_wr_ctr	; write controller command byte
	mov	ah,01100101b		; pc compatible, disable aux
					; enable keyboard, disable aux obf
					; interrupt, enable obf interrupt
	call	kbc_send_cmd
	pop	ax
	ret

;=========================================================================
; kbc_wait_aux_full - wait for data in keyboard controller auxiliary buffer
; Input:
;	none
; Output:
;	AL = keyboard status register
;	ZF == 0 - data is available
;	ZF == 1 - timed out
;-------------------------------------------------------------------------
kbc_wait_aux_full:
	call	kbc_wait_output_full
	jz	.error
	test	al,kbc_stat_aobf
.error:
	ret

%endif ; PS2_MOUSE

;=========================================================================
; kbc_flush - flush all data from i8042 buffers
; Input:
;	none
; Output:
;	CF = 0 - flushed successfully
;	CF = 1 - can't flush after 16 retries, probably no hardware
;-------------------------------------------------------------------------
kbc_flush:
	push	ax
	push	cx
	mov	cx,20			; maximal KBC buffer size
.flush_next_byte:
	in	al,kbc_status_reg
	test	al,kbc_stat_obf
	jz	.flushed
	push	cx
	mov	cx,4
	call	delay_15us		; 45-60us I/O delay
	pop	cx
	in	al,kbc_data_reg
	loop	.flush_next_byte
	stc				; unable to flush it
	jmp	.exit
.flushed:
	clc
.exit:
	pop	cx
	pop	ax
	ret

;=========================================================================
; kbc_init - Initialize keyboard controller
;-------------------------------------------------------------------------
kbc_init:
	push	ax
	push	cx
	push	dx

;-------------------------------------------------------------------------
; test keyboard controller

	call	kbc_flush		; flush all data from KBC

	mov	cx,10			; try 10 times

.kbc_reset_retry:
	mov	al,kbc_cmd_test		; send KBC self test command
	call 	kbc_send_cmd_byte
	call	kbc_wait_output_full	; wait for response

	in	al,kbc_output_reg
	cmp	al,55h			; check for success
	loopne	.kbc_reset_retry
	jne	kbd_ctrl_fail

	mov	cx,10			; try 10 times

.kbc_test_retry:
	mov	al,kbc_cmd_kbd_tst	; send test keyboard interface command
	call	kbc_send_cmd_byte
	call	kbc_wait_output_full	; wait for test result

	in	al,kbc_output_reg
	cmp	al,0			; check for success
	loopne	.kbc_test_retry
	jne	kbd_int_fail

;-------------------------------------------------------------------------
; read display type and set equipment bits accordingly

	mov	al,kbc_cmd_rd_in	; send read input port command
	call	kbc_send_cmd_byte
	call	kbc_wait_output_full

	in	al,kbc_output_reg
	test	al,kbc_in_display
	jnz	.get_disp_color		; input port bit 6 set => CGA display
	or	word [equipment_list],equip_mono
	jmp	.get_disp_done
.get_disp_color:
	or	word [equipment_list],equip_color
.get_disp_done:

;-------------------------------------------------------------------------
; initialize keyboard controller

	mov	al,kbc_cmd_kbd_ena	; send enable keyboard interface cmd
	call	kbc_send_cmd_byte

	mov	al,kbc_cmd_aux_ena	; send enable auxiliary interface cmd
	call	kbc_send_cmd_byte

	mov	al,kbc_cmd_wr_ctr	; send "write keyboard controller" cmd
	mov	ah,01101001B		; Bit 7 = 0 - reserved
					; Bit 6 = 1 - IBM PC scancodes
					; Bit 5 = 1 - IBM PC / no party check
					; Bit 4 = 0 - Enable keyboard
					; Bit 3 = 1 - Disable inhibit
					; Bit 2 = 0 - system flag = 0
					; Bit 1 = 0 - reserved
					; Bit 0 = 1 - enable OBF interrupt
	call	kbc_send_cmd

	mov	al,0FFh			; send keyboard reset command
	call	kbc_send_cmd_byte

	mov	cx,1000h
	call	delay_15us

	call	kbc_wait_output_full	; wait for response
	in	al,kbc_output_reg	; clear the output buffer


%ifdef PS2_MOUSE
;-------------------------------------------------------------------------
; check for PS/2 mouse presence

	mov	cx,10			; try 10 times
.mouse_reset_retry:
	mov	al,0FFh
	call	kbc_aux_send
	jnc	.mouse_reset_ok		; no error - continue
	cmp	ah,03h			; timeout error?
	loopz	.mouse_reset_retry
	jmp	.no_mouse
.mouse_reset_ok:
	call	kbc_aux_read
	jc	.no_mouse
	cmp	al,0AAh			; Basic Assurance Test successful?
	jne	.no_mouse
	call	kbc_aux_read
	jc	.no_mouse
; mouse reset successful, update equipment word accordingly
	or      word [equipment_list],equip_mouse
.no_mouse:
%endif ; PS2_MOUSE

;-------------------------------------------------------------------------
; setup keyboard buffer

        mov     ax,kbd_buffer   ; setup keyboard buffer
        mov     word [kbd_buffer_start],ax
        mov     word [kbd_buffer_head],ax
        mov     word [kbd_buffer_tail],ax
        add     ax,20h          ; size of the keyboard buffer
        mov     word [kbd_buffer_end],ax
        xor     ax,ax           ; clear keyboard flags
        mov     word [kbd_flags_1],ax
        mov     word [kbd_flags_2],ax
        mov     word [kbd_flags_3],ax
        mov     word [kbd_flags_4],ax
	mov	al,e_kbd_ok
	out	post_reg,al
	pop	dx
	pop	cx
	pop	ax
	ret

kbd_ctrl_fail:
	mov	al,e_kbd_ctrl_fail
	out	post_reg,al
.1:
	hlt
	jmp	.1

kbd_int_fail:
	and	al,0Fh
	add	al,e_kbd_int_fail
	out	post_reg,al
.1:
	hlt
	jmp	.1

kbd_key_fail:
	mov	al,e_kbd_key_fail
	out	post_reg,al
.1:
	hlt
	jmp	.1
