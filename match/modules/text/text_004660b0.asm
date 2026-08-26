BITS 32
org 0x004660b0

CollisionQuery_Execute:
; Initialize collision masks and copy the active scene/debug label.
.at_004660b0:
    sub esp,byte +0x38                    ; retail 83ec38
.at_004660b3:
    or ecx,byte -0x1                    ; retail 83c9ff
.at_004660b6:
    db 0x33, 0xc0                    ; xor eax,eax (retail encoding)
.at_004660b8:
    mov dword [dword 0x568638],0x106                    ; retail c7053886560006010000
.at_004660c2:
    push ebx                    ; retail 53
.at_004660c3:
    push ebp                    ; retail 55
.at_004660c4:
    push esi                    ; retail 56
.at_004660c5:
    push edi                    ; retail 57
.at_004660c6:
    mov edi,0x533e34                    ; retail bf343e5300
.at_004660cb:
    mov ebx,[dword 0x567f80]                    ; retail 8b1d807f5600
.at_004660d1:
    repne scasb                    ; retail f2ae
.at_004660d3:
    not ecx                    ; retail f7d1
.at_004660d5:
    db 0x2b, 0xf9                    ; sub edi,ecx (retail encoding)
.at_004660d7:
    push dword 0x533e20                    ; retail 68203e5300
.at_004660dc:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_004660de:
    db 0x8b, 0xf7                    ; mov esi,edi (retail encoding)
.at_004660e0:
    mov edi,0x56881c                    ; retail bf1c885600
.at_004660e5:
    shr ecx,byte 0x2                    ; retail c1e902
.at_004660e8:
    rep movsd                    ; retail f3a5
.at_004660ea:
    db 0x8b, 0xc8                    ; mov ecx,eax (retail encoding)
.at_004660ec:
    and ecx,byte +0x3                    ; retail 83e103
.at_004660ef:
    rep movsb                    ; retail f3a4
.at_004660f1:
    db 0x33, 0xf6                    ; xor esi,esi (retail encoding)
.at_004660f3:
    db 0x3b, 0xde                    ; cmp ebx,esi (retail encoding)
.at_004660f5:
    setnz cl                    ; retail 0f95c1
.at_004660f8:
    push ecx                    ; retail 51
.at_004660f9:
    call 0x4011e0                    ; retail e8e2b0f9ff
.at_004660fe:
    mov ecx,[dword 0x567c84]                    ; retail 8b0d847c5600
.at_00466104:
    add esp,byte +0x8                    ; retail 83c408
.at_00466107:
    db 0x3b, 0xce                    ; cmp ecx,esi (retail encoding)
.at_00466109:
    mov [dword 0x567a60],esi                    ; retail 8935607a5600
.at_0046610f:
    mov dword [dword 0x567a68],0xffffffff                    ; retail c705687a5600ffffffff
.at_00466119:
    mov eax,0x400000                    ; retail b800004000
.at_0046611e:
    db 0x74, 0x05                    ; jz 0x466125 (retail encoding)
.at_00466120:
    mov [0x567a60],eax                    ; retail a3607a5600
.at_00466125:
    cmp [dword 0x567c7c],esi                    ; retail 39357c7c5600
.at_0046612b:
    db 0x74, 0x06                    ; jz 0x466133 (retail encoding)
.at_0046612d:
    xor [dword 0x567a60],eax                    ; retail 3105607a5600
.at_00466133:
    cmp [dword 0x567c78],esi                    ; retail 3935787c5600
.at_00466139:
    db 0x75, 0x0a                    ; jnz 0x466145 (retail encoding)
.at_0046613b:
    xor dword [dword 0x567a60],0x200000                    ; retail 8135607a560000002000
.at_00466145:
    cmp [dword 0x567c74],esi                    ; retail 3935747c5600
.at_0046614b:
    db 0x74, 0x0a                    ; jz 0x466157 (retail encoding)
.at_0046614d:
    mov dword [dword 0x567a68],0xffefffff                    ; retail c705687a5600ffffefff
.at_00466157:
    cmp [dword 0x567c80],esi                    ; retail 3935807c5600
.at_0046615d:
    db 0x74, 0x0a                    ; jz 0x466169 (retail encoding)
.at_0046615f:
    xor dword [dword 0x567a68],0x20000                    ; retail 8135687a560000000200
.at_00466169:
    cmp [esp+0x50],esi                    ; retail 39742450
.at_0046616d:
    db 0x74, 0x13                    ; jz 0x466182 (retail encoding)
.at_0046616f:
    mov edx,[esp+0x4c]                    ; retail 8b54244c
.at_00466173:
    mov eax,[0x56af40]                    ; retail a140af5600
.at_00466178:
    push edx                    ; retail 52
.at_00466179:
    push eax                    ; retail 50
.at_0046617a:
    call 0x4628f0                    ; retail e871c7ffff
.at_0046617f:
    add esp,byte +0x8                    ; retail 83c408
.at_00466182:
    mov [esp+0x20],esi                    ; retail 89742420
; Walk collision partitions (0x660-byte descriptors) until the pointer slot
; at descriptor offset zero is null.
.at_00466186:
    mov ecx,[esp+0x20]                    ; retail 8b4c2420
.at_0046618a:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_0046618c:
    shl eax,byte 0x4                    ; retail c1e004
.at_0046618f:
    db 0x03, 0xc1                    ; add eax,ecx (retail encoding)
.at_00466191:
    lea eax,[eax+eax*2]                    ; retail 8d0440
.at_00466194:
    shl eax,byte 0x5                    ; retail c1e005
.at_00466197:
    mov [esp+0x14],eax                    ; retail 89442414
.at_0046619b:
    mov ecx,[eax+0x567f80]                    ; retail 8b88807f5600
.at_004661a1:
    test ecx,ecx                    ; retail 85c9
.at_004661a3:
    jz near 0x4667ad                    ; retail 0f8404060000
.at_004661a9:
    mov ecx,[esp+0x14]                    ; retail 8b4c2414
.at_004661ad:
    mov eax,[esp+0x4c]                    ; retail 8b44244c
.at_004661b1:
    mov edx,[ecx+0x567f8c]                    ; retail 8b918c7f5600
.at_004661b7:
    mov esi,[eax]                    ; retail 8b30
.at_004661b9:
    mov edi,[eax+0xc]                    ; retail 8b780c
.at_004661bc:
    mov ebx,[eax+0x8]                    ; retail 8b5808
.at_004661bf:
    mov ebp,[eax+0x14]                    ; retail 8b6814
.at_004661c2:
    mov eax,[ecx+0x567f84]                    ; retail 8b81847f5600
.at_004661c8:
    mov [esp+0x18],edx                    ; retail 89542418
.at_004661cc:
    mov edx,[ecx+0x567f88]                    ; retail 8b91887f5600
.at_004661d2:
    mov [esp+0x50],edx                    ; retail 89542450
.at_004661d6:
    mov edx,[ecx+0x567f90]                    ; retail 8b91907f5600
