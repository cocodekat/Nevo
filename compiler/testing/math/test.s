.global _var ; <- make sure _main gets seen

.text         ; <- code section

_var:
sub sp, sp, #16 ; create 16 bytes of space in memory on stack pointer (sp + 0-16)

mov w0, #5      ; put number 5 in register 0
str w0, [sp, #0]; load register 0 to memory slot sp+0

mov w0, #7      ; put number 7 in register 0
str w0, [sp, #4]; load register 0 to memory slot sp+4 (next 4 bits)

mov w0, #2      ; put number 7 in register 0
str w0, [sp, #8]; load register 0 to memory slot sp+8 (next 4 bits)

ldr w1, [sp, #0]; load sp0 (5) to reg w1
ldr w2, [sp, #4]; load sp4 (7) to reg w2
ldr w3, [sp, #8]; load sp8 (2) to reg w3

add w0, w1, w2  ; add w1 + w2 and store in w0
mul w0, w0, w3  ; mul w0 * w3 and store in w0

add sp, sp, #16 ; undoing the allocated 16 byte space (to prevent crashes)

ret 			; make sure the C test code receives the value in w0