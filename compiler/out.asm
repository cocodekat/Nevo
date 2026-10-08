.section __TEXT,__cstring,cstring_literals
_str0:
	.asciz "who are you: "
_str1:
	.asciz " is based\n"
_str2:
	.asciz " is cringe\n"
_str3:
	.asciz "spoon"
_str4:
	.asciz "brick"
_str5:
	.asciz "\n"

.section __DATA,__bss
_rt_buf:
	.space 33

.section __DATA,__data
	.p2align 3
_fnslot_main:
	.quad _main

.section __TEXT,__text
.globl _main

; ── runtime helper: int64 → decimal string ──────────────
; Input:  x0 = int64 value
; Output: x0 = pointer to null-terminated string in _rt_buf
; Clobbers: x1-x7
_int64_to_str:
	stp  x29, x30, [sp, #-16]!
	mov  x29, sp
	adrp x1, _rt_buf@PAGE
	add  x1, x1, _rt_buf@PAGEOFF   ; x1 = buf base

	; handle zero
	cbnz x0, _i2s_nonzero
	mov  w2, #48
	strb w2, [x1]
	mov  w2, #10
	strb w2, [x1, #1]              ; newline
	mov  w2, #0
	strb w2, [x1, #2]              ; null terminator
	mov  x0, x1
	ldp  x29, x30, [sp], #16
	ret

_i2s_nonzero:
	mov  x6, #0                    ; x6 = negative flag
	tbz  x0, #63, _i2s_positive
	mov  x6, #1
	neg  x0, x0
_i2s_positive:
	add  x3, x1, #30              ; x3 = write pointer (end)
	strb w4, [x1, #1]             ; newline at buf+31
	mov  w4, #0
	strb w4, [x3, #2]             ; null terminator at buf+32

_i2s_digit_loop:
	cbz  x0, _i2s_done_digits
	mov  x5, #10
	udiv x7, x0, x5               ; x7 = x0 / 10
	msub x4, x7, x5, x0           ; x4 = x0 % 10
	add  w4, w4, #48              ; to ASCII
	strb w4, [x3]
	sub  x3, x3, #1
	mov  x0, x7
	b    _i2s_digit_loop

_i2s_done_digits:
	cbz  x6, _i2s_no_neg
	mov  w4, #45                  ; '-'
	strb w4, [x3]
	sub  x3, x3, #1
_i2s_no_neg:
	add  x0, x3, #1               ; x0 = pointer to first digit
	ldp  x29, x30, [sp], #16
	ret

_main:
	stp  x29, x30, [sp, #-48]!
	mov  x29, sp
	adrp x8, _str0@PAGE
	add  x8, x8, _str0@PAGEOFF
	mov  x0, x8
	bl   _nevo_input
	mov  x8, x0
	str  x8, [x29, #16]
	bl _nevo_maybe
	mov x8, x0
	str  x8, [x29, #24]
	ldr  x8, [x29, #24]
	cbz  x8, _Ifalse0
	ldr  x8, [x29, #16]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str1@PAGE
	add  x0, x0, _str1@PAGEOFF
	bl   _nevo_print_text
	b    _Iend0
_Ifalse0:
	ldr  x8, [x29, #16]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str2@PAGE
	add  x0, x0, _str2@PAGEOFF
	bl   _nevo_print_text
_Iend0:
	mov x0, #1
	bl _nevo_array_new
	mov x8, x0
	str x8, [sp, #-16]!
	adrp x8, _str3@PAGE
	add  x8, x8, _str3@PAGEOFF
	mov x2, x8
	mov x1, #0
	ldr x0, [sp]
	bl _nevo_array_set
	ldr x8, [sp], #16
	str  x8, [x29, #32]
	ldr x8,[x29,#32]
	str x8,[sp,#-16]!
	adrp x8, _str4@PAGE
	add  x8, x8, _str4@PAGEOFF
	mov x1,x8
	ldr x0,[sp],#16
	bl _nevo_array_push
	ldr  x8, [x29, #32]
	mov x0,x8
	bl _nevo_array_pop
	mov x8,x0
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str5@PAGE
	add  x0, x0, _str5@PAGEOFF
	bl   _nevo_print_text
	mov  x0, #0
	ldp  x29, x30, [sp], #48
	ret