.at_004661dc:
    mov [esp+0x1c],edx                    ; retail 8954241c
.at_004661e0:
    mov edx,[ecx+0x567f94]                    ; retail 8b91947f5600
.at_004661e6:
    db 0x3b, 0xf0                    ; cmp esi,eax (retail encoding)
.at_004661e8:
    mov [esp+0x10],eax                    ; retail 89442410
.at_004661ec:
    mov [esp+0x24],edx                    ; retail 89542424
.at_004661f0:
    db 0x7d, 0x08                    ; jnl 0x4661fa (retail encoding)
.at_004661f2:
    db 0x3b, 0xf8                    ; cmp edi,eax (retail encoding)
.at_004661f4:
    jl near 0x4667ad                    ; retail 0f8cb3050000
.at_004661fa:
    mov eax,[esp+0x18]                    ; retail 8b442418
.at_004661fe:
    db 0x3b, 0xf0                    ; cmp esi,eax (retail encoding)
.at_00466200:
    db 0x7e, 0x0c                    ; jng 0x46620e (retail encoding)
.at_00466202:
    db 0x3b, 0xf8                    ; cmp edi,eax (retail encoding)
.at_00466204:
    jg near 0x4667ad                    ; retail 0f8fa3050000
.at_0046620a:
    mov ecx,[esp+0x14]                    ; retail 8b4c2414
.at_0046620e:
    mov eax,[esp+0x50]                    ; retail 8b442450
.at_00466212:
    db 0x3b, 0xd8                    ; cmp ebx,eax (retail encoding)
.at_00466214:
    db 0x7d, 0x0c                    ; jnl 0x466222 (retail encoding)
.at_00466216:
    db 0x3b, 0xe8                    ; cmp ebp,eax (retail encoding)
.at_00466218:
    jl near 0x4667ad                    ; retail 0f8c8f050000
.at_0046621e:
    mov ecx,[esp+0x14]                    ; retail 8b4c2414
.at_00466222:
    mov eax,[esp+0x1c]                    ; retail 8b44241c
.at_00466226:
    db 0x3b, 0xd8                    ; cmp ebx,eax (retail encoding)
.at_00466228:
    db 0x7e, 0x0c                    ; jng 0x466236 (retail encoding)
.at_0046622a:
    db 0x3b, 0xe8                    ; cmp ebp,eax (retail encoding)
.at_0046622c:
    jg near 0x4667ad                    ; retail 0f8f7b050000
.at_00466232:
    mov ecx,[esp+0x14]                    ; retail 8b4c2414
; Handle a point/degenerate segment directly through the spatial-cell table.
.at_00466236:
    db 0x3b, 0xf7                    ; cmp esi,edi (retail encoding)
.at_00466238:
    jnz near 0x466307                    ; retail 0f85c9000000
.at_0046623e:
    db 0x3b, 0xdd                    ; cmp ebx,ebp (retail encoding)
.at_00466240:
    jnz near 0x466307                    ; retail 0f85c1000000
.at_00466246:
    db 0x8b, 0xc6                    ; mov eax,esi (retail encoding)
.at_00466248:
    mov esi,[esp+0x10]                    ; retail 8b742410
.at_0046624c:
    db 0x2b, 0xc6                    ; sub eax,esi (retail encoding)
.at_0046624e:
    mov esi,[esp+0x24]                    ; retail 8b742424
.at_00466252:
    cdq                    ; retail 99
.at_00466253:
    idiv esi                    ; retail f7fe
.at_00466255:
    mov edx,[esp+0x50]                    ; retail 8b542450
.at_00466259:
    db 0x8b, 0xe8                    ; mov ebp,eax (retail encoding)
.at_0046625b:
    db 0x8b, 0xc3                    ; mov eax,ebx (retail encoding)
.at_0046625d:
    db 0x2b, 0xc2                    ; sub eax,edx (retail encoding)
.at_0046625f:
    cdq                    ; retail 99
.at_00466260:
    idiv esi                    ; retail f7fe
.at_00466262:
    movsx edx,word [ecx+0x567f9c]                    ; retail 0fbf919c7f5600
.at_00466269:
    db 0x3b, 0xea                    ; cmp ebp,edx (retail encoding)
.at_0046626b:
    db 0x8b, 0xd8                    ; mov ebx,eax (retail encoding)
.at_0046626d:
    db 0x75, 0x01                    ; jnz 0x466270 (retail encoding)
.at_0046626f:
    dec ebp                    ; retail 4d
.at_00466270:
    movsx eax,word [ecx+0x567f9e]                    ; retail 0fbf819e7f5600
.at_00466277:
    db 0x3b, 0xd8                    ; cmp ebx,eax (retail encoding)
.at_00466279:
    mov [esp+0x50],eax                    ; retail 89442450
.at_0046627d:
    db 0x75, 0x01                    ; jnz 0x466280 (retail encoding)
.at_0046627f:
    dec ebx                    ; retail 4b
.at_00466280:
    mov edi,0x533e34                    ; retail bf343e5300
.at_00466285:
    or ecx,byte -0x1                    ; retail 83c9ff
.at_00466288:
    db 0x33, 0xc0                    ; xor eax,eax (retail encoding)
.at_0046628a:
    mov dword [dword 0x568638],0x149                    ; retail c7053886560049010000
.at_00466294:
    repne scasb                    ; retail f2ae
.at_00466296:
    not ecx                    ; retail f7d1
.at_00466298:
    db 0x2b, 0xf9                    ; sub edi,ecx (retail encoding)
.at_0046629a:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_0046629c:
    db 0x8b, 0xf7                    ; mov esi,edi (retail encoding)
.at_0046629e:
    mov edi,0x56881c                    ; retail bf1c885600
.at_004662a3:
    shr ecx,byte 0x2                    ; retail c1e902
.at_004662a6:
    rep movsd                    ; retail f3a5
.at_004662a8:
    db 0x8b, 0xc8                    ; mov ecx,eax (retail encoding)
.at_004662aa:
    and ecx,byte +0x3                    ; retail 83e103
.at_004662ad:
    test ebp,ebp                    ; retail 85ed
.at_004662af:
    rep movsb                    ; retail f3a4
.at_004662b1:
    db 0x7c, 0x15                    ; jl 0x4662c8 (retail encoding)
.at_004662b3:
    db 0x3b, 0xea                    ; cmp ebp,edx (retail encoding)
.at_004662b5:
    db 0x7d, 0x11                    ; jnl 0x4662c8 (retail encoding)
.at_004662b7:
    test ebx,ebx                    ; retail 85db
.at_004662b9:
    db 0x7c, 0x0d                    ; jl 0x4662c8 (retail encoding)
.at_004662bb:
    cmp ebx,[esp+0x50]                    ; retail 3b5c2450
.at_004662bf:
    db 0x7d, 0x07                    ; jnl 0x4662c8 (retail encoding)
