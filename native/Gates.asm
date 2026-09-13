; Windows x64 mid-function gates. All volatile registers and XMM values are
; preserved across the C++ predicate. Both verified sites have 16-byte RSP.
EXTERN ShouldFreeCamera:PROC
EXTERN Threshold:PROC
EXTERN CameraOriginal:QWORD
EXTERN CameraContinue:QWORD
EXTERN ConeContinue:QWORD
.code
SAVE_STATE MACRO
    pushfq
    .allocstack 8
    push rax
    .allocstack 8
    push rcx
    .allocstack 8
    push rdx
    .allocstack 8
    push r8
    .allocstack 8
    push r9
    .allocstack 8
    push r10
    .allocstack 8
    push r11
    .allocstack 8
    sub rsp, 80h
    .allocstack 80h
    movdqu [rsp+20h], xmm0
    movdqu [rsp+30h], xmm1
    movdqu [rsp+40h], xmm2
    movdqu [rsp+50h], xmm3
    movdqu [rsp+60h], xmm4
    movdqu [rsp+70h], xmm5
    .endprolog
ENDM
RESTORE_STATE MACRO
    movdqu xmm0, [rsp+20h]
    movdqu xmm1, [rsp+30h]
    movdqu xmm2, [rsp+40h]
    movdqu xmm3, [rsp+50h]
    movdqu xmm4, [rsp+60h]
    movdqu xmm5, [rsp+70h]
    add rsp, 80h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
ENDM
CameraGate PROC FRAME
    SAVE_STATE
    mov rcx, r15
    call ShouldFreeCamera
    test al, al
    jz vanilla_camera
    RESTORE_STATE
    mov r12b, 1
    mov rsi, 0
    jmp QWORD PTR [CameraContinue]
vanilla_camera:
    RESTORE_STATE
    jmp QWORD PTR [CameraOriginal]
CameraGate ENDP
ConeGate PROC FRAME
    SAVE_STATE
    mov rcx, rbx
    call Threshold
    ; Compare the saved candidate dot with the new threshold. Replace the
    ; saved flags, then restore every register before the game's conditional
    ; jump. Windows x64 has no red zone, so nothing lives below restored RSP.
    movsd xmm1, QWORD PTR [rsp+40h]
    comisd xmm1, xmm0
    pushfq
    pop rax
    mov [rsp+0B8h], rax
    RESTORE_STATE
    jmp QWORD PTR [ConeContinue]
ConeGate ENDP
END
