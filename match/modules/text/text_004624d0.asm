BITS 32
org 0x004624d0

CollisionQuery_Initialize:
.at_004624d0:
    sub esp,byte +0xc                    ; retail 83ec0c
.at_004624d3:
    push ebx                    ; retail 53
.at_004624d4:
    push ebp                    ; retail 55
.at_004624d5:
    push esi                    ; retail 56
.at_004624d6:
    mov esi,[esp+0x1c]                    ; retail 8b74241c
.at_004624da:
    mov eax,0x7fffffff                    ; retail b8ffffff7f
.at_004624df:
    push edi                    ; retail 57
.at_004624e0:
    mov ecx,[esi]                    ; retail 8b0e
.at_004624e2:
    mov ebp,[esi+0x8]                    ; retail 8b6e08
.at_004624e5:
    mov [esi+0x40],eax                    ; retail 894640
.at_004624e8:
    mov [esi+0x8c],eax                    ; retail 89868c000000
.at_004624ee:
    mov eax,[esi+0xc]                    ; retail 8b460c
.at_004624f1:
    mov edi,[esi+0x10]                    ; retail 8b7e10
.at_004624f4:
    db 0x2b, 0xc1                    ; sub eax,ecx (retail encoding)
.at_004624f6:
    mov ecx,[esi+0x14]                    ; retail 8b4e14
.at_004624f9:
    db 0x2b, 0xcd                    ; sub ecx,ebp (retail encoding)
.at_004624fb:
    mov ebx,[esi+0x4]                    ; retail 8b5e04
.at_004624fe:
    sar ecx,byte 0xc                    ; retail c1f90c
.at_00462501:
    sar eax,byte 0xc                    ; retail c1f80c
.at_00462504:
    movsx ecx,cx                    ; retail 0fbfc9
.at_00462507:
    movsx ebp,ax                    ; retail 0fbfe8
.at_0046250a:
    db 0x8b, 0xc1                    ; mov eax,ecx (retail encoding)
.at_0046250c:
    mov [esp+0x10],ebp                    ; retail 896c2410
.at_00462510:
    imul ebp,ebp                    ; retail 0fafed
.at_00462513:
    imul eax,ecx                    ; retail 0fafc1
.at_00462516:
    db 0x2b, 0xfb                    ; sub edi,ebx (retail encoding)
.at_00462518:
    db 0x33, 0xd2                    ; xor edx,edx (retail encoding)
.at_0046251a:
    sar edi,byte 0xc                    ; retail c1ff0c
.at_0046251d:
    db 0x03, 0xe8                    ; add ebp,eax (retail encoding)
.at_0046251f:
    mov [esi+0x68],edx                    ; retail 895668
.at_00462522:
    mov [esi+0x80],edx                    ; retail 899680000000
.at_00462528:
    mov dword [esi+0x84],0xffffffff                    ; retail c78684000000ffffffff
.at_00462532:
    mov [esi+0x88],dl                    ; retail 889688000000
.at_00462538:
    mov [esi+0x89],dl                    ; retail 889689000000
.at_0046253e:
    mov [esp+0x14],ecx                    ; retail 894c2414
.at_00462542:
    jz near 0x4627bd                    ; retail 0f8475020000
; General path: normalize the horizontal delta and construct the query basis.
.at_00462548:
    push ebp                    ; retail 55
.at_00462549:
    call 0x4e22b0                    ; retail e862fd0700
.at_0046254e:
    mov eax,[0x6a3f04]                    ; retail a1043f6a00
.at_00462553:
    db 0x8b, 0xd5                    ; mov edx,ebp (retail encoding)
.at_00462555:
    lea ecx,[eax-0x1]                    ; retail 8d48ff
.at_00462558:
    db 0x8b, 0xd9                    ; mov ebx,ecx (retail encoding)
.at_0046255a:
    and ecx,byte +0x1e                    ; retail 83e11e
.at_0046255d:
    shl edx,cl                    ; retail d3e2
.at_0046255f:
    sar ebx,1                    ; retail d1fb
