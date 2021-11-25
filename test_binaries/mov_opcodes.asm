cpu 8086

mov al, [0x100]
mov bl, [0x100]
mov cl, [0x100]
mov dl, [0x100]

mov ah, [0x100]
mov bh, [0x100]
mov ch, [0x100]
mov dh, [0x100]

mov al, [si]
mov bl, [si]
mov cl, [si]
mov dl, [si]

mov ah, [si]
mov bh, [si]
mov ch, [si]
mov dh, [si]

mov al, [bx+si]
mov bl, [bx+si]
mov cl, [bx+si]
mov dl, [bx+si]

mov ah, [bx+si]
mov bh, [bx+si]
mov ch, [bx+si]
mov dh, [bx+si]

mov al, [bx+di]
mov bl, [bx+di]
mov cl, [bx+di]
mov dl, [bx+di]

mov ah, [bx+di]
mov bh, [bx+di]
mov ch, [bx+di]
mov dh, [bx+di+0x10]
mov dh, [bx+di+0x1020]

mov bh, [bx]