.at_004662c1:
    mov eax,0x1                    ; retail b801000000
.at_004662c6:
    db 0xeb, 0x02                    ; jmp short 0x4662ca (retail encoding)
.at_004662c8:
    db 0x33, 0xc0                    ; xor eax,eax (retail encoding)
.at_004662ca:
    push dword 0x533e08                    ; retail 68083e5300
.at_004662cf:
    push eax                    ; retail 50
.at_004662d0:
    call 0x4011e0                    ; retail e80baff9ff
.at_004662d5:
    mov ecx,[esp+0x54]                    ; retail 8b4c2454
.at_004662d9:
    push ecx                    ; retail 51
.at_004662da:
    mov ecx,[esp+0x2c]                    ; retail 8b4c242c
.at_004662de:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_004662e0:
    shl eax,byte 0x4                    ; retail c1e004
.at_004662e3:
    db 0x03, 0xc1                    ; add eax,ecx (retail encoding)
.at_004662e5:
    lea edx,[eax+eax*2]                    ; retail 8d1440
.at_004662e8:
    lea eax,[ebp+ebp*4+0x0]                    ; retail 8d44ad00
.at_004662ec:
    lea ecx,[ebx+eax*4]                    ; retail 8d0c83
.at_004662ef:
    lea edx,[ecx+edx*8]                    ; retail 8d14d1
.at_004662f2:
    mov eax,[edx*4+0x567fa0]                    ; retail 8b0495a07f5600
.at_004662f9:
    push eax                    ; retail 50
.at_004662fa:
    call 0x4638d0                    ; retail e8d1d5ffff
.at_004662ff:
    add esp,byte +0x10                    ; retail 83c410
.at_00466302:
    jmp 0x4667ad                    ; retail e9a6040000
; Clip/rasterize a non-degenerate segment against the partition bounds.
.at_00466307:
    mov eax,[esp+0x10]                    ; retail 8b442410
.at_0046630b:
    db 0x3b, 0xf0                    ; cmp esi,eax (retail encoding)
.at_0046630d:
    db 0x7d, 0x3c                    ; jnl 0x46634b (retail encoding)
.at_0046630f:
    db 0x3b, 0xdd                    ; cmp ebx,ebp (retail encoding)
.at_00466311:
    db 0x8b, 0xcf                    ; mov ecx,edi (retail encoding)
.at_00466313:
    db 0x7d, 0x17                    ; jnl 0x46632c (retail encoding)
.at_00466315:
    db 0x8b, 0xd5                    ; mov edx,ebp (retail encoding)
.at_00466317:
    db 0x2b, 0xce                    ; sub ecx,esi (retail encoding)
.at_00466319:
    db 0x2b, 0xd3                    ; sub edx,ebx (retail encoding)
.at_0046631b:
    push ecx                    ; retail 51
.at_0046631c:
    db 0x2b, 0xc6                    ; sub eax,esi (retail encoding)
.at_0046631e:
    push edx                    ; retail 52
.at_0046631f:
    push eax                    ; retail 50
.at_00466320:
    call 0x4f5f10                    ; retail e8ebfb0800
.at_00466325:
    add esp,byte +0xc                    ; retail 83c40c
.at_00466328:
    db 0x03, 0xd8                    ; add ebx,eax (retail encoding)
.at_0046632a:
    db 0xeb, 0x1b                    ; jmp short 0x466347 (retail encoding)
.at_0046632c:
    db 0x2b, 0xce                    ; sub ecx,esi (retail encoding)
.at_0046632e:
    db 0x2b, 0xdd                    ; sub ebx,ebp (retail encoding)
.at_00466330:
    push ecx                    ; retail 51
.at_00466331:
    push ebx                    ; retail 53
.at_00466332:
    mov ebx,[esp+0x18]                    ; retail 8b5c2418
.at_00466336:
    db 0x8b, 0xd7                    ; mov edx,edi (retail encoding)
.at_00466338:
    db 0x2b, 0xd3                    ; sub edx,ebx (retail encoding)
.at_0046633a:
    push edx                    ; retail 52
.at_0046633b:
    call 0x4f5f10                    ; retail e8d0fb0800
.at_00466340:
    add esp,byte +0xc                    ; retail 83c40c
.at_00466343:
    db 0x03, 0xc5                    ; add eax,ebp (retail encoding)
.at_00466345:
    db 0x8b, 0xd8                    ; mov ebx,eax (retail encoding)
.at_00466347:
    mov esi,[esp+0x10]                    ; retail 8b742410
.at_0046634b:
    cmp edi,[esp+0x10]                    ; retail 3b7c2410
.at_0046634f:
    db 0x7d, 0x40                    ; jnl 0x466391 (retail encoding)
.at_00466351:
    db 0x3b, 0xeb                    ; cmp ebp,ebx (retail encoding)
.at_00466353:
    db 0x8b, 0xc6                    ; mov eax,esi (retail encoding)
.at_00466355:
    db 0x7d, 0x1b                    ; jnl 0x466372 (retail encoding)
.at_00466357:
    mov edx,[esp+0x10]                    ; retail 8b542410
.at_0046635b:
    db 0x8b, 0xcb                    ; mov ecx,ebx (retail encoding)
.at_0046635d:
    db 0x2b, 0xc7                    ; sub eax,edi (retail encoding)
.at_0046635f:
    db 0x2b, 0xcd                    ; sub ecx,ebp (retail encoding)
.at_00466361:
    push eax                    ; retail 50
.at_00466362:
    db 0x2b, 0xd7                    ; sub edx,edi (retail encoding)
.at_00466364:
    push ecx                    ; retail 51
.at_00466365:
    push edx                    ; retail 52
.at_00466366:
    call 0x4f5f10                    ; retail e8a5fb0800
.at_0046636b:
    add esp,byte +0xc                    ; retail 83c40c
.at_0046636e:
    db 0x03, 0xe8                    ; add ebp,eax (retail encoding)
.at_00466370:
    db 0xeb, 0x1b                    ; jmp short 0x46638d (retail encoding)
.at_00466372:
    db 0x2b, 0xc7                    ; sub eax,edi (retail encoding)
.at_00466374:
    db 0x2b, 0xeb                    ; sub ebp,ebx (retail encoding)
.at_00466376:
    push eax                    ; retail 50
.at_00466377:
    push ebp                    ; retail 55
.at_00466378:
    mov ebp,[esp+0x18]                    ; retail 8b6c2418
.at_0046637c:
    db 0x8b, 0xce                    ; mov ecx,esi (retail encoding)
.at_0046637e:
    db 0x2b, 0xcd                    ; sub ecx,ebp (retail encoding)
.at_00466380:
    push ecx                    ; retail 51
.at_00466381:
    call 0x4f5f10                    ; retail e88afb0800
.at_00466386:
    add esp,byte +0xc                    ; retail 83c40c
.at_00466389:
    db 0x03, 0xc3                    ; add eax,ebx (retail encoding)