.at_00462561:
    push edx                    ; retail 52
.at_00462562:
    call 0x4f53b0                    ; retail e8492e0900
.at_00462567:
    mov [esp+0x28],eax                    ; retail 89442428
.at_0046256b:
    mov eax,[esp+0x18]                    ; retail 8b442418
.at_0046256f:
    lea ecx,[ebx+0xc]                    ; retail 8d4b0c
.at_00462572:
    mov word [dword 0x567bb2],0x0                    ; retail 66c705b27b56000000
.at_0046257b:
    shl eax,cl                    ; retail d3e0
.at_0046257d:
    movsx edi,di                    ; retail 0fbfff
.at_00462580:
    cdq                    ; retail 99
.at_00462581:
    idiv dword [esp+0x28]                    ; retail f77c2428
.at_00462585:
    mov [esp+0x20],ecx                    ; retail 894c2420
.at_00462589:
    mov [esp+0x20],edi                    ; retail 897c2420
.at_0046258d:
    imul edi,edi                    ; retail 0fafff
.at_00462590:
    db 0x03, 0xfd                    ; add edi,ebp (retail encoding)
.at_00462592:
    mov word [dword 0x567bb8],0x1000                    ; retail 66c705b87b56000010
.at_0046259b:
    push edi                    ; retail 57
.at_0046259c:
    mov [esp+0x1c],eax                    ; retail 8944241c
.at_004625a0:
    mov eax,[esp+0x20]                    ; retail 8b442420
.at_004625a4:
    shl eax,cl                    ; retail d3e0
.at_004625a6:
    mov ecx,[esp+0x1c]                    ; retail 8b4c241c
.at_004625aa:
    mov [dword 0x567bbc],cx                    ; retail 66890dbc7b5600
.at_004625b1:
    cdq                    ; retail 99
.at_004625b2:
    idiv dword [esp+0x2c]                    ; retail f77c242c
.at_004625b6:
    db 0x8b, 0xd1                    ; mov edx,ecx (retail encoding)
.at_004625b8:
    neg edx                    ; retail f7da
.at_004625ba:
    mov [dword 0x567bb4],dx                    ; retail 668915b47b5600
.at_004625c1:
    db 0x33, 0xd2                    ; xor edx,edx (retail encoding)
.at_004625c3:
    mov [dword 0x567bb6],dx                    ; retail 668915b67b5600
.at_004625ca:
    mov [dword 0x567bba],dx                    ; retail 668915ba7b5600
.at_004625d1:
    mov [dword 0x567bbe],dx                    ; retail 668915be7b5600
.at_004625d8:
    mov [0x567bb0],ax                    ; retail 66a3b07b5600
.at_004625de:
    mov [0x567bc0],ax                    ; retail 66a3c07b5600
.at_004625e4:
    call 0x4e22b0                    ; retail e8c7fc0700
.at_004625e9:
    mov eax,[0x6a3f04]                    ; retail a1043f6a00
.at_004625ee:
    lea ecx,[eax-0x1]                    ; retail 8d48ff
.at_004625f1:
    db 0x8b, 0xe9                    ; mov ebp,ecx (retail encoding)
.at_004625f3:
    and ecx,byte +0x1e                    ; retail 83e11e
.at_004625f6:
    shl edi,cl                    ; retail d3e7
.at_004625f8:
    sar ebp,1                    ; retail d1fd
.at_004625fa:
    push edi                    ; retail 57
.at_004625fb:
    call 0x4f53b0                    ; retail e8b02d0900
.at_00462600:
    db 0x8b, 0xcd                    ; mov ecx,ebp (retail encoding)
.at_00462602:
    db 0x8b, 0xf8                    ; mov edi,eax (retail encoding)
.at_00462604:
    sar eax,cl                    ; retail d3f8
.at_00462606:
    lea ecx,[ebp+0xc]                    ; retail 8d4d0c
.at_00462609:
    add esp,byte +0x10                    ; retail 83c410
.at_0046260c:
    mov [esi+0x44],eax                    ; retail 894644
