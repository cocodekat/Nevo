.data
msg:
    .asciz "Hello, world\n"

.global _main
.text
_main:
    ldr x16, =0x2000004   // write syscall
    mov x0, 1             // stdout

    adrp x1, msg@PAGE
    add  x1, x1, msg@PAGEOFF
    mov x2, 13

    svc 0

    ldr x16, =0x2000001   // exit syscall
    mov x0, 0
    svc 0