.at_0046638b:
    db 0x8b, 0xe8                    ; mov ebp,eax (retail encoding)
.at_0046638d:
    mov edi,[esp+0x10]                    ; retail 8b7c2410
.at_00466391:
    mov eax,[esp+0x18]                    ; retail 8b442418
.at_00466395:
    db 0x3b, 0xf0                    ; cmp esi,eax (retail encoding)
.at_00466397:
    db 0x7e, 0x3a                    ; jng 0x4663d3 (retail encoding)
.at_00466399:
    db 0x3b, 0xeb                    ; cmp ebp,ebx (retail encoding)
.at_0046639b:
    db 0x7d, 0x19                    ; jnl 0x4663b6 (retail encoding)
.at_0046639d:
    db 0x2b, 0xf7                    ; sub esi,edi (retail encoding)
.at_0046639f:
    db 0x8b, 0xd0                    ; mov edx,eax (retail encoding)
.at_004663a1:
    db 0x2b, 0xdd                    ; sub ebx,ebp (retail encoding)
.at_004663a3:
    push esi                    ; retail 56
.at_004663a4:
    db 0x2b, 0xd7                    ; sub edx,edi (retail encoding)
.at_004663a6:
    push ebx                    ; retail 53
.at_004663a7:
    push edx                    ; retail 52
.at_004663a8:
    call 0x4f5f10                    ; retail e863fb0800
.at_004663ad:
    add esp,byte +0xc                    ; retail 83c40c
.at_004663b0:
    db 0x03, 0xc5                    ; add eax,ebp (retail encoding)
.at_004663b2:
    db 0x8b, 0xd8                    ; mov ebx,eax (retail encoding)
.at_004663b4:
    db 0xeb, 0x19                    ; jmp short 0x4663cf (retail encoding)
.at_004663b6:
    db 0x8b, 0xc6                    ; mov eax,esi (retail encoding)
.at_004663b8:
    db 0x8b, 0xcd                    ; mov ecx,ebp (retail encoding)
.at_004663ba:
    db 0x2b, 0xc7                    ; sub eax,edi (retail encoding)
.at_004663bc:
    db 0x2b, 0xcb                    ; sub ecx,ebx (retail encoding)
.at_004663be:
    push eax                    ; retail 50
.at_004663bf:
    push ecx                    ; retail 51
.at_004663c0:
    sub esi,[esp+0x20]                    ; retail 2b742420
.at_004663c4:
    push esi                    ; retail 56
.at_004663c5:
    call 0x4f5f10                    ; retail e846fb0800
.at_004663ca:
    add esp,byte +0xc                    ; retail 83c40c
.at_004663cd:
    db 0x03, 0xd8                    ; add ebx,eax (retail encoding)
.at_004663cf:
    mov esi,[esp+0x18]                    ; retail 8b742418
.at_004663d3:
    mov eax,[esp+0x18]                    ; retail 8b442418
.at_004663d7:
    db 0x3b, 0xf8                    ; cmp edi,eax (retail encoding)
.at_004663d9:
    db 0x7e, 0x3a                    ; jng 0x466415 (retail encoding)
.at_004663db:
    db 0x3b, 0xdd                    ; cmp ebx,ebp (retail encoding)
.at_004663dd:
    db 0x7d, 0x19                    ; jnl 0x4663f8 (retail encoding)
.at_004663df:
    db 0x2b, 0xfe                    ; sub edi,esi (retail encoding)
.at_004663e1:
    db 0x8b, 0xd0                    ; mov edx,eax (retail encoding)
.at_004663e3:
    db 0x2b, 0xeb                    ; sub ebp,ebx (retail encoding)
.at_004663e5:
    push edi                    ; retail 57
.at_004663e6:
    db 0x2b, 0xd6                    ; sub edx,esi (retail encoding)
.at_004663e8:
    push ebp                    ; retail 55
.at_004663e9:
    push edx                    ; retail 52
.at_004663ea:
    call 0x4f5f10                    ; retail e821fb0800
.at_004663ef:
    add esp,byte +0xc                    ; retail 83c40c
.at_004663f2:
    db 0x03, 0xc3                    ; add eax,ebx (retail encoding)
.at_004663f4:
    db 0x8b, 0xe8                    ; mov ebp,eax (retail encoding)
.at_004663f6:
    db 0xeb, 0x19                    ; jmp short 0x466411 (retail encoding)
.at_004663f8:
    db 0x8b, 0xc7                    ; mov eax,edi (retail encoding)
.at_004663fa:
    db 0x8b, 0xcb                    ; mov ecx,ebx (retail encoding)
.at_004663fc:
    db 0x2b, 0xc6                    ; sub eax,esi (retail encoding)
.at_004663fe:
    db 0x2b, 0xcd                    ; sub ecx,ebp (retail encoding)
.at_00466400:
    push eax                    ; retail 50
.at_00466401:
    push ecx                    ; retail 51
.at_00466402:
    sub edi,[esp+0x20]                    ; retail 2b7c2420
.at_00466406:
    push edi                    ; retail 57
.at_00466407:
    call 0x4f5f10                    ; retail e804fb0800
.at_0046640c:
    add esp,byte +0xc                    ; retail 83c40c
.at_0046640f:
    db 0x03, 0xe8                    ; add ebp,eax (retail encoding)
.at_00466411:
    mov edi,[esp+0x18]                    ; retail 8b7c2418
.at_00466415:
    cmp ebx,[esp+0x50]                    ; retail 3b5c2450
.at_00466419:
    db 0x7d, 0x40                    ; jnl 0x46645b (retail encoding)
.at_0046641b:
    db 0x3b, 0xf7                    ; cmp esi,edi (retail encoding)
.at_0046641d:
    db 0x8b, 0xd5                    ; mov edx,ebp (retail encoding)
.at_0046641f:
    db 0x7d, 0x1b                    ; jnl 0x46643c (retail encoding)
.at_00466421:
    mov ecx,[esp+0x50]                    ; retail 8b4c2450
.at_00466425:
    db 0x8b, 0xc7                    ; mov eax,edi (retail encoding)
.at_00466427:
    db 0x2b, 0xd3                    ; sub edx,ebx (retail encoding)
.at_00466429:
    db 0x2b, 0xc6                    ; sub eax,esi (retail encoding)
.at_0046642b:
    push edx                    ; retail 52
.at_0046642c:
    db 0x2b, 0xcb                    ; sub ecx,ebx (retail encoding)
.at_0046642e:
    push eax                    ; retail 50
.at_0046642f:
    push ecx                    ; retail 51
.at_00466430:
    call 0x4f5f10                    ; retail e8dbfa0800
.at_00466435:
    add esp,byte +0xc                    ; retail 83c40c
.at_00466438:
    db 0x03, 0xf0                    ; add esi,eax (retail encoding)
.at_0046643a:
    db 0xeb, 0x1b                    ; jmp short 0x466457 (retail encoding)
.at_0046643c:
    db 0x2b, 0xd3                    ; sub edx,ebx (retail encoding)