.at_0046260f:
    mov eax,[esp+0x18]                    ; retail 8b442418
.at_00462613:
    shl eax,cl                    ; retail d3e0
.at_00462615:
    db 0x8b, 0xcb                    ; mov ecx,ebx (retail encoding)
.at_00462617:
    lea ebx,[esi+0x48]                    ; retail 8d5e48
.at_0046261a:
    db 0x2b, 0xcd                    ; sub ecx,ebp (retail encoding)
.at_0046261c:
    cdq                    ; retail 99
.at_0046261d:
    idiv edi                    ; retail f7ff
.at_0046261f:
    shl edi,cl                    ; retail d3e7
.at_00462621:
    db 0x8b, 0xcb                    ; mov ecx,ebx (retail encoding)
.at_00462623:
    mov word [ecx],0x1000                    ; retail 66c7010010
.at_00462628:
    add ecx,byte +0x2                    ; retail 83c102
.at_0046262b:
    mov word [ecx],0x0                    ; retail 66c7010000
.at_00462630:
    add ecx,byte +0x2                    ; retail 83c102
.at_00462633:
    mov word [ecx],0x0                    ; retail 66c7010000
.at_00462638:
    add ecx,byte +0x2                    ; retail 83c102
.at_0046263b:
    mov word [ecx],0x0                    ; retail 66c7010000
.at_00462640:
    add ecx,byte +0x2                    ; retail 83c102
.at_00462643:
    mov [esp+0x10],eax                    ; retail 89442410
.at_00462647:
    mov eax,[esp+0x20]                    ; retail 8b442420
.at_0046264b:
    shl eax,byte 0xc                    ; retail c1e00c
.at_0046264e:
    cdq                    ; retail 99
.at_0046264f:
    idiv edi                    ; retail f7ff
.at_00462651:
    mov edx,[esp+0x10]                    ; retail 8b542410
.at_00462655:
    add ecx,byte +0x2                    ; retail 83c102
.at_00462658:
    mov [ecx-0x2],ax                    ; retail 668941fe
.at_0046265c:
    db 0x8b, 0xfa                    ; mov edi,edx (retail encoding)
.at_0046265e:
    add ecx,byte +0x2                    ; retail 83c102
.at_00462661:
    neg edi                    ; retail f7df
.at_00462663:
    mov [ecx-0x2],di                    ; retail 668979fe
.at_00462667:
    mov word [ecx],0x0                    ; retail 66c7010000
.at_0046266c:
    add ecx,byte +0x2                    ; retail 83c102
.at_0046266f:
    mov [ecx],dx                    ; retail 668911
.at_00462672:
    mov [ecx+0x2],ax                    ; retail 66894102
.at_00462676:
    mov cx,[ebx]                    ; retail 668b0b
.at_00462679:
    mov [dword 0x6a3e10],cx                    ; retail 66890d103e6a00
.at_00462680:
    mov dx,[esi+0x4a]                    ; retail 668b564a
.at_00462684:
    mov [dword 0x6a3e12],dx                    ; retail 668915123e6a00
.at_0046268b:
    mov ax,[esi+0x4c]                    ; retail 668b464c
.at_0046268f:
    mov [0x6a3e14],ax                    ; retail 66a3143e6a00
.at_00462695:
    mov cx,[esi+0x4e]                    ; retail 668b4e4e
.at_00462699:
    mov [dword 0x6a3e16],cx                    ; retail 66890d163e6a00
.at_004626a0:
    mov dx,[esi+0x50]                    ; retail 668b5650
.at_004626a4:
    mov [dword 0x6a3e18],dx                    ; retail 668915183e6a00
.at_004626ab:
    mov ax,[esi+0x52]                    ; retail 668b4652
.at_004626af:
    mov [0x6a3e1a],ax                    ; retail 66a31a3e6a00
.at_004626b5:
    mov cx,[esi+0x54]                    ; retail 668b4e54
.at_004626b9:
    mov [dword 0x6a3e1c],cx                    ; retail 66890d1c3e6a00
