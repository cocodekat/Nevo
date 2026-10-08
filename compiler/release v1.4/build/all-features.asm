.section __TEXT,__cstring,cstring_literals
_str0:
	.asciz "========================================\n"
_str1:
	.asciz "        NEVO v1.4 DUNGEON TOUR          \n"
_str2:
	.asciz "Hero name: "
_str3:
	.asciz "Difficulty (1-3): "
_str4:
	.asciz "  THE SHIFTING DUNGEON  "
_str5:
	.asciz "north,east,south,west"
_str6:
	.asciz ","
_str7:
	.asciz "north"
_str8:
	.asciz "east"
_str9:
	.asciz "south"
_str10:
	.asciz "west"
_str11:
	.asciz "Campaign: "
_str12:
	.asciz " (letters: "
_str13:
	.asciz ", contains DUNGEON: "
_str14:
	.asciz "DUNGEON"
_str15:
	.asciz ")\nFirst route step: "
_str16:
	.asciz " | wheel chose: "
_str17:
	.asciz "\n"
_str18:
	.asciz "The secret door decided not to exist today.\n"
_str19:
	.asciz "nevo-showcase-report.txt"
_str20:
	.asciz "NEVO v1.4 EXPEDITION\nunnamed hero\nsummary pending\n"
_str21:
	.asciz "\nWelcome, "
_str22:
	.asciz ". Compiler version: "
_str23:
	.asciz "\nMap lines: "
_str24:
	.asciz ", rooms containing ROOM: "
_str25:
	.asciz "ROOM"
_str26:
	.asciz ", word 'the': "
_str27:
	.asciz "the"
_str28:
	.asciz "\n\nFirst map entry:\n"
_str29:
	.asciz "\nEntries mentioning gold:\n"
_str30:
	.asciz "gold"
_str31:
	.asciz "\nReward multiplier before replacement: "
_str32:
	.asciz "; active multiplier: "
_str33:
	.asciz "Room 2 was safely bypassed.\n"
_str34:
	.asciz "Room "
_str35:
	.asciz ": roll="
_str36:
	.asciz ", danger="
_str37:
	.asciz " -> treasure +"
_str38:
	.asciz " -> damage "
_str39:
	.asciz "  A potion restores 2 HP.\n"
_str40:
	.asciz "  The party can go no farther.\n"
_str41:
	.asciz "\nExit rune: "
_str42:
	.asciz " "
_str43:
	.asciz "OPEN!\n"
_str44:
	.asciz "The expedition escaped.\n"
_str45:
	.asciz "The dungeon won this time.\n"
_str46:
	.asciz "The expedition remains unfinished.\n"
_str47:
	.asciz "Rank: "
_str48:
	.asciz " | Gold: "
_str49:
	.asciz " (as text: "
_str50:
	.asciz ")"
_str51:
	.asciz " | HP: "
_str52:
	.asciz " | Visits: "
_str53:
	.asciz "nevo-showcase-map-copy.txt"
_str54:
	.asciz "Copied final map line: "
_str55:
	.asciz "A"
_str56:
	.asciz "Legacy local syntax: chapter "
_str57:
	.asciz "Reused variable name after rvar(): "
_str58:
	.asciz "Ordinary call result: "
_str59:
	.asciz "Press one key to sign the expedition log: "
_str60:
	.asciz "Stop demo (0=finish, 1=quit, 2=kaboom): "
_str61:
	.asciz "examples/showcase_data.txt"
_str62:
	.asciz "Dungeon legend"
_str63:
	.asciz "Treasure hunter"
_str64:
	.asciz "Cautious explorer"
_str65:
	.asciz "\nShowcase complete. Read nevo-showcase-report.txt too!\n"

.section __DATA,__bss
_rt_buf:
	.space 33

.comm _gv_heroName, 8, 3
.comm _gv_visits, 8, 3
.comm _gv_gameRunning, 8, 3
.comm _gv_partyHp, 8, 3
.comm _gv_roomDanger, 8, 3
.comm _gv_sharedReport, 8, 3
.comm _gv_room, 8, 3
.comm _gv_legacyResult, 8, 3