.at_0046643e:
    mov ebx,[esp+0x50]                    ; retail 8b5c2450
.at_00466442:
    db 0x8b, 0xc5                    ; mov eax,ebp (retail encoding)
.at_00466444:
    db 0x2b, 0xf7                    ; sub esi,edi (retail encoding)
.at_00466446:
    push edx                    ; retail 52
.at_00466447:
    db 0x2b, 0xc3                    ; sub eax,ebx (retail encoding)
.at_00466449:
    push esi                    ; retail 56
.at_0046644a:
    push eax                    ; retail 50
.at_0046644b:
    call 0x4f5f10                    ; retail e8c0fa0800
.at_00466450:
    add esp,byte +0xc                    ; retail 83c40c
.at_00466453:
    db 0x03, 0xc7                    ; add eax,edi (retail encoding)
.at_00466455:
    db 0x8b, 0xf0                    ; mov esi,eax (retail encoding)
.at_00466457:
    mov ebx,[esp+0x50]                    ; retail 8b5c2450
.at_0046645b:
    mov eax,[esp+0x50]                    ; retail 8b442450
.at_0046645f:
    db 0x3b, 0xe8                    ; cmp ebp,eax (retail encoding)
.at_00466461:
    db 0x7d, 0x3c                    ; jnl 0x46649f (retail encoding)
.at_00466463:
    db 0x3b, 0xfe                    ; cmp edi,esi (retail encoding)
.at_00466465:
    db 0x8b, 0xcb                    ; mov ecx,ebx (retail encoding)
.at_00466467:
    db 0x7d, 0x17                    ; jnl 0x466480 (retail encoding)
.at_00466469:
    db 0x8b, 0xd6                    ; mov edx,esi (retail encoding)
.at_0046646b:
    db 0x2b, 0xcd                    ; sub ecx,ebp (retail encoding)
.at_0046646d:
    db 0x2b, 0xd7                    ; sub edx,edi (retail encoding)
.at_0046646f:
    push ecx                    ; retail 51
.at_00466470:
    db 0x2b, 0xc5                    ; sub eax,ebp (retail encoding)
.at_00466472:
    push edx                    ; retail 52
.at_00466473:
    push eax                    ; retail 50
.at_00466474:
    call 0x4f5f10                    ; retail e897fa0800
.at_00466479:
    add esp,byte +0xc                    ; retail 83c40c
.at_0046647c:
    db 0x03, 0xf8                    ; add edi,eax (retail encoding)
.at_0046647e:
    db 0xeb, 0x1b                    ; jmp short 0x46649b (retail encoding)
.at_00466480:
    db 0x2b, 0xcd                    ; sub ecx,ebp (retail encoding)
.at_00466482:
    mov ebp,[esp+0x50]                    ; retail 8b6c2450
.at_00466486:
    db 0x8b, 0xd3                    ; mov edx,ebx (retail encoding)
.at_00466488:
    db 0x2b, 0xfe                    ; sub edi,esi (retail encoding)
.at_0046648a:
    push ecx                    ; retail 51
.at_0046648b:
    db 0x2b, 0xd5                    ; sub edx,ebp (retail encoding)
.at_0046648d:
    push edi                    ; retail 57
.at_0046648e:
    push edx                    ; retail 52
.at_0046648f:
    call 0x4f5f10                    ; retail e87cfa0800
.at_00466494:
    add esp,byte +0xc                    ; retail 83c40c
.at_00466497:
    db 0x03, 0xc6                    ; add eax,esi (retail encoding)
.at_00466499:
    db 0x8b, 0xf8                    ; mov edi,eax (retail encoding)
.at_0046649b:
    mov ebp,[esp+0x50]                    ; retail 8b6c2450
.at_0046649f:
    mov eax,[esp+0x1c]                    ; retail 8b44241c
.at_004664a3:
    db 0x3b, 0xd8                    ; cmp ebx,eax (retail encoding)
.at_004664a5:
    db 0x7e, 0x3a                    ; jng 0x4664e1 (retail encoding)
.at_004664a7:
    db 0x3b, 0xfe                    ; cmp edi,esi (retail encoding)
.at_004664a9:
    db 0x7d, 0x17                    ; jnl 0x4664c2 (retail encoding)
.at_004664ab:
    db 0x2b, 0xdd                    ; sub ebx,ebp (retail encoding)
.at_004664ad:
    db 0x2b, 0xf7                    ; sub esi,edi (retail encoding)
.at_004664af:
    push ebx                    ; retail 53
.at_004664b0:
    db 0x2b, 0xc5                    ; sub eax,ebp (retail encoding)
.at_004664b2:
    push esi                    ; retail 56
.at_004664b3:
    push eax                    ; retail 50
.at_004664b4:
    call 0x4f5f10                    ; retail e857fa0800
.at_004664b9:
    add esp,byte +0xc                    ; retail 83c40c
.at_004664bc:
    db 0x03, 0xc7                    ; add eax,edi (retail encoding)
.at_004664be:
    db 0x8b, 0xf0                    ; mov esi,eax (retail encoding)
.at_004664c0:
    db 0xeb, 0x1b                    ; jmp short 0x4664dd (retail encoding)
.at_004664c2:
    db 0x8b, 0xcb                    ; mov ecx,ebx (retail encoding)
.at_004664c4:
    db 0x8b, 0xd7                    ; mov edx,edi (retail encoding)
.at_004664c6:
    db 0x2b, 0xcd                    ; sub ecx,ebp (retail encoding)
.at_004664c8:
    db 0x2b, 0xd6                    ; sub edx,esi (retail encoding)
.at_004664ca:
    push ecx                    ; retail 51
.at_004664cb:
    mov ecx,[esp+0x20]                    ; retail 8b4c2420
.at_004664cf:
    db 0x2b, 0xd9                    ; sub ebx,ecx (retail encoding)
.at_004664d1:
    push edx                    ; retail 52
.at_004664d2:
    push ebx                    ; retail 53
.at_004664d3:
    call 0x4f5f10                    ; retail e838fa0800
.at_004664d8:
    add esp,byte +0xc                    ; retail 83c40c
.at_004664db:
    db 0x03, 0xf0                    ; add esi,eax (retail encoding)
.at_004664dd:
    mov ebx,[esp+0x1c]                    ; retail 8b5c241c
.at_004664e1:
    mov eax,[esp+0x1c]                    ; retail 8b44241c
.at_004664e5:
    db 0x3b, 0xe8                    ; cmp ebp,eax (retail encoding)
.at_004664e7:
    db 0x7e, 0x3a                    ; jng 0x466523 (retail encoding)
.at_004664e9:
    db 0x3b, 0xf7                    ; cmp esi,edi (retail encoding)
.at_004664eb:
    db 0x7d, 0x17                    ; jnl 0x466504 (retail encoding)
.at_004664ed:
    db 0x2b, 0xeb                    ; sub ebp,ebx (retail encoding)