.at_004626c0:
    mov dx,[esi+0x56]                    ; retail 668b5656
.at_004626c4:
    mov cx,[dword 0x567bb0]                    ; retail 668b0db07b5600
.at_004626cb:
    mov [dword 0x6a3e1e],dx                    ; retail 6689151e3e6a00
.at_004626d2:
    mov ax,[esi+0x58]                    ; retail 668b4658
.at_004626d6:
    mov dx,[dword 0x567bb6]                    ; retail 668b15b67b5600
.at_004626dd:
    mov [0x6a3e20],ax                    ; retail 66a3203e6a00
.at_004626e3:
    mov ax,[0x567bbc]                    ; retail 66a1bc7b5600
.at_004626e9:
    mov [dword 0x6a3eb0],cx                    ; retail 66890db03e6a00
.at_004626f0:
    mov [dword 0x6a3eb2],dx                    ; retail 668915b23e6a00
.at_004626f7:
    mov [0x6a3eb4],ax                    ; retail 66a3b43e6a00
.at_004626fd:
    call 0x4e3130                    ; retail e82e0a0800
.at_00462702:
    mov cx,[dword 0x6a3eb0]                    ; retail 668b0db03e6a00
.at_00462709:
    mov [ebx],cx                    ; retail 66890b
.at_0046270c:
    mov dx,[dword 0x6a3eb2]                    ; retail 668b15b23e6a00
.at_00462713:
    mov [esi+0x4e],dx                    ; retail 6689564e
.at_00462717:
    mov ax,[0x6a3eb4]                    ; retail 66a1b43e6a00
.at_0046271d:
    mov [esi+0x54],ax                    ; retail 66894654
.at_00462721:
    mov cx,[dword 0x567bb2]                    ; retail 668b0db27b5600
.at_00462728:
    mov dx,[dword 0x567bb8]                    ; retail 668b15b87b5600
.at_0046272f:
    mov ax,[0x567bbe]                    ; retail 66a1be7b5600
.at_00462735:
    mov [dword 0x6a3eb0],cx                    ; retail 66890db03e6a00
.at_0046273c:
    mov [dword 0x6a3eb2],dx                    ; retail 668915b23e6a00
.at_00462743:
    mov [0x6a3eb4],ax                    ; retail 66a3b43e6a00
.at_00462749:
    call 0x4e3130                    ; retail e8e2090800
.at_0046274e:
    mov cx,[dword 0x6a3eb0]                    ; retail 668b0db03e6a00
.at_00462755:
    mov [esi+0x4a],cx                    ; retail 66894e4a
.at_00462759:
    mov dx,[dword 0x6a3eb2]                    ; retail 668b15b23e6a00
.at_00462760:
    mov [esi+0x50],dx                    ; retail 66895650
.at_00462764:
    mov ax,[0x6a3eb4]                    ; retail 66a1b43e6a00
.at_0046276a:
    mov [esi+0x56],ax                    ; retail 66894656
.at_0046276e:
    mov cx,[dword 0x567bb4]                    ; retail 668b0db47b5600
.at_00462775:
    mov dx,[dword 0x567bba]                    ; retail 668b15ba7b5600
.at_0046277c:
    mov ax,[0x567bc0]                    ; retail 66a1c07b5600
.at_00462782:
    mov [dword 0x6a3eb0],cx                    ; retail 66890db03e6a00
.at_00462789:
    mov [dword 0x6a3eb2],dx                    ; retail 668915b23e6a00
.at_00462790:
    mov [0x6a3eb4],ax                    ; retail 66a3b43e6a00
.at_00462796:
    call 0x4e3130                    ; retail e895090800
.at_0046279b:
    mov cx,[dword 0x6a3eb0]                    ; retail 668b0db03e6a00
.at_004627a2:
    mov [esi+0x4c],cx                    ; retail 66894e4c
.at_004627a6:
    mov dx,[dword 0x6a3eb2]                    ; retail 668b15b23e6a00
.at_004627ad:
    mov [esi+0x52],dx                    ; retail 66895652
