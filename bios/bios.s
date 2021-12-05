cpu 8086 

main: 
    jmp bios_init

some_op:
    mov al, bl 
    jmp main 

bios_init: 
    mov sp, 0xf000 
test_jmp:   
    mov ss, sp 

next:
    push cs 
    pop es

    push ax

    jmp ax 
    jmp bx
    jmp bp 
    jmp [bx]
    jmp [bx + 0x10]
    jmp [bx + 0x1040]
    jmp [0x1000]
    
    jmp far [bp + 0xbeef]
    jmp 0x8:data 
    jmp 0x15:0x1234
    jmp 0xffff:0xffff

data:

    ; jmp al
    ; @ jmp [al]
    ; @ jmp [ax]
    ; jmp [sp]
    jmp sp

    ; jmp DWORD ax
    
    cld 

    ; db 0xc0

    jmp test_jmp 