.at_004664ef:
    db 0x2b, 0xfe                    ; sub edi,esi (retail encoding)
.at_004664f1:
    push ebp                    ; retail 55
.at_004664f2:
    db 0x2b, 0xc3                    ; sub eax,ebx (retail encoding)
.at_004664f4:
    push edi                    ; retail 57
.at_004664f5:
    push eax                    ; retail 50
.at_004664f6:
    call 0x4f5f10                    ; retail e815fa0800
.at_004664fb:
    add esp,byte +0xc                    ; retail 83c40c
.at_004664fe:
    db 0x03, 0xc6                    ; add eax,esi (retail encoding)
.at_00466500:
    db 0x8b, 0xf8                    ; mov edi,eax (retail encoding)
.at_00466502:
    db 0xeb, 0x1b                    ; jmp short 0x46651f (retail encoding)
.at_00466504:
    db 0x8b, 0xcd                    ; mov ecx,ebp (retail encoding)
.at_00466506:
    db 0x8b, 0xd6                    ; mov edx,esi (retail encoding)
.at_00466508:
    db 0x2b, 0xcb                    ; sub ecx,ebx (retail encoding)
.at_0046650a:
    db 0x2b, 0xd7                    ; sub edx,edi (retail encoding)
.at_0046650c:
    push ecx                    ; retail 51
.at_0046650d:
    mov ecx,[esp+0x20]                    ; retail 8b4c2420
.at_00466511:
    db 0x2b, 0xe9                    ; sub ebp,ecx (retail encoding)
.at_00466513:
    push edx                    ; retail 52
.at_00466514:
    push ebp                    ; retail 55
.at_00466515:
    call 0x4f5f10                    ; retail e8f6f90800
.at_0046651a:
    add esp,byte +0xc                    ; retail 83c40c
.at_0046651d:
    db 0x03, 0xf8                    ; add edi,eax (retail encoding)
.at_0046651f:
    mov ebp,[esp+0x1c]                    ; retail 8b6c241c
; Derive signed cell steps and fixed-point grid coordinates.
.at_00466523:
    db 0x8b, 0xc7                    ; mov eax,edi (retail encoding)
.at_00466525:
    db 0x2b, 0xc6                    ; sub eax,esi (retail encoding)
.at_00466527:
    mov [esp+0x34],eax                    ; retail 89442434
.at_0046652b:
    db 0x79, 0x10                    ; jns 0x46653d (retail encoding)
.at_0046652d:
    neg eax                    ; retail f7d8
.at_0046652f:
    mov dword [esp+0x3c],0xffffffff                    ; retail c744243cffffffff
.at_00466537:
    mov [esp+0x34],eax                    ; retail 89442434
.at_0046653b:
    db 0xeb, 0x08                    ; jmp short 0x466545 (retail encoding)
.at_0046653d:
    mov dword [esp+0x3c],0x1                    ; retail c744243c01000000
.at_00466545:
    db 0x8b, 0xc5                    ; mov eax,ebp (retail encoding)
.at_00466547:
    db 0x2b, 0xc3                    ; sub eax,ebx (retail encoding)
.at_00466549:
    mov [esp+0x30],eax                    ; retail 89442430
.at_0046654d:
    db 0x79, 0x10                    ; jns 0x46655f (retail encoding)
.at_0046654f:
    neg eax                    ; retail f7d8
.at_00466551:
    mov dword [esp+0x44],0xffffffff                    ; retail c7442444ffffffff
.at_00466559:
    mov [esp+0x30],eax                    ; retail 89442430
.at_0046655d:
    db 0xeb, 0x08                    ; jmp short 0x466567 (retail encoding)
.at_0046655f:
    mov dword [esp+0x44],0x1                    ; retail c744244401000000
.at_00466567:
    mov ecx,[esp+0x10]                    ; retail 8b4c2410
.at_0046656b:
    db 0x8b, 0xc6                    ; mov eax,esi (retail encoding)
.at_0046656d:
    db 0x2b, 0xc1                    ; sub eax,ecx (retail encoding)
.at_0046656f:
    mov ecx,[esp+0x24]                    ; retail 8b4c2424
.at_00466573:
    mov [esp+0x40],eax                    ; retail 89442440
.at_00466577:
    cdq                    ; retail 99
.at_00466578:
    idiv ecx                    ; retail f7f9
.at_0046657a:
    mov edx,[esp+0x10]                    ; retail 8b542410
.at_0046657e:
    mov [esp+0x1c],eax                    ; retail 8944241c
.at_00466582:
    db 0x8b, 0xc7                    ; mov eax,edi (retail encoding)
.at_00466584:
    db 0x2b, 0xc2                    ; sub eax,edx (retail encoding)
.at_00466586:
    cdq                    ; retail 99
.at_00466587:
    idiv ecx                    ; retail f7f9
.at_00466589:
    mov edx,[esp+0x50]                    ; retail 8b542450
.at_0046658d:
    mov [esp+0x28],eax                    ; retail 89442428
.at_00466591:
    db 0x8b, 0xc3                    ; mov eax,ebx (retail encoding)
.at_00466593:
    db 0x2b, 0xc2                    ; sub eax,edx (retail encoding)
.at_00466595:
    mov [esp+0x18],eax                    ; retail 89442418
.at_00466599:
    cdq                    ; retail 99
.at_0046659a:
    idiv ecx                    ; retail f7f9
.at_0046659c:
    mov edx,[esp+0x50]                    ; retail 8b542450
.at_004665a0:
    mov [esp+0x10],eax                    ; retail 89442410
.at_004665a4:
    db 0x8b, 0xc5                    ; mov eax,ebp (retail encoding)
.at_004665a6:
    db 0x2b, 0xc2                    ; sub eax,edx (retail encoding)
.at_004665a8:
    cdq                    ; retail 99
.at_004665a9:
    idiv ecx                    ; retail f7f9
.at_004665ab:
    mov [esp+0x2c],eax                    ; retail 8944242c
.at_004665af:
    mov eax,[esp+0x40]                    ; retail 8b442440
.at_004665b3:
    cdq                    ; retail 99
.at_004665b4:
    idiv ecx                    ; retail f7f9
.at_004665b6:
    mov eax,[esp+0x18]                    ; retail 8b442418
.at_004665ba:
    mov [esp+0x38],edx                    ; retail 89542438
.at_004665be:
    cdq                    ; retail 99
.at_004665bf:
    idiv ecx                    ; retail f7f9
.at_004665c1:
    mov eax,[esp+0x14]                    ; retail 8b442414
.at_004665c5:
    movsx eax,word [eax+0x567f9c]                    ; retail 0fbf809c7f5600
.at_004665cc:
    mov [esp+0x18],edx                    ; retail 89542418
.at_004665d0:
    mov edx,[esp+0x1c]                    ; retail 8b54241c
.at_004665d4:
    db 0x3b, 0xd0                    ; cmp edx,eax (retail encoding)
