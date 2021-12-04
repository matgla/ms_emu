cpu 8086 

main: 
    jmp bios_init

some_op:
    mov al, bl 
    jmp main 

bios_init: 
    mov sp, 0xf000 
test_jmp:
    jmp next
    mov ss, sp 

next:
    push cs 
    pop es

    push ax

    cld 

    ; db 0xc0

    jmp test_jmp 
