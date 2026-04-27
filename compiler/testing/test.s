.global _var ; <- make sure _main gets seen

.text         ; <- code section

_var:
    sub sp, sp, #16  ; Create stack pointer

    mov w0, #5       ; Load 5 into w0
    str w0, [sp, #0] ; Load w0 to sp #0

    mov w0, #7       ; Load 7 into w0
    str w0, [sp, #4] ; Load w0 to sp #4

    ldr w1, [sp, #0] ; Load sp #0 to w1
    ldr w2, [sp, #4] ; Load sp #4 to w2

    cmp w1, w2       ; Compare w1 with w2
    b.lt _set_1
    b.gt _set_2
    b.eq _set_0

_after_if:
    add sp, sp, #16  // restore stack
    ret			 ; make sure the C test code receives the value in w0

_set_1:
    mov w0, #1
    b _after_if

_set_2:
    mov w0, #2
    b _after_if

_set_0:
    mov w0, #0
    b _after_if