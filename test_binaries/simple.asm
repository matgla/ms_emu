BITS 16
CPU 8086

MOV AX, 0B800h     ; set AX to hexadecimal value of B800h. 
MOV DS, AX         ; copy value of AX to DS. 
MOV CL, 'A'        ; set CL to ASCII code of 'A', it is 41h. 
MOV CH, 1101_1111b ; set CH to binary value. 
MOV AX, 0x0000
MOV BX, 20h       ; set BX to 15Eh. 
MOV [BX], CX       ; copy contents of CX to memory at B800:015E 
RET                ; returns to operating system. 