.at_004627b1:
    mov ax,[0x6a3eb4]                    ; retail 66a1b43e6a00
.at_004627b7:
    mov [esi+0x58],ax                    ; retail 66894658
.at_004627bb:
    db 0xeb, 0x62                    ; jmp short 0x46281f (retail encoding)
; Axis-aligned path: select the signed Z basis directly.
.at_004627bd:
    db 0x66, 0x3b, 0xfa                    ; cmp di,dx (retail encoding)
.at_004627c0:
    db 0x7d, 0x0f                    ; jnl 0x4627d1 (retail encoding)
.at_004627c2:
    movsx eax,di                    ; retail 0fbfc7
.at_004627c5:
    neg eax                    ; retail f7d8
.at_004627c7:
    mov ecx,0xfffff000                    ; retail b900f0ffff
.at_004627cc:
    mov [esi+0x44],eax                    ; retail 894644
.at_004627cf:
    db 0xeb, 0x12                    ; jmp short 0x4627e3 (retail encoding)
.at_004627d1:
    movsx eax,di                    ; retail 0fbfc7
.at_004627d4:
    mov ecx,0x1000                    ; retail b900100000
.at_004627d9:
    mov [esi+0x44],eax                    ; retail 894644
.at_004627dc:
    mov byte [esi+0x89],0x1                    ; retail c6868900000001
.at_004627e3:
    lea ebx,[esi+0x48]                    ; retail 8d5e48
.at_004627e6:
    db 0x8b, 0xf9                    ; mov edi,ecx (retail encoding)
.at_004627e8:
    db 0x8b, 0xc3                    ; mov eax,ebx (retail encoding)
.at_004627ea:
    neg edi                    ; retail f7df
.at_004627ec:
    mov word [eax],0x1000                    ; retail 66c7000010
.at_004627f1:
    add eax,byte +0x2                    ; retail 83c002
.at_004627f4:
    mov [eax],dx                    ; retail 668910
.at_004627f7:
    add eax,byte +0x2                    ; retail 83c002
.at_004627fa:
    mov [eax],dx                    ; retail 668910
.at_004627fd:
    add eax,byte +0x2                    ; retail 83c002
.at_00462800:
    mov [eax],dx                    ; retail 668910
.at_00462803:
    add eax,byte +0x2                    ; retail 83c002
.at_00462806:
    mov [eax],dx                    ; retail 668910
.at_00462809:
    add eax,byte +0x2                    ; retail 83c002
.at_0046280c:
    mov [eax],di                    ; retail 668938
.at_0046280f:
    add eax,byte +0x2                    ; retail 83c002
.at_00462812:
    mov [eax],dx                    ; retail 668910
.at_00462815:
    add eax,byte +0x2                    ; retail 83c002
.at_00462818:
    mov [eax],cx                    ; retail 668908
.at_0046281b:
    mov [eax+0x2],dx                    ; retail 66895002
; Publish the 3x3 fixed-point basis used by the collision engine.
.at_0046281f:
    mov cx,[ebx]                    ; retail 668b0b
.at_00462822:
    mov [dword 0x6a3e10],cx                    ; retail 66890d103e6a00
.at_00462829:
    mov dx,[esi+0x4a]                    ; retail 668b564a
.at_0046282d:
    mov [dword 0x6a3e12],dx                    ; retail 668915123e6a00
.at_00462834:
    mov ax,[esi+0x4c]                    ; retail 668b464c
.at_00462838:
    mov [0x6a3e14],ax                    ; retail 66a3143e6a00
.at_0046283e:
    mov cx,[esi+0x4e]                    ; retail 668b4e4e
.at_00462842:
    mov [dword 0x6a3e16],cx                    ; retail 66890d163e6a00
.at_00462849:
    mov dx,[esi+0x50]                    ; retail 668b5650
.at_0046284d:
    mov [dword 0x6a3e18],dx                    ; retail 668915183e6a00
.at_00462854:
    mov ax,[esi+0x52]                    ; retail 668b4652
.at_00462858:
    mov [0x6a3e1a],ax                    ; retail 66a31a3e6a00
