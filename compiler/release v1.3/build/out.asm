.section __TEXT,__cstring,cstring_literals

.section __DATA,__bss
_rt_buf:
	.space 33

.comm _gv_total, 8, 3
.comm _gv_result, 8, 3

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
	stp  x29, x30, [sp, #-16]!
	mov  x29, sp
	mov  x8, #7
	str  x8, [sp, #-16]!
	mov  x8, #8
	str  x8, [sp, #-16]!
	ldr  x1, [sp], #16
	ldr  x0, [sp], #16
	bl   _add
	mov  x8, x0
	adrp x9, _gv_total@PAGE
	str  x8, [x9, _gv_total@PAGEOFF]
	adrp x9, _gv_total@PAGE
	ldr  x8, [x9, _gv_total@PAGEOFF]
	mov  x0, x8
	bl   _int64_to_str
	bl   _puts
	mov  x0, #0
	ldp  x29, x30, [sp], #16
	ret

_add:
	stp  x29, x30, [sp, #-32]!
	mov  x29, sp
	str  x0, [x29, #16]       ; parameter a
	str  x1, [x29, #24]       ; parameter b
	ldr  x8, [x29, #16]
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #24]
	ldr  x10, [sp], #16
	add  x8, x10, x8
	adrp x9, _gv_result@PAGE
	str  x8, [x9, _gv_result@PAGEOFF]
	adrp x9, _gv_result@PAGE
	ldr  x8, [x9, _gv_result@PAGEOFF]
	mov  x0, x8
	ldp  x29, x30, [sp], #32
	ret