.section __DATA,__data
	.p2align 3
_fnslot_version:
	.quad _version
_fnslot_main:
	.quad _main
_fnslot_reward:
	.quad _reward
_fnslot_loadMap:
	.quad _loadMap
_fnslot_rank:
	.quad _rank
_fnslot_updateSharedState:
	.quad _updateSharedState
_fnslot_legacyAdd:
	.quad _legacyAdd
_fnslot_finish:
	.quad _finish

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

_version:
	stp  x29, x30, [sp, #-16]!
	mov  x29, sp
	mov  x8, #14
	mov  x0, x8
	ldp  x29, x30, [sp], #16
	ret

_main:
	stp  x29, x30, [sp, #-192]!
	mov  x29, sp
	bl _nevo_clear
	adrp x0, _str0@PAGE
	add  x0, x0, _str0@PAGEOFF
	bl   _nevo_print_text
	adrp x0, _str1@PAGE
	add  x0, x0, _str1@PAGEOFF
	bl   _nevo_print_text
	adrp x0, _str0@PAGE
	add  x0, x0, _str0@PAGEOFF
	bl   _nevo_print_text
	adrp x8, _str2@PAGE
	add  x8, x8, _str2@PAGEOFF
	mov  x0, x8
	bl   _nevo_input
	mov  x8, x0
	adrp x9, _gv_heroName@PAGE
	str  x8, [x9, _gv_heroName@PAGEOFF]
	adrp x8, _str3@PAGE
	add  x8, x8, _str3@PAGEOFF
	str x8,[sp,#-16]!
	mov  x8, #1
	str x8,[sp,#-16]!
	mov  x8, #3
	mov x2,x8
	ldr x1,[sp],#16
	ldr x0,[sp],#16
	bl _nevo_input_num_range
	mov x8,x0
	str  x8, [x29, #16]
	adrp x8, _str4@PAGE
	add  x8, x8, _str4@PAGEOFF
	str  x8, [x29, #24]
	ldr  x8, [x29, #24]
	mov x0,x8
	bl _nevo_text_trim
	mov x8,x0
	str  x8, [x29, #32]
	adrp x8, _str5@PAGE
	add  x8, x8, _str5@PAGEOFF
	str  x8, [x29, #40]
	ldr  x8, [x29, #40]
	str  x8, [sp, #-16]!
	adrp x8, _str6@PAGE
	add  x8, x8, _str6@PAGEOFF
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_text_split
	mov  x8, x0
	str  x8, [x29, #48]
	ldr  x8, [x29, #48]
	str  x8, [sp, #-16]!
	mov  x8, #0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [x29, #56]
	mov x0,#4
	bl _nevo_array_new
	mov x8,x0
	str x8,[sp,#-16]!
	adrp x8, _str7@PAGE
	add  x8, x8, _str7@PAGEOFF
	mov x2,x8
	mov x1,#0
	ldr x0,[sp]
	bl _nevo_array_set
	adrp x8, _str8@PAGE
	add  x8, x8, _str8@PAGEOFF
	mov x2,x8
	mov x1,#1
	ldr x0,[sp]
	bl _nevo_array_set
	adrp x8, _str9@PAGE
	add  x8, x8, _str9@PAGEOFF
	mov x2,x8
	mov x1,#2
	ldr x0,[sp]
	bl _nevo_array_set
	adrp x8, _str10@PAGE
	add  x8, x8, _str10@PAGEOFF
	mov x2,x8
	mov x1,#3
	ldr x0,[sp]
	bl _nevo_array_set
	mov x0,#4
	bl _arc4random_uniform
	mov x1,x0
	ldr x0,[sp],#16
	bl _nevo_array_get
	mov x8,x0
	str  x8, [x29, #64]
	bl _nevo_maybe
	mov x8, x0
	str  x8, [x29, #72]
	adrp x0, _str11@PAGE
	add  x0, x0, _str11@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #32]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str12@PAGE
	add  x0, x0, _str12@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #32]
	mov x0,x8
	bl _nevo_text_length
	mov x8,x0
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str13@PAGE
	add  x0, x0, _str13@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #32]
	str  x8, [sp, #-16]!
	adrp x8, _str14@PAGE
	add  x8, x8, _str14@PAGEOFF
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_text_contains
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str15@PAGE
	add  x0, x0, _str15@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #56]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str16@PAGE
	add  x0, x0, _str16@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #64]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #72]
	cmp x8, #0
	cset x8, eq
	cbz  x8, _Ifalse0
	adrp x0, _str18@PAGE
	add  x0, x0, _str18@PAGEOFF
	bl   _nevo_print_text
_Ifalse0:
	mov  x8, #0
	adrp x9, _gv_visits@PAGE
	str  x8, [x9, _gv_visits@PAGEOFF]
	mov  x8, #1
	adrp x9,_gv_gameRunning@PAGE
	str x8,[x9,_gv_gameRunning@PAGEOFF]
	mov x0, #3
	bl _nevo_array_new
	mov x8, x0
	str x8, [sp, #-16]!
	mov  x8, #12
	mov x2, x8
	mov x1, #0
	ldr x0, [sp]
	bl _nevo_array_set
	mov  x8, #9
	mov x2, x8
	mov x1, #1
	ldr x0, [sp]
	bl _nevo_array_set
	mov  x8, #7
	mov x2, x8
	mov x1, #2
	ldr x0, [sp]
	bl _nevo_array_set
	ldr x8, [sp], #16
	adrp x9,_gv_partyHp@PAGE
	str x8,[x9,_gv_partyHp@PAGEOFF]
	mov x0, #4
	bl _nevo_array_new
	mov x8, x0
	str x8, [sp, #-16]!
	mov  x8, #1
	mov x2, x8
	mov x1, #0
	ldr x0, [sp]
	bl _nevo_array_set
	mov  x8, #3
	mov x2, x8
	mov x1, #1
	ldr x0, [sp]
	bl _nevo_array_set
	mov  x8, #2
	mov x2, x8
	mov x1, #2
	ldr x0, [sp]
	bl _nevo_array_set
	mov  x8, #5
	mov x2, x8
	mov x1, #3
	ldr x0, [sp]
	bl _nevo_array_set
	ldr x8, [sp], #16
	adrp x9,_gv_roomDanger@PAGE
	str x8,[x9,_gv_roomDanger@PAGEOFF]
	adrp x8, _str19@PAGE
	add  x8, x8, _str19@PAGEOFF
	mov  x0, x8
	bl   _nevo_createf
	mov  x8, x0
	adrp x9, _gv_sharedReport@PAGE
	str  x8, [x9, _gv_sharedReport@PAGEOFF]
	adrp x16, _fnslot_loadMap@PAGE
	ldr  x16, [x16, _fnslot_loadMap@PAGEOFF]
	blr  x16
	mov  x8, x0
	str  x8, [x29, #80]
	adrp x9, _gv_sharedReport@PAGE
	ldr  x8, [x9, _gv_sharedReport@PAGEOFF]
	str  x8, [sp, #-16]!
	adrp x8, _str20@PAGE
	add  x8, x8, _str20@PAGEOFF
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_write_text
	adrp x16, _fnslot_updateSharedState@PAGE
	ldr  x16, [x16, _fnslot_updateSharedState@PAGEOFF]
	blr  x16
	mov  x8, x0
	adrp x0, _str21@PAGE
	add  x0, x0, _str21@PAGEOFF
	bl   _nevo_print_text
	adrp x9, _gv_heroName@PAGE
	ldr  x8, [x9, _gv_heroName@PAGEOFF]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str22@PAGE
	add  x0, x0, _str22@PAGEOFF
	bl   _nevo_print_text
	adrp x16, _fnslot_version@PAGE
	ldr  x16, [x16, _fnslot_version@PAGEOFF]
	blr  x16
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str23@PAGE
	add  x0, x0, _str23@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #80]
	mov  x0, x8
	bl   _nevo_file_count_lines
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str24@PAGE
	add  x0, x0, _str24@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #80]
	str  x8, [sp, #-16]!
	adrp x8, _str25@PAGE
	add  x8, x8, _str25@PAGEOFF
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_file_count_filter
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str26@PAGE
	add  x0, x0, _str26@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #80]
	str  x8, [sp, #-16]!
	adrp x8, _str27@PAGE
	add  x8, x8, _str27@PAGEOFF
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_file_count_word
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str28@PAGE
	add  x0, x0, _str28@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #80]
	str  x8, [sp, #-16]!
	mov  x8, #1
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_file_line
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str29@PAGE
	add  x0, x0, _str29@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #80]
	str  x8, [sp, #-16]!
	adrp x8, _str30@PAGE
	add  x8, x8, _str30@PAGEOFF
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_file_filter
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str31@PAGE
	add  x0, x0, _str31@PAGEOFF
	bl   _nevo_print_text
	adrp x16, _fnslot_reward@PAGE
	ldr  x16, [x16, _fnslot_reward@PAGEOFF]
	blr  x16
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
	ldr  x8, [x29, #16]
	str  x8, [sp, #-16]!
	mov  x8, #3
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, eq
	cbz  x8, _Ifalse1
	adrp x9, _fnslot_reward@PAGE
	adrp x8, __nevo_repl_0_reward@PAGE
	add  x8, x8, __nevo_repl_0_reward@PAGEOFF
	str  x8, [x9, _fnslot_reward@PAGEOFF]
_Ifalse1:
	adrp x0, _str32@PAGE
	add  x0, x0, _str32@PAGEOFF
	bl   _nevo_print_text
	adrp x16, _fnslot_reward@PAGE
	ldr  x16, [x16, _fnslot_reward@PAGEOFF]
	blr  x16
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
	mov  x8, #0
	str  x8, [x29, #88]
	mov  x8, #0
	str  x8, [x29, #96]
	mov  x8, #0
	str  x8, [x29, #104]
_Lwhile0:
	ldr  x8, [x29, #104]
	str  x8, [sp, #-16]!
	adrp x9, _gv_roomDanger@PAGE
	ldr  x8, [x9, _gv_roomDanger@PAGEOFF]
	mov x0,x8
	bl _nevo_array_length
	mov x8,x0
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, lt
	cbz x8,_Lbreak0
	ldr  x8, [x29, #104]
	str  x8, [sp, #-16]!
	mov  x8, #1
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, eq
	cbz  x8, _Llogic_false0
	ldr  x8, [x29, #16]
	str  x8, [sp, #-16]!
	mov  x8, #1
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, eq
	cmp  x8, #0
	cset x8, ne
	b _Llogic_end0
_Llogic_false0:
	mov x8, #0
_Llogic_end0:
	cbz  x8, _Ifalse2
	adrp x0, _str33@PAGE
	add  x0, x0, _str33@PAGEOFF
	bl   _nevo_print_text
	b _Lcontinue0
_Ifalse2:
	mov  x8, #1
	str  x8, [sp, #-16]!
	mov  x8, #6
	ldr  x10, [sp], #16
	sub  x8, x8, x10
	add  x8, x8, #1
	mov  x0, x8
	str  x10, [sp, #-16]!
	bl   _arc4random_uniform
	ldr  x10, [sp], #16
	add  x8, x0, x10
	str  x8, [x29, #112]
	adrp x9, _gv_roomDanger@PAGE
	ldr  x8, [x9, _gv_roomDanger@PAGEOFF]
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #104]
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #16]
	ldr  x10, [sp], #16
	add  x8, x10, x8
	str  x8, [x29, #120]
	adrp x0, _str34@PAGE
	add  x0, x0, _str34@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #104]
	str  x8, [sp, #-16]!
	mov  x8, #1
	ldr  x10, [sp], #16
	add  x8, x10, x8
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str35@PAGE
	add  x0, x0, _str35@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #112]
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str36@PAGE
	add  x0, x0, _str36@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #120]
	mov  x0, x8
	bl   _nevo_print_num
	ldr  x8, [x29, #112]
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #120]
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, ge
	cbnz x8, _Llogic_true1
	ldr  x8, [x29, #112]
	str  x8, [sp, #-16]!
	mov  x8, #6
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, eq
	cmp  x8, #0
	cset x8, ne
	b _Llogic_end1
_Llogic_true1:
	mov x8, #1
_Llogic_end1:
	cbz  x8, _Ifalse3
	mov  x8, #1
	str  x8, [sp, #-16]!
	mov  x8, #4
	ldr  x10, [sp], #16
	sub  x8, x8, x10
	add  x8, x8, #1
	mov  x0, x8
	str  x10, [sp, #-16]!
	bl   _arc4random_uniform
	ldr  x10, [sp], #16
	add  x8, x0, x10
	str  x8, [sp, #-16]!
	adrp x16, _fnslot_reward@PAGE
	ldr  x16, [x16, _fnslot_reward@PAGEOFF]
	blr  x16
	mov  x8, x0
	ldr  x10, [sp], #16
	mul  x8, x10, x8
	str  x8, [x29, #128]
	ldr  x10, [x29, #88]
	ldr  x8, [x29, #128]
	add  x8, x10, x8
	str  x8, [x29, #88]
	adrp x0, _str37@PAGE
	add  x0, x0, _str37@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #128]
	mov  x0, x8
	bl   _nevo_print_num
	b    _Iend3
_Ifalse3:
	adrp x9,_gv_partyHp@PAGE
	ldr x8,[x9,_gv_partyHp@PAGEOFF]
	str x8,[sp,#-16]!
	mov  x8, #0
	str x8,[sp,#-16]!
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #120]
	ldr  x10, [sp], #16
	sub  x8, x10, x8
	mov x2,x8
	ldr x1,[sp],#16
	ldr x0,[sp],#16
	bl _nevo_array_set
	adrp x0, _str38@PAGE
	add  x0, x0, _str38@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #120]
	mov  x0, x8
	bl   _nevo_print_num
_Iend3:
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
_Lwhile1:
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	mov  x8, #6
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, lt
	cbz  x8, _Llogic_false2
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #1
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	mov  x8, #0
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, gt
	cmp  x8, #0
	cset x8, ne
	b _Llogic_end2
_Llogic_false2:
	mov x8, #0
_Llogic_end2:
	cbz x8,_Lbreak1
	adrp x9,_gv_partyHp@PAGE
	ldr x8,[x9,_gv_partyHp@PAGEOFF]
	str x8,[sp,#-16]!
	mov  x8, #0
	str x8,[sp,#-16]!
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	mov  x8, #2
	ldr  x10, [sp], #16
	add  x8, x10, x8
	mov x2,x8
	ldr x1,[sp],#16
	ldr x0,[sp],#16
	bl _nevo_array_set
	adrp x9,_gv_partyHp@PAGE
	ldr x8,[x9,_gv_partyHp@PAGEOFF]
	str x8,[sp,#-16]!
	mov  x8, #1
	str x8,[sp,#-16]!
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #1
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	mov  x8, #1
	ldr  x10, [sp], #16
	sub  x8, x10, x8
	mov x2,x8
	ldr x1,[sp],#16
	ldr x0,[sp],#16
	bl _nevo_array_set
	adrp x0, _str39@PAGE
	add  x0, x0, _str39@PAGEOFF
	bl   _nevo_print_text
_Lcontinue1:
	b _Lwhile1
_Lbreak1:
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	mov  x8, #0
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, le
	cbz  x8, _Ifalse4
	mov  x8, #0
	adrp x9, _gv_gameRunning@PAGE
	str  x8, [x9, _gv_gameRunning@PAGEOFF]
	adrp x0, _str40@PAGE
	add  x0, x0, _str40@PAGEOFF
	bl   _nevo_print_text
	b _Lbreak0
_Ifalse4:
	ldr  x8, [x29, #104]
	str  x8, [sp, #-16]!
	mov  x8, #3
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, eq
	cbz  x8, _Ifalse5
	mov  x8, #1
	str  x8, [x29, #96]
_Ifalse5:
	mov  x8, #15
	mov x0,x8
	bl _nevo_sleep_ms
_Lcontinue0:
	ldr  x8, [x29, #104]
	str  x8, [sp, #-16]!
	mov  x8, #1
	ldr  x10, [sp], #16
	add  x8, x10, x8
	str  x8, [x29, #104]
	b _Lwhile0
_Lbreak0:
	mov  x8, #3
	str  x8, [x29, #136]
	adrp x0, _str41@PAGE
	add  x0, x0, _str41@PAGEOFF
	bl   _nevo_print_text
	mov  x8, #3
	str  x8, [x29, #184]         ; loop counter
_Lstart0:
	ldr  x8, [x29, #184]
	cbz  x8, _Lend0
	ldr  x8, [x29, #136]
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str42@PAGE
	add  x0, x0, _str42@PAGEOFF
	bl   _nevo_print_text
	ldr  x10, [x29, #136]
	mov  x8, #1
	sub  x8, x10, x8
	str  x8, [x29, #136]
_Lcontinue2:
	ldr  x8, [x29, #184]
	sub  x8, x8, #1
	str  x8, [x29, #184]
	b    _Lstart0
_Lend0:
_Lbreak2:
	adrp x0, _str43@PAGE
	add  x0, x0, _str43@PAGEOFF
	bl   _nevo_print_text
	adrp x9, _gv_gameRunning@PAGE
	ldr  x8, [x9, _gv_gameRunning@PAGEOFF]
	cbz  x8, _Llogic_false3
	ldr  x8, [x29, #96]
	cmp  x8, #0
	cset x8, ne
	b _Llogic_end3
_Llogic_false3:
	mov x8, #0
_Llogic_end3:
	cbz  x8, _Ifalse6
	adrp x0, _str44@PAGE
	add  x0, x0, _str44@PAGEOFF
	bl   _nevo_print_text
	b    _Iend6
_Ifalse6:
	adrp x9, _gv_gameRunning@PAGE
	ldr  x8, [x9, _gv_gameRunning@PAGEOFF]
	cmp x8, #0
	cset x8, eq
	cbnz x8, _Llogic_true4
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	mov  x8, #0
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, le
	cmp  x8, #0
	cset x8, ne
	b _Llogic_end4
_Llogic_true4:
	mov x8, #1
_Llogic_end4:
	cbz  x8, _Ifalse7
	adrp x0, _str45@PAGE
	add  x0, x0, _str45@PAGEOFF
	bl   _nevo_print_text
	b    _Iend7
_Ifalse7:
	adrp x0, _str46@PAGE
	add  x0, x0, _str46@PAGEOFF
	bl   _nevo_print_text
_Iend7:
_Iend6:
	adrp x0, _str47@PAGE
	add  x0, x0, _str47@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #88]
	str  x8, [sp, #-16]!
	ldr  x0, [sp], #16
	adrp x16, _fnslot_rank@PAGE
	ldr  x16, [x16, _fnslot_rank@PAGEOFF]
	blr  x16
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str48@PAGE
	add  x0, x0, _str48@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #88]
	mov  x0, x8
	bl   _nevo_print_num
	ldr  x8, [x29, #88]
	mov x0,x8
	bl _nevo_num_to_text
	mov x8,x0
	str  x8, [x29, #144]
	adrp x0, _str49@PAGE
	add  x0, x0, _str49@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #144]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str50@PAGE
	add  x0, x0, _str50@PAGEOFF
	bl   _nevo_print_text
	adrp x0, _str51@PAGE
	add  x0, x0, _str51@PAGEOFF
	bl   _nevo_print_text
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str52@PAGE
	add  x0, x0, _str52@PAGEOFF
	bl   _nevo_print_text
	adrp x9, _gv_visits@PAGE
	ldr  x8, [x9, _gv_visits@PAGEOFF]
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
	adrp x9, _gv_sharedReport@PAGE
	ldr  x8, [x9, _gv_sharedReport@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #3
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #80]
	str  x8, [sp, #-16]!
	mov  x8, #1
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_file_line
	mov  x8, x0
	mov  x2, x8
	ldr  x1, [sp], #16
	ldr  x0, [sp], #16
	bl   _nevo_write_line_text
	adrp x8, _str53@PAGE
	add  x8, x8, _str53@PAGEOFF
	mov  x0, x8
	bl   _nevo_createf
	mov  x8, x0
	str  x8, [x29, #152]
	ldr  x8, [x29, #152]
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #80]
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_write_file
	ldr  x8, [x29, #152]
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #80]
	str  x8, [sp, #-16]!
	mov  x8, #4
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_file_line
	mov  x8, x0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_write_text
	adrp x0, _str54@PAGE
	add  x0, x0, _str54@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #152]
	mov  x0, x8
	bl   _nevo_print_file
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
	mov  x8, #1
	str  x8, [x29, #160]
	adrp x8, _str55@PAGE
	add  x8, x8, _str55@PAGEOFF
	str  x8, [x29, #168]
	adrp x0, _str56@PAGE
	add  x0, x0, _str56@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #160]
	mov  x0, x8
	bl   _nevo_print_num
	ldr  x8, [x29, #168]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
	mov  x8, #2
	str  x8, [x29, #160]
	adrp x0, _str57@PAGE
	add  x0, x0, _str57@PAGEOFF
	bl   _nevo_print_text
	ldr  x8, [x29, #160]
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
	mov  x8, #3
	str  x8, [sp, #-16]!
	mov  x8, #4
	str  x8, [sp, #-16]!
	ldr  x1, [sp], #16
	ldr  x0, [sp], #16
	adrp x16, _fnslot_legacyAdd@PAGE
	ldr  x16, [x16, _fnslot_legacyAdd@PAGEOFF]
	blr  x16
	mov  x8, x0
	adrp x0, _str58@PAGE
	add  x0, x0, _str58@PAGEOFF
	bl   _nevo_print_text
	adrp x9, _gv_legacyResult@PAGE
	ldr  x8, [x9, _gv_legacyResult@PAGEOFF]
	mov  x0, x8
	bl   _nevo_print_num
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
	adrp x0, _str59@PAGE
	add  x0, x0, _str59@PAGEOFF
	bl   _nevo_print_text
	bl _nevo_keypress
	mov x8, x0
	str  x8, [x29, #176]
	ldr  x8, [x29, #176]
	mov  x0, x8
	bl   _nevo_print_text
	adrp x0, _str17@PAGE
	add  x0, x0, _str17@PAGEOFF
	bl   _nevo_print_text
	adrp x8, _str60@PAGE
	add  x8, x8, _str60@PAGEOFF
	str x8,[sp,#-16]!
	mov  x8, #0
	str x8,[sp,#-16]!
	mov  x8, #2
	mov x2,x8
	ldr x1,[sp],#16
	ldr x0,[sp],#16
	bl _nevo_input_num_range
	mov x8,x0
	str  x8, [x29, #184]
	ldr  x8, [x29, #184]
	str  x8, [sp, #-16]!
	mov  x8, #1
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, eq
	cbz  x8, _Ifalse8
	mov  x0, #0
	bl   _exit
	b    _Iend8
_Ifalse8:
	ldr  x8, [x29, #184]
	str  x8, [sp, #-16]!
	mov  x8, #2
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, eq
	cbz  x8, _Ifalse9
	mov  x0, #0
	bl   _exit
_Ifalse9:
_Iend8:
	ldp  x29, x30, [sp], #192
	adrp x16, _fnslot_finish@PAGE
	ldr  x16, [x16, _fnslot_finish@PAGEOFF]
	br   x16

__nevo_repl_0_reward:
	stp  x29, x30, [sp, #-16]!
	mov  x29, sp
	mov  x8, #3
	mov  x0, x8
	ldp  x29, x30, [sp], #16
	ret

_reward:
	stp  x29, x30, [sp, #-16]!
	mov  x29, sp
	mov  x8, #1
	mov  x0, x8
	ldp  x29, x30, [sp], #16
	ret

_loadMap:
	stp  x29, x30, [sp, #-32]!
	mov  x29, sp
	adrp x8, _str61@PAGE
	add  x8, x8, _str61@PAGEOFF
	mov  x0, x8
	bl   _nevo_loadf
	mov  x8, x0
	str  x8, [x29, #16]
	ldr  x8, [x29, #16]
	mov  x0, x8
	ldp  x29, x30, [sp], #32
	ret

_rank:
	stp  x29, x30, [sp, #-32]!
	mov  x29, sp
	str  x0, [x29, #16]       ; parameter gold
	ldr  x8, [x29, #16]
	str  x8, [sp, #-16]!
	mov  x8, #12
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, ge
	cbz  x8, _Ifalse10
	adrp x8, _str62@PAGE
	add  x8, x8, _str62@PAGEOFF
	mov  x0, x8
	ldp  x29, x30, [sp], #32
	ret
	b    _Iend10
_Ifalse10:
	ldr  x8, [x29, #16]
	str  x8, [sp, #-16]!
	mov  x8, #6
	ldr  x10, [sp], #16
	cmp  x10, x8
	cset x8, ge
	cbz  x8, _Ifalse11
	adrp x8, _str63@PAGE
	add  x8, x8, _str63@PAGEOFF
	mov  x0, x8
	ldp  x29, x30, [sp], #32
	ret
	b    _Iend11
_Ifalse11:
	adrp x8, _str64@PAGE
	add  x8, x8, _str64@PAGEOFF
	mov  x0, x8
	ldp  x29, x30, [sp], #32
	ret
_Iend11:
_Iend10:
	mov  x0, #0
	ldp  x29, x30, [sp], #32
	ret

_updateSharedState:
	stp  x29, x30, [sp, #-16]!
	mov  x29, sp
	adrp x9, _gv_visits@PAGE
	ldr  x10, [x9, _gv_visits@PAGEOFF]
	mov  x8, #1
	add  x8, x10, x8
	adrp x9, _gv_visits@PAGE
	str  x8, [x9, _gv_visits@PAGEOFF]
	adrp x9,_gv_partyHp@PAGE
	ldr x8,[x9,_gv_partyHp@PAGEOFF]
	str x8,[sp,#-16]!
	mov  x8, #0
	str x8,[sp,#-16]!
	adrp x9, _gv_partyHp@PAGE
	ldr  x8, [x9, _gv_partyHp@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #0
	mov  x1, x8
	ldr  x0, [sp], #16
	bl   _nevo_array_get
	mov  x8, x0
	str  x8, [sp, #-16]!
	mov  x8, #2
	ldr  x10, [sp], #16
	add  x8, x10, x8
	mov x2,x8
	ldr x1,[sp],#16
	ldr x0,[sp],#16
	bl _nevo_array_set
	adrp x9, _gv_gameRunning@PAGE
	ldr  x8, [x9, _gv_gameRunning@PAGEOFF]
	cbz  x8, _Ifalse12
	adrp x9, _gv_sharedReport@PAGE
	ldr  x8, [x9, _gv_sharedReport@PAGEOFF]
	str  x8, [sp, #-16]!
	mov  x8, #2
	str  x8, [sp, #-16]!
	adrp x9, _gv_heroName@PAGE
	ldr  x8, [x9, _gv_heroName@PAGEOFF]
	mov  x2, x8
	ldr  x1, [sp], #16
	ldr  x0, [sp], #16
	bl   _nevo_write_line_text
_Ifalse12:
	mov  x0, #0
	ldp  x29, x30, [sp], #16
	ret

_legacyAdd:
	stp  x29, x30, [sp, #-32]!
	mov  x29, sp
	str  x0, [x29, #16]       ; parameter a
	str  x1, [x29, #24]       ; parameter b
	ldr  x8, [x29, #16]
	str  x8, [sp, #-16]!
	ldr  x8, [x29, #24]
	ldr  x10, [sp], #16
	add  x8, x10, x8
	adrp x9, _gv_legacyResult@PAGE
	str  x8, [x9, _gv_legacyResult@PAGEOFF]
	mov  x0, #0
	ldp  x29, x30, [sp], #32
	ret

_finish:
	stp  x29, x30, [sp, #-16]!
	mov  x29, sp
	adrp x0, _str65@PAGE
	add  x0, x0, _str65@PAGEOFF
	bl   _nevo_print_text
	mov  x0, #0
	ldp  x29, x30, [sp], #16
	ret