.at_0046285e:
    mov cx,[esi+0x54]                    ; retail 668b4e54
.at_00462862:
    mov [dword 0x6a3e1c],cx                    ; retail 66890d1c3e6a00
.at_00462869:
    mov dx,[esi+0x56]                    ; retail 668b5656
.at_0046286d:
    mov [dword 0x6a3e1e],dx                    ; retail 6689151e3e6a00
.at_00462874:
    mov ax,[esi+0x58]                    ; retail 668b4658
.at_00462878:
    mov [0x6a3e20],ax                    ; retail 66a3203e6a00
; Cache component-wise minima/maxima for the segment bounds.
.at_0046287e:
    mov eax,[esi]                    ; retail 8b06
.at_00462880:
    mov ecx,[esi+0xc]                    ; retail 8b4e0c
.at_00462883:
    db 0x3b, 0xc1                    ; cmp eax,ecx (retail encoding)
.at_00462885:
    db 0x7d, 0x0b                    ; jnl 0x462892 (retail encoding)
.at_00462887:
    mov [esi+0x18],eax                    ; retail 894618
.at_0046288a:
    mov ecx,[esi+0xc]                    ; retail 8b4e0c
.at_0046288d:
    mov [esi+0x24],ecx                    ; retail 894e24
.at_00462890:
    db 0xeb, 0x06                    ; jmp short 0x462898 (retail encoding)
.at_00462892:
    mov [esi+0x18],ecx                    ; retail 894e18
.at_00462895:
    mov [esi+0x24],eax                    ; retail 894624
.at_00462898:
    mov eax,[esi+0x4]                    ; retail 8b4604
.at_0046289b:
    mov ecx,[esi+0x10]                    ; retail 8b4e10
.at_0046289e:
    db 0x3b, 0xc1                    ; cmp eax,ecx (retail encoding)
.at_004628a0:
    db 0x7d, 0x0b                    ; jnl 0x4628ad (retail encoding)
.at_004628a2:
    mov [esi+0x1c],eax                    ; retail 89461c
.at_004628a5:
    mov edx,[esi+0x10]                    ; retail 8b5610
.at_004628a8:
    mov [esi+0x28],edx                    ; retail 895628
.at_004628ab:
    db 0xeb, 0x06                    ; jmp short 0x4628b3 (retail encoding)
.at_004628ad:
    mov [esi+0x1c],ecx                    ; retail 894e1c
.at_004628b0:
    mov [esi+0x28],eax                    ; retail 894628
.at_004628b3:
    mov eax,[esi+0x8]                    ; retail 8b4608
.at_004628b6:
    mov ecx,[esi+0x14]                    ; retail 8b4e14
.at_004628b9:
    db 0x3b, 0xc1                    ; cmp eax,ecx (retail encoding)
.at_004628bb:
    db 0x7d, 0x08                    ; jnl 0x4628c5 (retail encoding)
.at_004628bd:
    mov [esi+0x20],eax                    ; retail 894620
.at_004628c0:
    mov eax,[esi+0x14]                    ; retail 8b4614
.at_004628c3:
    db 0xeb, 0x03                    ; jmp short 0x4628c8 (retail encoding)
.at_004628c5:
    mov [esi+0x20],ecx                    ; retail 894e20
.at_004628c8:
    mov [esi+0x2c],eax                    ; retail 89462c
.at_004628cb:
    call 0x462490                    ; retail e8c0fbffff
.at_004628d0:
    mov cx,[dword 0x533ab4]                    ; retail 668b0db43a5300
.at_004628d7:
    pop edi                    ; retail 5f
.at_004628d8:
    mov [esi+0x8a],cx                    ; retail 66898e8a000000
.at_004628df:
    pop esi                    ; retail 5e
.at_004628e0:
    pop ebp                    ; retail 5d
.at_004628e1:
    pop ebx                    ; retail 5b
.at_004628e2:
    add esp,byte +0xc                    ; retail 83c40c
.at_004628e5:
    ret                    ; retail c3
