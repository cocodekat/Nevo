.global _var ; <- make sure _main gets seen

.text         ; <- code section

_var:
sub sp, sp, #16 ; create 16 bytes of space in memory on stack pointer (sp + 0-16)
mov w0, #5      ; put number 5 in register 0
str w0, [sp, #0]; load register 0 to memory slot sp+0

add sp, sp, #16 ; undoing the allocated 16 byte space (to prevent crashes)

ret 			; make sure the C test code receives the value in w0