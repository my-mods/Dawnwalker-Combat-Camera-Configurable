; Windows x64 mid-function gates. All volatile registers and XMM values are
; preserved across the C++ predicate. Attachment leaves use eight extra
; stack bytes so every predicate call has aligned RSP and native shadow space.
EXTERN ShouldFreeCamera:PROC
EXTERN Threshold:PROC
EXTERN ShouldUseFreeDirection:PROC
EXTERN ShouldPreventCameraAttach:PROC
EXTERN AttachDirectOriginal:QWORD
EXTERN AttachDirectContinue:QWORD
EXTERN AttachRequestOriginal:QWORD
EXTERN AttachRequestContinue:QWORD
EXTERN AttachScriptOriginal:QWORD
EXTERN AttachScriptContinue:QWORD
EXTERN AttachCastOriginal:QWORD
EXTERN AttachCastContinue:QWORD
EXTERN AttachAbilityOriginal:QWORD
EXTERN AttachAbilityContinue:QWORD
EXTERN AttachThreatOriginal:QWORD
EXTERN AttachThreatContinue:QWORD
EXTERN AttachCombatOriginal:QWORD
EXTERN AttachCombatContinue:QWORD
EXTERN AttachLockOriginal:QWORD
EXTERN AttachLockContinue:QWORD
EXTERN AttachSelectionOriginal:QWORD
EXTERN AttachSelectionContinue:QWORD
EXTERN CameraOriginal:QWORD
EXTERN CameraContinue:QWORD
EXTERN ConeContinue:QWORD
EXTERN ForwardOriginal:QWORD
EXTERN ForwardContinue:QWORD
.code
SAVE_STATE MACRO padding:=<0>
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
    sub rsp, 80h+padding
    .allocstack 80h+padding
    movdqu [rsp+20h], xmm0
    movdqu [rsp+30h], xmm1
    movdqu [rsp+40h], xmm2
    movdqu [rsp+50h], xmm3
    movdqu [rsp+60h], xmm4
    movdqu [rsp+70h], xmm5
    .endprolog
ENDM
RESTORE_STATE MACRO padding:=<0>
    movdqu xmm0, [rsp+20h]
    movdqu xmm1, [rsp+30h]
    movdqu xmm2, [rsp+40h]
    movdqu xmm3, [rsp+50h]
    movdqu xmm4, [rsp+60h]
    movdqu xmm5, [rsp+70h]
    add rsp, 80h+padding
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
ForwardGate PROC FRAME
    SAVE_STATE
    mov rcx, rdi
    call ShouldUseFreeDirection
    test al, al
    jz vanilla_direction
    RESTORE_STATE
    ; The game has already calculated its normalized planar forward vector.
    ; Use its existing no-target result, bypassing both combat/action targets.
    jmp QWORD PTR [ForwardContinue]
vanilla_direction:
    RESTORE_STATE
    jmp QWORD PTR [ForwardOriginal]
ForwardGate ENDP
; Skip only the attachment store. Every surrounding instruction, including
; target changes, ability work and broadcasts, continues through native code.
ATTACH_GATE MACRO gateName, ownerRegister, padding
LOCAL native_store
gateName&Gate PROC FRAME
    SAVE_STATE padding
    mov rcx, ownerRegister
    call ShouldPreventCameraAttach
    test al, al
    jz native_store
    RESTORE_STATE padding
    jmp QWORD PTR [gateName&Continue]
native_store:
    RESTORE_STATE padding
    jmp QWORD PTR [gateName&Original]
gateName&Gate ENDP
ENDM
ATTACH_GATE AttachDirect, rcx, 8
ATTACH_GATE AttachRequest, rcx, 8
ATTACH_GATE AttachScript, rbp, 0
ATTACH_GATE AttachCast, rax, 0
ATTACH_GATE AttachAbility, rax, 0
ATTACH_GATE AttachThreat, rax, 0
ATTACH_GATE AttachCombat, rdi, 0
ATTACH_GATE AttachLock, rcx, 0
ATTACH_GATE AttachSelection, rax, 0
END
