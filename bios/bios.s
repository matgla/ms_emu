cpu 8086 

main: 
    jmp bios_init


bios_init: 
    mov sp, 0xf000 
    mov ss, sp 

    push cs 
    pop es

    push ax

    cld 

    db 0xc0