.at_004665d6:
    db 0x75, 0x0f                    ; jnz 0x4665e7 (retail encoding)
.at_004665d8:
    dec edx                    ; retail 4a
.at_004665d9:
    mov [esp+0x1c],edx                    ; retail 8954241c
.at_004665dd:
    mov edx,[esp+0x38]                    ; retail 8b542438
.at_004665e1:
    db 0x03, 0xd1                    ; add edx,ecx (retail encoding)
.at_004665e3:
    mov [esp+0x38],edx                    ; retail 89542438
.at_004665e7:
    mov edx,[esp+0x14]                    ; retail 8b542414
.at_004665eb:
    movsx edx,word [edx+0x567f9e]                    ; retail 0fbf929e7f5600
.at_004665f2:
    cmp [esp+0x10],edx                    ; retail 39542410
.at_004665f6:
    mov [esp+0x50],edx                    ; retail 89542450
.at_004665fa:
    db 0x75, 0x17                    ; jnz 0x466613 (retail encoding)
.at_004665fc:
    mov edx,[esp+0x10]                    ; retail 8b542410
.at_00466600:
    dec edx                    ; retail 4a
.at_00466601:
    mov [esp+0x10],edx                    ; retail 89542410
.at_00466605:
    mov edx,[esp+0x18]                    ; retail 8b542418
.at_00466609:
    db 0x03, 0xd1                    ; add edx,ecx (retail encoding)
.at_0046660b:
    mov [esp+0x18],edx                    ; retail 89542418
.at_0046660f:
    mov edx,[esp+0x50]                    ; retail 8b542450
.at_00466613:
    mov ecx,[esp+0x28]                    ; retail 8b4c2428
.at_00466617:
    db 0x3b, 0xc8                    ; cmp ecx,eax (retail encoding)
.at_00466619:
    db 0x75, 0x07                    ; jnz 0x466622 (retail encoding)
.at_0046661b:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_0046661d:
    dec eax                    ; retail 48
.at_0046661e:
    mov [esp+0x28],eax                    ; retail 89442428
.at_00466622:
    mov eax,[esp+0x2c]                    ; retail 8b44242c
.at_00466626:
    db 0x3b, 0xc2                    ; cmp eax,edx (retail encoding)
.at_00466628:
    db 0x75, 0x05                    ; jnz 0x46662f (retail encoding)
.at_0046662a:
    dec eax                    ; retail 48
.at_0046662b:
    mov [esp+0x2c],eax                    ; retail 8944242c
.at_0046662f:
    mov eax,[esp+0x24]                    ; retail 8b442424
.at_00466633:
    mov ecx,[esp+0x18]                    ; retail 8b4c2418
.at_00466637:
    mov edx,[esp+0x34]                    ; retail 8b542434
.at_0046663b:
    push eax                    ; retail 50
.at_0046663c:
    push ecx                    ; retail 51
.at_0046663d:
    push edx                    ; retail 52
.at_0046663e:
    call 0x4f5f10                    ; retail e8cdf80800
.at_00466643:
    mov ecx,[esp+0x44]                    ; retail 8b4c2444
.at_00466647:
    mov edx,[esp+0x3c]                    ; retail 8b54243c
.at_0046664b:
    mov [esp+0x4c],eax                    ; retail 8944244c
.at_0046664f:
    mov eax,[esp+0x30]                    ; retail 8b442430
.at_00466653:
    push eax                    ; retail 50
.at_00466654:
    push ecx                    ; retail 51
.at_00466655:
    push edx                    ; retail 52
.at_00466656:
    call 0x4f5f10                    ; retail e8b5f80800
.at_0046665b:
    mov ecx,[esp+0x34]                    ; retail 8b4c2434
.at_0046665f:
    mov edx,[esp+0x28]                    ; retail 8b542428
.at_00466663:
    add esp,byte +0x18                    ; retail 83c418
.at_00466666:
    db 0x3b, 0xf7                    ; cmp esi,edi (retail encoding)
.at_00466668:
    mov [esp+0x18],ecx                    ; retail 894c2418
.at_0046666c:
    mov [esp+0x50],edx                    ; retail 89542450
.at_00466670:
    db 0x7d, 0x06                    ; jnl 0x466678 (retail encoding)
.at_00466672:
    sub eax,[esp+0x30]                    ; retail 2b442430
.at_00466676:
    db 0xeb, 0x02                    ; jmp short 0x46667a (retail encoding)
.at_00466678:
    neg eax                    ; retail f7d8
.at_0046667a:
    db 0x3b, 0xdd                    ; cmp ebx,ebp (retail encoding)
.at_0046667c:
    db 0x7d, 0x0c                    ; jnl 0x46668a (retail encoding)
.at_0046667e:
    mov ecx,[esp+0x34]                    ; retail 8b4c2434
.at_00466682:
    mov edx,[esp+0x40]                    ; retail 8b542440
.at_00466686:
    db 0x2b, 0xca                    ; sub ecx,edx (retail encoding)
.at_00466688:
    db 0xeb, 0x04                    ; jmp short 0x46668e (retail encoding)
.at_0046668a:
    mov ecx,[esp+0x40]                    ; retail 8b4c2440
.at_0046668e:
    mov esi,[esp+0x1c]                    ; retail 8b74241c
.at_00466692:
    lea edi,[ecx+eax]                    ; retail 8d3c01
.at_00466695:
    mov eax,[esp+0x3c]                    ; retail 8b44243c
.at_00466699:
    mov ebp,[esp+0x18]                    ; retail 8b6c2418
.at_0046669d:
    mov ebx,[esp+0x50]                    ; retail 8b5c2450
.at_004666a1:
    lea esi,[esi+esi*4]                    ; retail 8d34b6
.at_004666a4:
    lea eax,[eax+eax*4]                    ; retail 8d0480
.at_004666a7:
    shl esi,byte 0x2                    ; retail c1e602
.at_004666aa:
    shl eax,byte 0x2                    ; retail c1e002
.at_004666ad:
    mov [esp+0x40],eax                    ; retail 89442440
; Visit crossed cells and dispatch their collision entries.
.at_004666b1:
    cmp ebp,[esp+0x28]                    ; retail 3b6c2428
.at_004666b5:
    db 0x75, 0x0a                    ; jnz 0x4666c1 (retail encoding)
.at_004666b7:
    cmp ebx,[esp+0x2c]                    ; retail 3b5c242c
.at_004666bb:
    jz near 0x46674b                    ; retail 0f848a000000
.at_004666c1:
    test ebp,ebp                    ; retail 85ed
.at_004666c3:
    jl near 0x4667ad                    ; retail 0f8ce4000000
.at_004666c9:
    mov eax,[esp+0x14]                    ; retail 8b442414
.at_004666cd:
    mov ebp,[esp+0x18]                    ; retail 8b6c2418
.at_004666d1:
    movsx ecx,word [eax+0x567f9c]                    ; retail 0fbf889c7f5600
