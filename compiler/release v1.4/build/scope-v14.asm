.section __TEXT,__cstring,cstring_literals
_str0:
	.asciz "score="
_str1:
	.asciz "tests/fixtures/source.txt"
_str2:
	.asciz "\n"
_str3:
	.asciz "|"

.section __DATA,__bss
_rt_buf:
	.space 33

.comm _gv_score, 8, 3
.comm _gv_label, 8, 3
.comm _gv_enabled, 8, 3
.comm _gv_points, 8, 3
.comm _gv_source, 8, 3

.section __DATA,__data
	.p2align 3
_fnslot_main:
	.quad _main
_fnslot_increase:
	.quad _increase
_fnslot_showGlobals:
	.quad _showGlobals

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
	stp  x29, x30, [sp, #-32]!
	mov  x29, sp
	mov  x8, #1
	adrp x9, _gv_score@PAGE
	str  x8, [x9, _gv_score@PAGEOFF]
	adrp x8, _str0@PAGE
	add  x8, x8, _str0@PAGEOFF
	adrp x9, _gv_label@PAGE
	str  x8, [x9, _gv_label@PAGEOFF]
	mov  x8, #1
	adrp x9,_gv_enabled@PAGE
	str x8,[x9,_gv_enabled@PAGEOFF]
	mov x0, #2
	bl _nevo_array_new
	mov x8, x0
	str x8, [sp, #-16]!
	mov  x8, #2
	mov x2, x8
	mov x1, #0
	ldr x0, [sp]
	bl _nevo_array_set
	mov  x8, #3
	mov x2, x8
	mov x1, #1
	ldr x0, [sp]
	bl _nevo_array_set
	ldr x8, [sp], #16
	adrp x9,_gv_points@PAGE
	str x8,[x9,_gv_points@PAGEOFF]
	adrp x8, _str1@PAGE
	add  x8, x8, _str1@PAGEOFF
	mov  x0, x8
	bl   _nevo_loadf
	mov  x8, x0
	adrp x9, _gv_source@PAGE
	str  x8, [x9, _gv_source@PAGEOFF]
	mov  x8, #5
	str  x8, [x29, #16]
	adrp x9, _gv_score@PAGE
	ldr  x8, [x9, _gv_score@PAGEOFF]
	str  x8, [sp, #-16]!
	ldr  x0, [sp], #16
	adrp x16, _fnslot_increase@PAGE
	ldr  x16, [x16, _fnslot_increase@PAGEOFF]
	blr  x16
	mov  x8, x0
	adrp x9, _gv_score@PAGE
	str  x8, [x9, _gv_score@PAGEOFF]
	ldr  x8, [x29, #16]
	mov  x0, x8
	bl   _nevo_print_num
	adrp x9, _gv_score@PAGE
	ldr  x8, [x9, _gv_score@PAGEOFF]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str2@PAGE
	add  x0, x0, _str2@PAGEOFF
	bl   _nevo_print_text
	adrp x16, _fnslot_showGlobals@PAGE
	ldr  x16, [x16, _fnslot_showGlobals@PAGEOFF]
	blr  x16
	mov  x8, x0
	mov  x0, #0
	ldp  x29, x30, [sp], #32
	ret

_increase:
	stp  x29, x30, [sp, #-32]!
	mov  x29, sp
	str  x0, [x29, #16]       ; parameter value
	mov  x8, #40
	str  x8, [x29, #24]
	ldr  x8, [x29, #16]
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #24]
	ldr  x10, [sp], #16
	add  x8, x10, x8
	mov  x0, x8
	ldp  x29, x30, [sp], #32
	ret

_showGlobals:
	stp  x29, x30, [sp, #-16]!
	mov  x29, sp
	adrp x9, _gv_label@PAGE
	ldr  x8, [x9, _gv_label@PAGEOFF]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x9, _gv_enabled@PAGE
	ldr  x8, [x9, _gv_enabled@PAGEOFF]
	cbz  x8, _Ifalse0
	adrp x9, _gv_points@PAGE
	ldr  x8, [x9, _gv_points@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #1
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
_Ifalse0:
	adrp x0, _str3@PAGE
	add  x0, x0, _str3@PAGEOFF
	bl   _nevo_print_text
	adrp x9, _gv_source@PAGE
	ldr  x8, [x9, _gv_source@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #1
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_file_line
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_text
	mov  x0, #0
	ldp  x29, x30, [sp], #16
	ret

