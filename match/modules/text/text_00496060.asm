BITS 32
org 0x00496060

Skater_PositionWritePath:
    sub esp, 0xa8
    push esi
    db 0x8b, 0xf1                 ; mov esi, ecx (retail 8B /r form)
    mov eax, [esi + 0x3200]
    test eax, eax
    jz short .collision_resolution

    ; Fast path: collision resolution is disabled, so commit XYZ directly.
    mov eax, [esp + 0xb0]
    mov ecx, [esp + 0xb4]
    mov edx, [esp + 0xb8]
    mov [esi + 0x08], eax
    mov [esi + 0x0c], ecx
    mov [esi + 0x10], edx
    pop esi
    add esp, 0xa8
    ret 0x0c

.collision_resolution:
    mov eax, [0x00567c7c]
    mov dword [0x00567c7c], 1
    mov ecx, [esi + 0x08]
    mov edx, [esi + 0x0c]
    push ebx
    mov ebx, [esp + 0xbc]         ; proposed Z
    push ebp
    mov ebp, [esp + 0xbc]         ; proposed Y
    mov [esp + 0x10], ecx         ; current X
    push edi
    mov edi, [esp + 0xbc]         ; proposed X
    mov [esp + 0x10], eax         ; saved collision-query guard
    mov eax, [esi + 0x10]
    lea ecx, [esp + 0x14]
    push ecx
    mov [esp + 0x1c], edx         ; current Y
    mov [esp + 0x20], eax         ; current Z
    mov [esp + 0x24], edi         ; candidate X
    mov [esp + 0x28], ebp         ; candidate Y
    mov [esp + 0x2c], ebx         ; candidate Z
    call 0x004624d0
    lea edx, [esp + 0x18]
    push byte 1
    push edx
    call 0x00466090
    mov eax, [esp + 0x88]
    add esp, byte 0x0c
    test eax, eax
    jz near .commit_candidate

    ; Candidate 2: current X, proposed Y/Z.
    mov eax, [esi + 0x08]
    lea ecx, [esp + 0x14]
    push ecx
    mov [esp + 0x24], eax
    call 0x004624d0
    lea edx, [esp + 0x18]
    push byte 1
    push edx
    call 0x00466090
    mov eax, [esp + 0x88]
    add esp, byte 0x0c
    test eax, eax
    jz near .commit_candidate

    ; Candidate 3: proposed X/Y, current Z.
    mov eax, [esi + 0x10]
    lea ecx, [esp + 0x14]
    push ecx
    mov [esp + 0x24], edi
    mov [esp + 0x2c], eax
    call 0x004624d0
    lea edx, [esp + 0x18]
    push byte 1
    push edx
    call 0x00466090
    mov eax, [esp + 0x88]
    add esp, byte 0x0c
    test eax, eax
    jz near .commit_candidate

    ; Candidate 4: proposed X, current Y, proposed Z.
    mov eax, [esi + 0x0c]
    lea ecx, [esp + 0x14]
    push ecx
    mov [esp + 0x2c], ebx
    mov [esp + 0x28], eax
    call 0x004624d0
    lea edx, [esp + 0x18]
    push byte 1
    push edx
    call 0x00466090
    mov eax, [esp + 0x88]
    add esp, byte 0x0c
    test eax, eax
    jz near .commit_candidate

    ; Candidate 5: proposed X, current Y/Z.
    mov eax, [esi + 0x0c]
    mov ecx, [esi + 0x10]
    lea edx, [esp + 0x14]
    mov [esp + 0x20], edi
    push edx
    mov [esp + 0x28], eax
    mov [esp + 0x2c], ecx
    call 0x004624d0
    lea eax, [esp + 0x18]
    push byte 1
    push eax
    call 0x00466090
    mov eax, [esp + 0x88]
    add esp, byte 0x0c
    test eax, eax
    jz short .commit_candidate

    ; Candidate 6: current X/Y, proposed Z.
    mov ecx, [esi + 0x08]
    mov edx, [esi + 0x0c]
    lea eax, [esp + 0x14]
    mov [esp + 0x20], ecx
    push eax
    mov [esp + 0x28], edx
    mov [esp + 0x2c], ebx
    call 0x004624d0
    lea ecx, [esp + 0x18]
    push byte 1
    push ecx
    call 0x00466090
    mov eax, [esp + 0x88]
    add esp, byte 0x0c
    test eax, eax
    jz short .commit_candidate

    ; Candidate 7: current X, proposed Y, current Z.
    mov edx, [esi + 0x08]
    mov eax, [esi + 0x10]
    lea ecx, [esp + 0x14]
    mov [esp + 0x20], edx
    push ecx
    mov [esp + 0x28], ebp
    mov [esp + 0x2c], eax
    call 0x004624d0
    lea edx, [esp + 0x18]
    push byte 1
    push edx
    call 0x00466090
    mov eax, [esp + 0x88]
    add esp, byte 0x0c
    test eax, eax
    jz short .commit_candidate

    ; All queries rejected: keep current Y. Candidate X/Z already contain
    ; current X/Z after the final substitution.
    mov eax, [esi + 0x0c]
    jmp short .commit_y

.commit_candidate:
    mov eax, [esp + 0x24]
.commit_y:
    mov ecx, [esp + 0x20]
    mov edx, [esp + 0x28]
    mov [esi + 0x0c], eax
    mov eax, [esp + 0x10]
    pop edi
    pop ebp
    mov [esi + 0x08], ecx
    mov [esi + 0x10], edx
    pop ebx
    mov [0x00567c7c], eax
    pop esi
    add esp, strict dword 0xa8
    ret 0x0c