.at_004666d8:
    db 0x3b, 0xe9                    ; cmp ebp,ecx (retail encoding)
.at_004666da:
    db 0x7d, 0x6f                    ; jnl 0x46674b (retail encoding)
.at_004666dc:
    mov ebx,[esp+0x50]                    ; retail 8b5c2450
.at_004666e0:
    test ebx,ebx                    ; retail 85db
.at_004666e2:
    db 0x7c, 0x67                    ; jl 0x46674b (retail encoding)
.at_004666e4:
    movsx edx,word [eax+0x567f9e]                    ; retail 0fbf909e7f5600
.at_004666eb:
    db 0x3b, 0xda                    ; cmp ebx,edx (retail encoding)
.at_004666ed:
    db 0x7d, 0x5c                    ; jnl 0x46674b (retail encoding)
.at_004666ef:
    mov eax,[esp+0x4c]                    ; retail 8b44244c
.at_004666f3:
    mov ecx,[esp+0x20]                    ; retail 8b4c2420
.at_004666f7:
    push eax                    ; retail 50
.at_004666f8:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_004666fa:
    shl eax,byte 0x4                    ; retail c1e004
.at_004666fd:
    db 0x03, 0xc1                    ; add eax,ecx (retail encoding)
.at_004666ff:
    lea ecx,[eax+eax*2]                    ; retail 8d0c40
.at_00466702:
    lea edx,[esi+ecx*8]                    ; retail 8d14ce
.at_00466705:
    db 0x03, 0xd3                    ; add edx,ebx (retail encoding)
.at_00466707:
    mov eax,[edx*4+0x567fa0]                    ; retail 8b0495a07f5600
.at_0046670e:
    push eax                    ; retail 50
.at_0046670f:
    call 0x4638d0                    ; retail e8bcd1ffff
.at_00466714:
    add esp,byte +0x8                    ; retail 83c408
.at_00466717:
    test edi,edi                    ; retail 85ff
.at_00466719:
    db 0x7c, 0x1b                    ; jl 0x466736 (retail encoding)
.at_0046671b:
    mov ecx,[esp+0x3c]                    ; retail 8b4c243c
.at_0046671f:
    mov edx,[esp+0x30]                    ; retail 8b542430
.at_00466723:
    mov eax,[esp+0x40]                    ; retail 8b442440
.at_00466727:
    db 0x03, 0xe9                    ; add ebp,ecx (retail encoding)
.at_00466729:
    db 0x2b, 0xfa                    ; sub edi,edx (retail encoding)
.at_0046672b:
    mov [esp+0x18],ebp                    ; retail 896c2418
.at_0046672f:
    db 0x03, 0xf0                    ; add esi,eax (retail encoding)
.at_00466731:
    jmp 0x4666b1                    ; retail e97bffffff
.at_00466736:
    mov ecx,[esp+0x34]                    ; retail 8b4c2434
.at_0046673a:
    mov eax,[esp+0x44]                    ; retail 8b442444
.at_0046673e:
    db 0x03, 0xf9                    ; add edi,ecx (retail encoding)
.at_00466740:
    db 0x03, 0xd8                    ; add ebx,eax (retail encoding)
.at_00466742:
    mov [esp+0x50],ebx                    ; retail 895c2450
.at_00466746:
    jmp 0x4666b1                    ; retail e966ffffff
.at_0046674b:
    test ebp,ebp                    ; retail 85ed
.at_0046674d:
    db 0x7c, 0x5e                    ; jl 0x4667ad (retail encoding)
.at_0046674f:
    mov ecx,[esp+0x14]                    ; retail 8b4c2414
.at_00466753:
    mov eax,[esp+0x18]                    ; retail 8b442418
.at_00466757:
    movsx edx,word [ecx+0x567f9c]                    ; retail 0fbf919c7f5600
.at_0046675e:
    db 0x3b, 0xc2                    ; cmp eax,edx (retail encoding)
.at_00466760:
    db 0x7d, 0x4b                    ; jnl 0x4667ad (retail encoding)
.at_00466762:
    mov eax,[esp+0x50]                    ; retail 8b442450
.at_00466766:
    test eax,eax                    ; retail 85c0
.at_00466768:
    db 0x7c, 0x43                    ; jl 0x4667ad (retail encoding)
.at_0046676a:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_0046676c:
    movsx ecx,word [eax+0x567f9e]                    ; retail 0fbf889e7f5600
.at_00466773:
    cmp [esp+0x50],ecx                    ; retail 394c2450
.at_00466777:
    db 0x7d, 0x34                    ; jnl 0x4667ad (retail encoding)
.at_00466779:
    mov ecx,[esp+0x20]                    ; retail 8b4c2420
.at_0046677d:
    mov edx,[esp+0x4c]                    ; retail 8b54244c
.at_00466781:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_00466783:
    push edx                    ; retail 52
.at_00466784:
    shl eax,byte 0x4                    ; retail c1e004
.at_00466787:
    db 0x03, 0xc1                    ; add eax,ecx (retail encoding)
.at_00466789:
    lea ecx,[eax+eax*2]                    ; retail 8d0c40
.at_0046678c:
    mov eax,[esp+0x1c]                    ; retail 8b44241c
.at_00466790:
    lea edx,[eax+eax*4]                    ; retail 8d1480
.at_00466793:
    mov eax,[esp+0x54]                    ; retail 8b442454
.at_00466797:
    lea edx,[eax+edx*4]                    ; retail 8d1490
.at_0046679a:
    lea eax,[edx+ecx*8]                    ; retail 8d04ca
.at_0046679d:
    mov ecx,[eax*4+0x567fa0]                    ; retail 8b0c85a07f5600
.at_004667a4:
    push ecx                    ; retail 51
.at_004667a5:
    call 0x4638d0                    ; retail e826d1ffff
.at_004667aa:
    add esp,byte +0x8                    ; retail 83c408
; Advance to the next partition, then finalize the query after the sentinel.
.at_004667ad:
    mov eax,[esp+0x20]                    ; retail 8b442420
.at_004667b1:
    inc eax                    ; retail 40
.at_004667b2:
    test eax,eax                    ; retail 85c0
.at_004667b4:
    mov [esp+0x20],eax                    ; retail 89442420
.at_004667b8:
    jng near 0x466186                    ; retail 0f8ec8f9ffff
.at_004667be:
    mov edx,[esp+0x4c]                    ; retail 8b54244c
.at_004667c2:
    push edx                    ; retail 52
.at_004667c3:
    call 0x463d50                    ; retail e888d5ffff
.at_004667c8:
    add esp,byte +0x4                    ; retail 83c404
.at_004667cb:
    pop edi                    ; retail 5f
.at_004667cc:
    pop esi                    ; retail 5e
.at_004667cd:
    pop ebp                    ; retail 5d
.at_004667ce:
    pop ebx                    ; retail 5b
.at_004667cf:
    add esp,byte +0x38                    ; retail 83c438
.at_004667d2:
    ret                    ; retail c3
