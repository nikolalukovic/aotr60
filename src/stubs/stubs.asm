; AotR60 code stubs. Every patch site in tools/sites.json jumps or calls into one of the procedures below.
;
; Rules (PLAN §2):
;  - Every stub has an explicit 30-mode path that reproduces the displaced original instructions exactly
;    (registers, stack, live flags). 60-mode behaviour is selected only by DLL globals (runtime.h).
;  - Mid-stream stubs touch nothing but what their site contract (sites.json "live_after") allows; when they
;    call C++ they save flags, all GPRs and XMM0-7 around the call (SAVE_ALL / RESTORE_ALL).
;  - Absolute game addresses are reached through the T_xxxxxx pointer table (indirect jmp/call), never through
;    rel32 fixups, so the stub code itself is position independent with respect to the game image.
;  - Per-site counters g_siteRun / g_siteSkip are indexed by the generated IDX_* constants (sites.gen.inc).

.686p
.xmm
.model flat, C
option casemap:none
ASSUME FS:NOTHING

INCLUDE sites.gen.inc

; ---------------------------------------------------------------------------------------------------------------
; DLL globals (runtime.cpp) and C++ helpers
; ---------------------------------------------------------------------------------------------------------------
EXTERN g_m60:BYTE, g_uiTick:BYTE, g_inB:BYTE, g_inClientUpdate:BYTE, g_skipB:BYTE, g_forceHalt:BYTE
EXTERN g_mainTid:DWORD, g_renderId:DWORD
EXTERN g_syncOwed:DWORD, g_syncHalfPar:DWORD
EXTERN g_pwActive:DWORD, g_pw1Open:BYTE, g_pw2Open:BYTE, g_fracSaved:DWORD, g_swapActive:BYTE
EXTERN g_mDrawRid:DWORD, g_mDrawAM:DWORD, g_gapDevLost:BYTE, g_gapReset:BYTE
EXTERN g_shoreStepA:DWORD, g_shoreStepB:DWORD
EXTERN g_siteRun:DWORD, g_siteSkip:DWORD
EXTERN g_tmFFin60:DWORD, g_anomaly:DWORD, g_unknownPath:DWORD
EXTERN g_vt188Calls:DWORD, g_vt188LastRet:DWORD, g_palBadRet:DWORD, g_iguiBadRet:DWORD
EXTERN g_lp4b:BYTE, g_uiPart4b:BYTE, g_wanim4b:BYTE
EXTERN g_featPresent:BYTE, g_radarAFrame:DWORD

EXTERN OnPreRender:PROC, OnPostRender:PROC, OnEngineReset:PROC
EXTERN AotR60_Pacer:PROC, AotR60_PresentSkip:PROC
EXTERN CamSwapToMk_B:PROC, S2_RecordMkAndOpen:PROC, SceneOpen_A:PROC, SceneRestore:PROC, CamSwapEndGuard:PROC

; g_anomaly bits (keep in sync with runtime.h)
ANOM_PAL_CALLER  EQU 1
ANOM_IGUI_CALLER EQU 2
; g_unknownPath bits
UP_VT188         EQU 1

; ---------------------------------------------------------------------------------------------------------------
; Helpers
; ---------------------------------------------------------------------------------------------------------------
RUNCNT MACRO idx
    inc dword ptr [g_siteRun + (idx) * 4]
ENDM

SKIPCNT MACRO idx
    inc dword ptr [g_siteSkip + (idx) * 4]
ENDM

; Flags, every GPR and XMM0-7 around a C++ call made from the middle of game code.
SAVE_ALL MACRO
    pushfd
    pushad
    sub esp, 128
    movdqu xmmword ptr [esp + 0], xmm0
    movdqu xmmword ptr [esp + 16], xmm1
    movdqu xmmword ptr [esp + 32], xmm2
    movdqu xmmword ptr [esp + 48], xmm3
    movdqu xmmword ptr [esp + 64], xmm4
    movdqu xmmword ptr [esp + 80], xmm5
    movdqu xmmword ptr [esp + 96], xmm6
    movdqu xmmword ptr [esp + 112], xmm7
    cld
ENDM

RESTORE_ALL MACRO
    movdqu xmm0, xmmword ptr [esp + 0]
    movdqu xmm1, xmmword ptr [esp + 16]
    movdqu xmm2, xmmword ptr [esp + 32]
    movdqu xmm3, xmmword ptr [esp + 48]
    movdqu xmm4, xmmword ptr [esp + 64]
    movdqu xmm5, xmmword ptr [esp + 80]
    movdqu xmm6, xmmword ptr [esp + 96]
    movdqu xmm7, xmmword ptr [esp + 112]
    add esp, 128
    popad
    popfd
ENDM

; ZF=1 when running on the game's main thread. Clobbers nothing but flags (EAX is saved).
IS_MAIN_KEEP_EAX MACRO
    push eax
    mov eax, fs:[24h]
    cmp eax, [g_mainTid]
    pop eax
ENDM

; ZF=1 when running on the game's main thread. Clobbers reg.
IS_MAIN MACRO reg
    mov reg, fs:[24h]
    cmp reg, [g_mainTid]
ENDM

; ---------------------------------------------------------------------------------------------------------------
; Absolute game addresses (targets of indirect jumps / calls)
; ---------------------------------------------------------------------------------------------------------------
.const
DEFTARGET MACRO addr
T_&addr DD 0&addr&h
ENDM

DEFTARGET 632534
DEFTARGET 635D11
DEFTARGET 62B385
DEFTARGET 83B471
DEFTARGET 8392A7
DEFTARGET 50EB3C
DEFTARGET 645750
DEFTARGET 782C56
DEFTARGET 65C1EA
DEFTARGET 64870A
DEFTARGET 6D7B33
DEFTARGET 6C0E4D
DEFTARGET 444CDA
DEFTARGET 444CFD
DEFTARGET 6A1F4D
DEFTARGET 4FDB58
DEFTARGET 4684FB
DEFTARGET 4E83F6
DEFTARGET 4E83FE
DEFTARGET 732AB1
DEFTARGET 4B4699
DEFTARGET 44B8D8
DEFTARGET 44BC5F
DEFTARGET 48A953
DEFTARGET 67BDC5
DEFTARGET 49405A
DEFTARGET 492C35
DEFTARGET 63A19C
DEFTARGET 63A1F5
DEFTARGET 522006
DEFTARGET 4430BB
DEFTARGET 4438DA
DEFTARGET 44B977
DEFTARGET 44B98C
DEFTARGET 44B9E3
DEFTARGET 48C701
DEFTARGET 48BE02
DEFTARGET 48BD25
DEFTARGET 48C706
DEFTARGET 48C76B
DEFTARGET 449DB0
DEFTARGET 5208D0
DEFTARGET 48B7B6
DEFTARGET 4FD25A
DEFTARGET 4CF434
DEFTARGET 4CF3CF
DEFTARGET 4CF40E
DEFTARGET 4CA5FC
DEFTARGET 4CA59E
DEFTARGET 4CDEC9
DEFTARGET 4CDE93
DEFTARGET 4CC201
DEFTARGET 4CC323
DEFTARGET 4CC518
DEFTARGET 4CC4B9
DEFTARGET 4B1561
DEFTARGET 4B3249
DEFTARGET 4B326B
DEFTARGET 46D665
DEFTARGET 46D7C9
DEFTARGET 65CFAB
DEFTARGET 65CF0D
DEFTARGET 65FFCA
DEFTARGET 6603BA
DEFTARGET 6A5437
DEFTARGET 69DF29
DEFTARGET 69DF16
DEFTARGET 4F5C93
DEFTARGET 4F5CB1
DEFTARGET 97D69E
DEFTARGET 5EE9AB
DEFTARGET 450177
DEFTARGET 450117
DEFTARGET 4BCE68
DEFTARGET 4C5CF7
DEFTARGET 4C5D2D
DEFTARGET 4B2528
DEFTARGET 4D0215
DEFTARGET 4D0265
DEFTARGET 4B6F8B
DEFTARGET 4E0A73

k_f0_5 DD 3F000000h                     ; 0.5f

.code

; ===============================================================================================================
; Core: C0 pre-render, halt, isTick, reset, C4 cache key
; ===============================================================================================================

; 0x6325CF 'call [eax+0x9C]' (clientUpdate). EAX = engine vtable, ECX = ESI = engine, BL = halt flag.
C0_STUB PROC
    push eax
    push ecx
    push ecx
    call OnPreRender                    ; cdecl(engine): X/Y toggle, renderId, mode controller, publish globals
    add esp, 4
    pop ecx
    pop eax
    call dword ptr [eax + 9Ch]          ; clientUpdate, exactly as the original call
    call OnPostRender                   ; cdecl(): leave the render, compute g_forceHalt
    ret                                 ; to 0x6325D4 (nop), then the HALT site
C0_STUB ENDP

; 0x6325D5 'test bl,bl / mov eax,[0xDE4388]' -> ZF for 'je 0x6325F8' (stock step) at 0x6325DC.
HALT_STUB PROC
    mov eax, ds:[0DE4388h]
    cmp byte ptr [g_forceHalt], 0
    jne forced
    test bl, bl                         ; stock decision
    ret
forced:
    RUNCNT IDX_HALT
    test esp, esp                       ; ZF=0: take the stepper's own halted branch 0x6325DE
    ret
HALT_STUB ENDP

; 0x63252F isTickFrame entry (thiscall ECX=engine, returns AL).
ISTICK_CAVE PROC
    cmp byte ptr [g_m60], 0
    je stock
    cmp byte ptr [g_inB], 0
    je stock
    cmp byte ptr [g_inClientUpdate], 0
    je stock
    IS_MAIN eax
    jne stock
    SKIPCNT IDX_ISTICK
    xor eax, eax                        ; B-render: never a tick frame
    ret
stock:
    mov eax, ds:[0D9F60Ch]              ; displaced
    jmp dword ptr [T_632534]
ISTICK_CAVE ENDP

; 0x44181A thunk 'jmp 0x635D11' (GameEngine::reset), ECX = engine.
RESET_CAVE PROC
    push ecx
    call OnEngineReset                  ; cdecl(): back to stock immediately
    pop ecx
    jmp dword ptr [T_635D11]
RESET_CAVE ENDP

; C4: GameClient::getFrame() as the drawable transform-cache key (0x6765D5 / 0x67173B / 0x671774).
C4_KEY_STUB PROC
    mov ecx, ds:[0DE4388h]
    mov eax, [ecx]
    call dword ptr [eax + 7Ch]          ; m_frame
    cmp byte ptr [g_m60], 0
    je done
    add eax, eax                        ; 60 mode: 0x80000000 | ((2*m_frame - window) & 0x7FFFFFFF)
    sub eax, [g_pwActive]
    and eax, 7FFFFFFFh
    or eax, 80000000h
done:
    ret
C4_KEY_STUB ENDP

; ===============================================================================================================
; A-only gates (60-mode B-render skips the call; 30 mode and A-renders run it unchanged)
; ===============================================================================================================

; Original: 'mov eax,[ecx] / call [eax+off]' with ECX already loaded.
VTGATE MACRO name, idx, off
name PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    je skip
run:
    RUNCNT idx
    mov eax, [ecx]
    jmp dword ptr [eax + off]
skip:
    SKIPCNT idx
    ret
name ENDP
ENDM

; Original: 'call target'.
CALLGATE MACRO name, idx, target
name PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    je skip
run:
    RUNCNT idx
    jmp dword ptr [T_&target]
skip:
    SKIPCNT idx
    ret
name ENDP
ENDM

; Original: 'mov ecx,[global] / mov eax,[ecx] / call [eax+off]'.
LOADVTGATE MACRO name, idx, global, off
name PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    je skip
run:
    RUNCNT idx
    mov ecx, ds:[global]
    mov eax, [ecx]
    jmp dword ptr [eax + off]
skip:
    SKIPCNT idx
    ret
name ENDP
ENDM

; Original: 'mov ecx,[global] / test ecx,ecx' followed by 'je skipBoth'. B-renders see ECX=0.
SELGATE MACRO name, idx, global
name PROC
    mov ecx, ds:[global]
    cmp byte ptr [g_m60], 0
    je t
    cmp byte ptr [g_uiTick], 0
    jne t
    SKIPCNT idx
    xor ecx, ecx
    test ecx, ecx
    ret
t:
    RUNCNT idx
    test ecx, ecx                       ; last flag writer, as the original
    ret
name ENDP
ENDM

; ---- clientUpdate 0x632409 ----
CALLGATE   Gate_62B385,        IDX_GATE_CU_PENDING_GAME, 62B385
VTGATE     GateVt28_APT,       IDX_GATE_CU_APT,          28h
VTGATE     GateVt28_Radar,     IDX_GATE_CU_RADAR,        28h
VTGATE     GateVt28_KbdDrain,  IDX_GATE_CU_KBD_DRAIN,    28h
VTGATE     GateVt28_Audio,     IDX_GATE_CU_AUDIO,        28h

; ---- GameClient::update 0x64849E ----
CALLGATE   Gate_GC_LOOKAT,     IDX_GATE_GC_LOOKAT,       83B471
CALLGATE   Gate_GC_LWXLAT,     IDX_GATE_GC_LWXLAT,       8392A7
CALLGATE   Gate_GC_GESTURE,    IDX_GATE_GC_GESTURE,      50EB3C
VTGATE     Gate_GC_LWVIEW,     IDX_GATE_GC_LWVIEW,       6Ch
CALLGATE   Gate_GC_LWUI,       IDX_GATE_GC_LWUI,         645750
VTGATE     Gate_GC_CLOUD,      IDX_GATE_GC_CLOUD,        28h
VTGATE     Gate_GC_FIRE,       IDX_GATE_GC_FIRE,         28h
VTGATE     Gate_GC_ANIM2D,     IDX_GATE_GC_ANIM2D,       28h
SELGATE    Sel_GC_KBD,         IDX_GATE_GC_KBDSEL,       0DE4334h
VTGATE     Gate_GC_SKEVA,      IDX_GATE_GC_SKEVA,        28h
VTGATE     Gate_GC_EVA,        IDX_GATE_GC_EVA,          28h
SELGATE    Sel_GC_MOUSE,       IDX_GATE_GC_MOUSESEL,     0DE36E0h
CALLGATE   Gate_GC_POPUPS,     IDX_GATE_GC_POPUPS,       782C56
LOADVTGATE Gate_GC_WM,         IDX_GATE_GC_WM,           0DE495Ch, 28h
CALLGATE   Gate_GC_PENDGAME2,  IDX_GATE_GC_PENDGAME2,    62B385
VTGATE     Gate_GC_TERRAIN,    IDX_GATE_GC_TERRAIN,      28h
VTGATE     Gate_GC_DISPUPD,    IDX_GATE_GC_DISPUPD,      28h
CALLGATE   Gate_GC_DISPUPD_LW, IDX_GATE_GC_DISPUPD_LW,   65C1EA
VTGATE     Gate_GC_DSM,        IDX_GATE_GC_DSM,          28h
VTGATE     Gate_GC_SHELL,      IDX_GATE_GC_SHELL,        28h
LOADVTGATE Gate_GC_IGUI,       IDX_GATE_GC_IGUI,         0DE4830h, 28h
VTGATE     Gate_GC_DISPUPD_INTRO, IDX_GATE_GC_DISPUPD_INTRO, 28h

; 0x648705 'mov eax,[0xD9F6F8]' before 'cmp eax,[ebx+0x10]': B-renders pretend the drawables were already updated
; for this m_frame, so the drawable update block stays A-only.
Cave_GC_DRAWBLK PROC
    mov eax, ds:[0D9F6F8h]
    cmp byte ptr [g_m60], 0
    je back
    cmp byte ptr [g_uiTick], 0
    jne back
    SKIPCNT IDX_GATE_GC_DRAWBLK
    mov eax, [ebx + 10h]
back:
    jmp dword ptr [T_64870A]
Cave_GC_DRAWBLK ENDP

; ---- callee-side gates (callers outside clientUpdate run stock) ----

; 0x5039C1 W3DAptPalantir vt+0x28 thunk 'jmp 0x6D7B33'.
PalGate PROC
    mov eax, [esp]
    cmp eax, 006A23E2h
    je k
    cmp eax, 00612272h
    je k
    mov [g_palBadRet], eax
    lock or dword ptr [g_anomaly], ANOM_PAL_CALLER
k:
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    jne run
    cmp byte ptr [g_inClientUpdate], 0
    je run
    IS_MAIN eax
    jne run
    SKIPCNT IDX_GATE_PALANTIR
    ret
run:
    lock inc dword ptr [g_siteRun + IDX_GATE_PALANTIR * 4]
    jmp dword ptr [T_6D7B33]
PalGate ENDP

; 0x49AAD4 'call 0x6C0E4D' (Living World manager update; consumes the logic RNG through the eye tower).
LwmGate PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    jne run
    cmp byte ptr [g_inClientUpdate], 0
    je run
    IS_MAIN eax
    jne run
    SKIPCNT IDX_GATE_LWM
    ret
run:
    lock inc dword ptr [g_siteRun + IDX_GATE_LWM * 4]
    jmp dword ptr [T_6C0E4D]
LwmGate ENDP

; 0x444CD4 W3DDisplay vt+0x188 entry 'mov ecx,[0xDE3C08]'.
Vt188Detour PROC
    lock inc dword ptr [g_vt188Calls]
    mov eax, [esp]
    mov [g_vt188LastRet], eax
    cmp byte ptr [g_m60], 0
    je run
    IS_MAIN eax
    jne run
    cmp byte ptr [g_inClientUpdate], 0
    jne incu
    lock or dword ptr [g_unknownPath], UP_VT188   ; unexpected caller in 60 mode: mode controller drops to 30
    jmp run
incu:
    cmp byte ptr [g_uiTick], 0
    jne run
    SKIPCNT IDX_GATE_DISPLAY_VT188
    xor eax, eax                        ; as the function's own early 'ret' at 0x444CFD
    ret
run:
    RUNCNT IDX_GATE_DISPLAY_VT188
    mov ecx, ds:[0DE3C08h]              ; displaced
    jmp dword ptr [T_444CDA]
Vt188Detour ENDP

; 0x48EA1F InGameUI::update thunk 'jmp 0x6A1F4D' (telemetry only).
IguiThunkTel PROC
    RUNCNT IDX_TEL_IGUI_THUNK
    mov eax, [esp]
    cmp eax, 006488B1h                  ; original return address
    je ok
    cmp eax, 006488ABh                  ; return address through Gate_GC_IGUI
    je ok
    mov [g_iguiBadRet], eax
    lock or dword ptr [g_anomaly], ANOM_IGUI_CALLER
ok:
    jmp dword ptr [T_6A1F4D]
IguiThunkTel ENDP

; 0x449D40 'mov ecx,[0xDE3744] / mov eax,[ecx] / call [eax+0x28]' (particle system manager update).
C7A_PartMgrGate PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    jne run
    IS_MAIN eax
    jne run
    SKIPCNT IDX_C7_PARTMGR_DRAW
    ret
run:
    RUNCNT IDX_C7_PARTMGR_DRAW
    mov ecx, ds:[0DE3744h]
    mov eax, [ecx]
    jmp dword ptr [eax + 28h]
C7A_PartMgrGate ENDP

; 0x444CF2 tail 'mov ecx,[0xDE3744] / mov eax,[ecx] / jmp [eax+0x28]'.
C7B_PartMgrTail PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    jne run
    IS_MAIN edx
    jne run
    SKIPCNT IDX_C7_PARTMGR_TAIL
    jmp dword ptr [T_444CFD]            ; the function's own 'ret' with EAX still 0
run:
    RUNCNT IDX_C7_PARTMGR_TAIL
    mov ecx, ds:[0DE3744h]
    mov eax, [ecx]
    jmp dword ptr [eax + 28h]
C7B_PartMgrTail ENDP

; Snow manager: weather transition 0x49405A and lightning 0x492C35 counters (A-only).
SNOWGATE MACRO name, idx, target
name PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    jne run
    IS_MAIN eax
    jne run
    SKIPCNT idx
    ret
run:
    RUNCNT idx
    jmp dword ptr [T_&target]
name ENDP
ENDM

SNOWGATE SNOW_Gate_49405A, IDX_SNOW_WEATHER,   49405A
SNOWGATE SNOW_Gate_492C35, IDX_SNOW_LIGHTNING, 492C35

; 0x44B7B3 'mov ecx,edi / call 0x4430BB / mov ecx,edi / call 0x4438DA' (FPS average + dynamic LOD): A-only.
LOD_STUB PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_uiTick], 0
    jne run
    IS_MAIN eax
    jne run
    SKIPCNT IDX_LOD_GATE
    ret
run:
    RUNCNT IDX_LOD_GATE
    mov ecx, edi
    call dword ptr [T_4430BB]
    mov ecx, edi
    call dword ptr [T_4438DA]
    ret
LOD_STUB ENDP

; ===============================================================================================================
; Render-internal integrators (g_skipB = 60-mode B-render inside clientUpdate; main thread only)
; ===============================================================================================================

; 'inc dword ptr [addr]' (view-filter fade counters). The game's INC stays the last flag writer.
INCSTUB MACRO name, idx, addr
name PROC
    cmp byte ptr [g_skipB], 0
    je doinc
    IS_MAIN_KEEP_EAX
    je skip
doinc:
    RUNCNT idx
    inc dword ptr ds:[addr]
    ret
skip:
    SKIPCNT idx
    ret
name ENDP
ENDM

INCSTUB STUB_INC_DD1BEC, IDX_INT_VF_BW_S1,   0DD1BECh
INCSTUB STUB_INC_DD1A40, IDX_INT_VF_MASK_IN, 0DD1A40h
INCSTUB STUB_INC_DD1BFC, IDX_INT_VF_MONO_1,  0DD1BFCh

; 0x4F5C7D mask view-filter fade-out: 'cmp [0xDD1A40],eax / jge 0x4F5CB1 / cvtsi2ss xmm0,[0xDD1A40] /
; inc [0xDD1A40]'. The compare and conversion precede the increment here, so a B-render converts the counter value
; this pair's A-render showed (counter-1) and never takes the end-of-fade path. EAX (fade length) is live.
CAVE_VF_MASK_OUT PROC
    cmp byte ptr [g_skipB], 0
    je stock
    IS_MAIN_KEEP_EAX
    jne stock
    SKIPCNT IDX_INT_VF_MASK_OUT
    push ecx
    mov ecx, ds:[0DD1A40h]
    test ecx, ecx
    jle bstock                          ; A did not advance the fade
    dec ecx                             ; the value A showed
    cmp ecx, eax
    jge bstock
    cvtsi2ss xmm0, ecx
    pop ecx
    jmp dword ptr [T_4F5C93]
bstock:
    pop ecx
    cmp dword ptr ds:[0DD1A40h], eax
    jge fadeEnd
    cvtsi2ss xmm0, dword ptr ds:[0DD1A40h]
    jmp dword ptr [T_4F5C93]
stock:
    RUNCNT IDX_INT_VF_MASK_OUT
    cmp dword ptr ds:[0DD1A40h], eax
    jge fadeEnd
    cvtsi2ss xmm0, dword ptr ds:[0DD1A40h]
    inc dword ptr ds:[0DD1A40h]
    jmp dword ptr [T_4F5C93]
fadeEnd:
    jmp dword ptr [T_4F5CB1]
CAVE_VF_MASK_OUT ENDP

; 0x4FDB4A 'add dword ptr [esi+0x74],0x21 / jmp 0x4FDB58' (shore wave timer).
CAVE_SHORE PROC
    cmp byte ptr [g_skipB], 0
    je a
    IS_MAIN_KEEP_EAX
    jne a
    SKIPCNT IDX_INT_SHORE
    push eax
    mov eax, [g_shoreStepB]
    add dword ptr [esi + 74h], eax
    pop eax
    jmp dword ptr [T_4FDB58]
a:
    RUNCNT IDX_INT_SHORE
    cmp byte ptr [g_m60], 0
    jne a60
    add dword ptr [esi + 74h], 21h
    jmp dword ptr [T_4FDB58]
a60:
    push eax
    mov eax, [g_shoreStepA]
    add dword ptr [esi + 74h], eax
    pop eax
    jmp dword ptr [T_4FDB58]
CAVE_SHORE ENDP

; Call gates skipped on B-renders: trees 0x449D55 -> 0x4684FB, recoil 0x4C78CE -> 0x4B4699.
SKIPBCALL MACRO name, idx, target
name PROC
    cmp byte ptr [g_skipB], 0
    je run
    IS_MAIN_KEEP_EAX
    jne run
    SKIPCNT idx
    ret
run:
    RUNCNT idx
    jmp dword ptr [T_&target]
name ENDP
ENDM

SKIPBCALL STUB_TREES,  IDX_INT_TREES,  4684FB
SKIPBCALL STUB_RECOIL, IDX_INT_RECOIL, 4B4699

; 0x4E83F0 'cmp byte ptr [ebp-1],0 / jne 0x4E83FE' (shrub sway + push-aside run unless paused).
CAVE_SHRUB PROC
    cmp byte ptr [g_skipB], 0
    je stock
    IS_MAIN_KEEP_EAX
    jne stock
    SKIPCNT IDX_INT_SHRUBS
    mov byte ptr [ebp - 1], 1           ; B-render behaves as stock does while paused
    jmp dword ptr [T_4E83FE]
stock:
    cmp byte ptr [ebp - 1], 0
    jne taken
    jmp dword ptr [T_4E83F6]
taken:
    jmp dword ptr [T_4E83FE]
CAVE_SHRUB ENDP

; 0x732AA9 'mov eax,[esi+4] / movss xmm0,[eax+0x5C]' (decal spiral velocity/angle integrators).
CAVE_DECAL_SPIRAL PROC
    cmp byte ptr [g_skipB], 0
    je stock
    IS_MAIN_KEEP_EAX
    jne stock
    xorps xmm3, xmm3                    ; no acceleration step
    xorps xmm4, xmm4                    ; no angle step
stock:
    mov eax, [esi + 4]
    movss xmm0, dword ptr [eax + 5Ch]
    jmp dword ptr [T_732AB1]
CAVE_DECAL_SPIRAL ENDP

; 0x4FD1B4 'dec dword ptr [ebx+4] / cmp dword ptr [ebx+4],2' (motion-blur end-pan counter). Flags stay live.
STUB_MB_ENDPAN PROC
    cmp byte ptr [g_skipB], 0
    je dodec
    IS_MAIN_KEEP_EAX
    je docmp
dodec:
    dec dword ptr [ebx + 4]
docmp:
    cmp dword ptr [ebx + 4], 2
    ret
STUB_MB_ENDPAN ENDP

; ===============================================================================================================
; Clock: C3 W3D sync lag, S0 fast-forward guard, C5 trampoline
; ===============================================================================================================

; 0x44B911: 'push esi / call C3_SyncDelta / add [0xDC7580],eax'. stdcall(d), returns the sync delta.
C3_SyncDelta PROC
    mov eax, ds:[0DC7A8Ch]              ; v = ms per client frame
    imul eax, dword ptr [esp + 4]       ; P = v*d (identical to the original 'imul eax,esi')
    cmp byte ptr [g_m60], 0
    je ret4
    IS_MAIN edx
    jne ret4
    cmp byte ptr [g_inB], 0
    jne flush
    cmp dword ptr [esp + 4], 0
    jle flush
    ; advancing A-render: add half now, owe the rest to the B-render
    mov ecx, [g_syncHalfPar]
    xor dword ptr [g_syncHalfPar], 1
    add ecx, eax
    sar ecx, 1                          ; h = (P + parity) >> 1
    sub eax, ecx                        ; P - h
    mov edx, [g_syncOwed]
    mov [g_syncOwed], eax
    lea eax, [edx + ecx]                ; previous owed (normally 0) + h
    ret 4
flush:
    add eax, [g_syncOwed]
    mov dword ptr [g_syncOwed], 0
ret4:
    ret 4
C3_SyncDelta ENDP

; 0x44B8D0 'mov ecx,[ebp-0x1C] / call 0x48A953' on the fast-forward path of W3DDisplay::draw.
S0_FFGuard PROC
    cmp byte ptr [g_m60], 0
    je stock
    IS_MAIN ecx
    jne stock
    inc dword ptr [g_tmFFin60]          ; expected to stay 0
    cmp byte ptr [g_inB], 0
    je stock
    SKIPCNT IDX_S0_FFGUARD
    jmp dword ptr [T_44BC5F]            ; B-render: nothing (stock render k took this branch already)
stock:
    mov ecx, [ebp - 1Ch]
    call dword ptr [T_48A953]
    jmp dword ptr [T_44B8D8]
S0_FFGuard ENDP

; Trampoline for the C5 detour at 0x67BDC0 (calcPhysicsXform): displaced prologue, then the original body.
C5_Tramp PROC
    push ebp
    mov ebp, esp
    push ecx
    push edi
    jmp dword ptr [T_67BDC5]
C5_Tramp ENDP

; ===============================================================================================================
; Pacing, Present, LOD, camera-multiplier draw skipping
; ===============================================================================================================

; 0x63A196 'call [timeGetTime]' at the start of the stock frame limiter. ESI = engine, EBX = 0.
PACER_CAVE PROC
    cmp byte ptr [g_m60], 0
    jne p60
    call dword ptr ds:[0BD0920h]        ; stock limiter, unchanged
    jmp dword ptr [T_63A19C]
p60:
    push esi
    call AotR60_Pacer                   ; cdecl(engine) -> timeGetTime() after the wait
    add esp, 4
    mov edi, eax
    jmp dword ptr [T_63A1F5]            ; stock 'waited' exit: [0xDE4318] = EDI
PACER_CAVE ENDP

; 0x522644: 'mov eax,[0xDD3474] / mov edx,[eax] / push ebx x4 / push eax / call [edx+0x44]' (Present).
PRESENT_STUB PROC
    cmp byte ptr [g_m60], 0
    je present
    call AotR60_PresentSkip             ; cdecl() -> nonzero to skip this B-render's Present
    test eax, eax
    jz present
    SKIPCNT IDX_PRESENT
    xor eax, eax                        ; S_OK
    ret
present:
    RUNCNT IDX_PRESENT
    mov eax, ds:[0DD3474h]
    mov edx, [eax]
    push ebx
    push ebx
    push ebx
    push ebx
    push eax
    call dword ptr [edx + 44h]
    cmp eax, 88760868h                  ; D3DERR_DEVICELOST
    jne done
    mov byte ptr [g_gapDevLost], 1
done:
    ret
PRESENT_STUB ENDP

; 0x522000 DX8Wrapper::Reset_Device entry 'mov eax,fs:[0]': pacing gap marker.
GAP_RESET_CAVE PROC
    mov byte ptr [g_gapReset], 1
    mov eax, fs:[0]
    jmp dword ptr [T_522006]
GAP_RESET_CAVE ENDP

; 0x44B95F camera time multiplier draw skipping. EAX = M, ESI = now, EDI = W3DDisplay.
MDRAW_CAVE PROC
    cmp byte ptr [g_m60], 0
    je stock
    IS_MAIN ecx
    jne stock
    cmp byte ptr [g_inB], 0
    jne bpath
    ; 60-mode A-render: the stock decision, recording when A will draw
    cmp eax, 1
    jle a_le1
    dec dword ptr ds:[0D98CB4h]
    cmp dword ptr ds:[0D98CB4h], 1
    jle a_drawm
    jmp dword ptr [T_44BC5F]
a_drawm:
    cmp byte ptr [edi + 115h], 0
    jne a_nodrawm
    mov ecx, [g_renderId]
    mov [g_mDrawRid], ecx
    mov [g_mDrawAM], eax
a_nodrawm:
    jmp dword ptr [T_44B977]
a_le1:
    cmp byte ptr [edi + 115h], 0
    jne a_nodraw1
    mov ecx, [g_renderId]
    mov [g_mDrawRid], ecx
    mov [g_mDrawAM], eax
a_nodraw1:
    jmp dword ptr [T_44B98C]
bpath:
    ; 60-mode B-render: draw iff this pair's A-render drew; no counter, no wait
    mov ecx, [g_renderId]
    dec ecx
    cmp ecx, [g_mDrawRid]
    je b_draw
    SKIPCNT IDX_MDRAW
    jmp dword ptr [T_44BC5F]
b_draw:
    mov esi, ds:[0BD089Ch]              ; == 0x44B9A2 (USER32!ClientToScreen)
    mov eax, ds:[0DE4364h]              ; == 0x44B9A8 (GlobalData)
    jmp dword ptr [T_44B9E3]
stock:
    cmp eax, 1
    jle s_le1
    dec dword ptr ds:[0D98CB4h]
    cmp dword ptr ds:[0D98CB4h], 1
    jle s_draw
    jmp dword ptr [T_44BC5F]
s_draw:
    jmp dword ptr [T_44B977]
s_le1:
    jmp dword ptr [T_44B98C]
MDRAW_CAVE ENDP

; ===============================================================================================================
; Camera (stepped only on A-renders) and presentation windows
; ===============================================================================================================

; 0x48BD1B 'mov byte ptr [ebp-0xE],0 / je 0x48BE02' in W3DView::update. B-renders skip every camera mutation.
CAVE_S1 PROC
    mov byte ptr [ebp - 0Eh], 0
    cmp byte ptr [g_m60], 0
    je stock
    cmp byte ptr [g_inB], 0
    je stock
    IS_MAIN_KEEP_EAX
    jne stock
    SKIPCNT IDX_S1_VIEWUPDATE_SKIP
    SAVE_ALL
    push ebx
    call CamSwapToMk_B                  ; cdecl(view+0xB4): present stock render k's camera M_k
    add esp, 4
    RESTORE_ALL
    lea edi, [ebx - 0B4h]               ; EDI = view (callback context of the drawable pass)
    jmp dword ptr [T_48C701]
stock:
    cmp eax, esi                        ; original ZF
    jne notz
    jmp dword ptr [T_48BE02]
notz:
    jmp dword ptr [T_48BD25]
CAVE_S1 ENDP

; 0x48C701 'mov eax,[0xDD1E0C]' at the start of the drawable pass. A-renders record M_k and open window 1.
CAVE_S2 PROC
    cmp byte ptr [g_m60], 0
    je stock
    cmp byte ptr [g_inB], 0
    jne stock
    IS_MAIN_KEEP_EAX
    jne stock
    SAVE_ALL
    push ebx
    call S2_RecordMkAndOpen             ; cdecl(view+0xB4)
    add esp, 4
    RESTORE_ALL
stock:
    mov eax, ds:[0DD1E0Ch]
    jmp dword ptr [T_48C706]
CAVE_S2 ENDP

; 0x48C765 'mov ecx,[ebp-0xC] / pop edi / pop esi / pop ebx' at the end of the drawable pass: close window 1.
CAVE_S2E PROC
    cmp byte ptr [g_pw1Open], 0
    je stock
    push eax
    push edx
    mov eax, ds:[0DE4324h]
    mov edx, [g_fracSaved]
    mov [eax + 3Ch], edx                ; stock fraction back
    mov dword ptr [g_pwActive], 0
    mov byte ptr [g_pw1Open], 0
    pop edx
    pop eax
stock:
    mov ecx, [ebp - 0Ch]
    pop edi
    pop esi
    pop ebx
    jmp dword ptr [T_48C76B]
CAVE_S2E ENDP

; 0x449DAB 'mov eax,[0xDE412C]' in drawFrame before the scene render: A-renders open window 2 (fraction, key,
; interpolated camera picture).
CAVE_SCENE_OPEN PROC
    cmp byte ptr [g_m60], 0
    je stock
    cmp byte ptr [g_inB], 0
    jne stock
    IS_MAIN_KEEP_EAX
    jne stock
    SAVE_ALL
    call SceneOpen_A                    ; cdecl()
    RESTORE_ALL
stock:
    mov eax, ds:[0DE412Ch]
    jmp dword ptr [T_449DB0]
CAVE_SCENE_OPEN ENDP

; 0x44A271 'call 0x5208D0' on every drawFrame exit: restore the real camera and the stock fraction/key.
STUB_SCENE_RESTORE PROC
    cmp byte ptr [g_swapActive], 0
    jne work
    cmp byte ptr [g_pw2Open], 0
    jne work
    jmp dword ptr [T_5208D0]
work:
    SAVE_ALL
    call SceneRestore                   ; cdecl()
    RESTORE_ALL
    jmp dword ptr [T_5208D0]
STUB_SCENE_RESTORE ENDP

; 0x48B7B1 W3DView::setCameraTransform entry 'mov eax,0xB73F25': end any camera swap first.
CAVE_GUARD_SCT PROC
    cmp byte ptr [g_swapActive], 0
    je stock
    SAVE_ALL
    call CamSwapEndGuard                ; cdecl()
    RESTORE_ALL
stock:
    mov eax, 00B73F25h
    jmp dword ptr [T_48B7B6]
CAVE_GUARD_SCT ENDP

; 0x4FD254 'mov ecx,[0xDE447C]' before the motion-blur filter's lookAt: end any camera swap first.
CAVE_GUARD_MB PROC
    cmp byte ptr [g_swapActive], 0
    je stock
    SAVE_ALL
    call CamSwapEndGuard
    RESTORE_ALL
stock:
    mov ecx, ds:[0DE447Ch]
    jmp dword ptr [T_4FD25A]
CAVE_GUARD_MB ENDP

; ===============================================================================================================
; Render-internal integrators, part 2 (light, rope, treads, truck, wake, debris, subobject fade, outline,
; LightPulse, instance fade, overlay fade, subtitles, UI particles, world-anim rise). Same B predicate as part 1.
; ===============================================================================================================

; Jumps to `label` when this is a 60-mode B-render's clientUpdate on the main thread. Keeps EAX; writes flags.
IF_B_GOTO MACRO label
    LOCAL notB
    cmp byte ptr [g_skipB], 0
    je notB
    IS_MAIN_KEEP_EAX
    je label
notB:
ENDM

; 0x4CF3C9 'comiss xmm1,[edi+0x14] / jb 0x4CF40E' (W3DLightDraw pulse countdown, reload and ramp).
CAVE_LIGHT_PULSE PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_LIGHT_PULSE
    comiss xmm1, dword ptr [edi + 14h]
    jb taken
    jmp dword ptr [T_4CF3CF]
taken:
    jmp dword ptr [T_4CF40E]
bpath:
    SKIPCNT IDX_INT_LIGHT_PULSE
    movss xmm0, dword ptr [edi + 1Ch]   ; the intensity A computed
    movss dword ptr [ebp - 4], xmm0
    jmp dword ptr [T_4CF434]
CAVE_LIGHT_PULSE ENDP

; 0x4CA599 'movss xmm0,[esi+0x48]' (W3DRopeDraw wobble/drop/speed integrators after the draw).
CAVE_ROPE PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_ROPE
    movss xmm0, dword ptr [esi + 48h]
    jmp dword ptr [T_4CA59E]
bpath:
    SKIPCNT IDX_INT_ROPE
    jmp dword ptr [T_4CA5FC]            ; epilogue
CAVE_ROPE ENDP

; 0x4CDE8B 'xor edi,edi / cmp [ebx+0x354],edi' (W3DTankDraw tread scroll loop).
CAVE_TREAD PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_TREAD
    xor edi, edi
    cmp dword ptr [ebx + 354h], edi
    jmp dword ptr [T_4CDE93]
bpath:
    SKIPCNT IDX_INT_TREAD
    jmp dword ptr [T_4CDEC9]            ; base doDrawModule call
CAVE_TREAD ENDP

; 0x4CC1F5 'mulss xmm0,[ecx+0x1E4] / addss xmm0,[eax]' (W3DTruckDraw cab damping). EAX is live.
CAVE_TRUCK_CAB PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_TRUCK_CAB
    mulss xmm0, dword ptr [ecx + 1E4h]
    addss xmm0, dword ptr [eax]
    jmp dword ptr [T_4CC201]
bpath:
    SKIPCNT IDX_INT_TRUCK_CAB
    movss xmm0, dword ptr [eax]         ; current cab angle
    jmp dword ptr [T_4CC201]
CAVE_TRUCK_CAB ENDP

; 0x4CC313 'mulss xmm1,[eax+0x1E4] / addss xmm1,[esi+0x36C]' (W3DTruckDraw trailer damping).
CAVE_TRUCK_TRAILER PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_TRUCK_TRAILER
    mulss xmm1, dword ptr [eax + 1E4h]
    addss xmm1, dword ptr [esi + 36Ch]
    jmp dword ptr [T_4CC323]
bpath:
    SKIPCNT IDX_INT_TRUCK_TRAILER
    movss xmm1, dword ptr [esi + 36Ch]  ; current trailer angle
    jmp dword ptr [T_4CC323]
CAVE_TRUCK_TRAILER ENDP

; 0x4CC4B3 'cmp byte ptr [esi+0x2EA],bl' (W3DTruckDraw tire rotation block).
CAVE_TRUCK_WHEEL PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_TRUCK_WHEEL
    cmp byte ptr [esi + 2EAh], bl       ; ZF live until 0x4CC4EB
    jmp dword ptr [T_4CC4B9]
bpath:
    SKIPCNT IDX_INT_TRUCK_WHEEL
    movss xmm1, dword ptr [esi + 310h]  ; rear tire angle as A left it
    jmp dword ptr [T_4CC518]
CAVE_TRUCK_WHEEL ENDP

; 0x4B155C 'inc dword ptr [esi+0x3C] / pop edi / pop ebx' (W3DDebrisDraw per-draw counter).
CAVE_DEBRIS PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_DEBRIS
    inc dword ptr [esi + 3Ch]
    pop edi
    pop ebx
    jmp dword ptr [T_4B1561]
bpath:
    SKIPCNT IDX_INT_DEBRIS
    pop edi
    pop ebx
    jmp dword ptr [T_4B1561]
CAVE_DEBRIS ENDP

; 0x4B3244 'subss xmm1,[edi+0x10]' (subobject fade delay).
CAVE_SUBOBJ_DELAY PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_SUBOBJ_DELAY
    subss xmm1, dword ptr [edi + 10h]
bpath:
    jmp dword ptr [T_4B3249]
CAVE_SUBOBJ_DELAY ENDP

; 0x4B3266 'addss xmm1,[edi+8]' (subobject fade alpha).
CAVE_SUBOBJ_ALPHA PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_SUBOBJ_ALPHA
    addss xmm1, dword ptr [edi + 8]
bpath:
    jmp dword ptr [T_4B326B]
CAVE_SUBOBJ_ALPHA ENDP

; W3DDynamicLight vtable slot 0xBDC00C (On_Frame_Update 0x46D665): LightPulse counters step on A-renders only.
STUB_LIGHTPULSE PROC
    cmp byte ptr [g_skipB], 0
    je run
    cmp byte ptr [g_lp4b], 0
    jne run
    IS_MAIN_KEEP_EAX
    jne run
    SKIPCNT IDX_INT_LIGHTPULSE
    ret
run:
    RUNCNT IDX_INT_LIGHTPULSE
    jmp dword ptr [T_46D665]
STUB_LIGHTPULSE ENDP

; Phase 4b only: 0x46D7A9 W3DDynamicLight pulse setup, counts doubled in 60 mode.
CAVE_LP_DBL PROC
    mov eax, [esp + 8]
    cmp byte ptr [g_lp4b], 0
    je d
    cmp byte ptr [g_m60], 0
    je d
    test eax, eax
    js d
    add eax, eax
d:
    mov [ecx + 150h], eax
    mov [ecx + 148h], eax
    mov eax, [esp + 4]
    cmp byte ptr [g_lp4b], 0
    je i
    cmp byte ptr [g_m60], 0
    je i
    test eax, eax
    js i
    add eax, eax
i:
    mov [ecx + 14Ch], eax
    mov [ecx + 154h], eax
    jmp dword ptr [T_46D7C9]
CAVE_LP_DBL ENDP

; 0x65CF05 overlay fade entry 'movss xmm1,[ecx+0x120]': B-renders draw the overlay as A left it, without stepping.
CAVE_OVL_FADE PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_OVL_FADE
    movss xmm1, dword ptr [ecx + 120h]
    jmp dword ptr [T_65CF0D]
bpath:
    SKIPCNT IDX_INT_OVL_FADE
    movss xmm1, dword ptr [ecx + 120h]
    xorps xmm0, xmm0
    comiss xmm1, xmm0
    jbe nodraw
    jmp dword ptr [T_65CFAB]            ; stock draw tail
nodraw:
    ret
CAVE_OVL_FADE ENDP

; 0x66045E 'call 0x65FFCA' (subtitle box state machine): B-renders report A's visibility without stepping.
STUB_SUBT_STATE PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_SUBT_STATE
    jmp dword ptr [T_65FFCA]
bpath:
    SKIPCNT IDX_INT_SUBT_STATE
    mov eax, [ecx + 14h]
    test eax, eax
    je vis
    cmp eax, 2
    je vis
    cmp eax, 3
    je vis
    xor eax, eax
    ret
vis:
    mov eax, 1
    ret
STUB_SUBT_STATE ENDP

; 0x660469 'call 0x6603BA' (subtitle line scroller).
STUB_SUBT_SCROLL PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_SUBT_SCROLL
    jmp dword ptr [T_6603BA]
bpath:
    SKIPCNT IDX_INT_SUBT_SCROLL
    ret
STUB_SUBT_SCROLL ENDP

; 0x6A53FB UI particle integration block (60 bytes). Re-creates the flags of 'test byte [esi+0x10],1' at the end.
CAVE_UIPART PROC
    IF_B_GOTO bpath
    cmp byte ptr [g_uiPart4b], 0
    je stock
    cmp byte ptr [g_m60], 0
    je stock
    ; 4b A: p += 0.5*v
    movss xmm0, dword ptr [esi + 14h]
    mulss xmm0, dword ptr [k_f0_5]
    addss xmm0, dword ptr [esi + 4]
    movss dword ptr [esi + 4], xmm0
    movss xmm0, dword ptr [esi + 18h]
    mulss xmm0, dword ptr [k_f0_5]
    addss xmm0, dword ptr [esi + 8]
    movss dword ptr [esi + 8], xmm0
    jmp done
bpath:
    SKIPCNT IDX_INT_UIPART
    cmp byte ptr [g_uiPart4b], 0
    je done
    ; 4b B: p += 0.5*v, v *= d
    movss xmm0, dword ptr [esi + 14h]
    mulss xmm0, dword ptr [k_f0_5]
    addss xmm0, dword ptr [esi + 4]
    movss dword ptr [esi + 4], xmm0
    movss xmm0, dword ptr [esi + 14h]
    mulss xmm0, dword ptr [esi + 1Ch]
    movss dword ptr [esi + 14h], xmm0
    movss xmm0, dword ptr [esi + 18h]
    mulss xmm0, dword ptr [k_f0_5]
    addss xmm0, dword ptr [esi + 8]
    movss dword ptr [esi + 8], xmm0
    movss xmm0, dword ptr [esi + 1Ch]
    mulss xmm0, dword ptr [esi + 18h]
    movss dword ptr [esi + 18h], xmm0
    jmp done
stock:
    RUNCNT IDX_INT_UIPART
    movss xmm0, dword ptr [esi + 4]
    addss xmm0, dword ptr [esi + 14h]
    movss dword ptr [esi + 4], xmm0
    movss xmm0, dword ptr [esi + 14h]
    mulss xmm0, dword ptr [esi + 1Ch]
    movss dword ptr [esi + 14h], xmm0
    movss xmm0, dword ptr [esi + 8]
    addss xmm0, dword ptr [esi + 18h]
    movss dword ptr [esi + 8], xmm0
    movss xmm0, dword ptr [esi + 1Ch]
    mulss xmm0, dword ptr [esi + 18h]
    movss dword ptr [esi + 18h], xmm0
done:
    test byte ptr [esi + 10h], 1
    jmp dword ptr [T_6A5437]
CAVE_UIPART ENDP

; 0x69DF0E 'cvtsi2ss xmm0,[0xD9F608]' (world-anim rise z += rise/LTR).
CAVE_WANIM_RISE PROC
    IF_B_GOTO bpath
    cmp byte ptr [g_wanim4b], 0
    je stock
    cmp byte ptr [g_m60], 0
    je stock
    jmp half
bpath:
    cmp byte ptr [g_wanim4b], 0
    jne half
    SKIPCNT IDX_INT_WANIM_RISE
    jmp dword ptr [T_69DF29]            ; no rise step; projection and draw run
half:
    cvtsi2ss xmm0, dword ptr ds:[0D9F608h]
    addss xmm0, xmm0
    jmp dword ptr [T_69DF16]
stock:
    RUNCNT IDX_INT_WANIM_RISE
    cvtsi2ss xmm0, dword ptr ds:[0D9F608h]
    jmp dword ptr [T_69DF16]
CAVE_WANIM_RISE ENDP

; ===============================================================================================================
; Effects sweep 4a (after the first 60 FPS play test): UI clock, tooltip, radar, animation FX events, attached
; models, rider/sail slews, laser texture cells, turret history (2a), floor fade, terrain tile rebuild budget.
; ===============================================================================================================

; UI_PBCLOCK_* 'mov byte [esi],0 / call winSetUserData(esi)' after a push button drew its progress clock.
; The clock request is armed by ControlBar::update (A-only, after the draw). A B-render keeps it, so both renders
; of a pair show the clock and the next A-render consumes it exactly like stock render k+1.
PBCLOCK_STUB MACRO name, idx
name PROC
    cmp byte ptr [g_skipB], 0
    je clear
    IS_MAIN_KEEP_EAX
    je keep
clear:
    RUNCNT idx
    mov byte ptr [esi], 0
    jmp dword ptr [T_97D69E]            ; winSetUserData: ret 4 pops the pushed ESI, returns to site+5
keep:
    SKIPCNT idx
    jmp dword ptr [T_97D69E]
name ENDP
ENDM

PBCLOCK_STUB STUB_PB_CLOCK_KEEP_B,        IDX_UI_PBCLOCK_IMAGE
PBCLOCK_STUB STUB_PB_CLOCK_KEEP_B_PLAIN,  IDX_UI_PBCLOCK_PLAIN
PBCLOCK_STUB STUB_PB_CLOCK_KEEP_B_RADIAL, IDX_UI_PBCLOCK_RADIAL

; UI_TOOLTIP_LINGER 0x5EE9A3 'lea eax,[esi+0x1304] / dec dword [eax]' (tooltip hide grace, per draw).
CAVE_TOOLTIP_LINGER PROC
    lea eax, [esi + 1304h]
    IF_B_GOTO bpath
    RUNCNT IDX_UI_TOOLTIP_LINGER
    dec dword ptr [eax]
    jmp dword ptr [T_5EE9AB]
bpath:
    SKIPCNT IDX_UI_TOOLTIP_LINGER
    jmp dword ptr [T_5EE9AB]
CAVE_TOOLTIP_LINGER ENDP

; UI_RADAR_REFRESH_B 0x450111 'mov ecx,[0xDE4388]' before the radar overlay's m_frame%6 refresh test: a B-render
; skips the test when the A-render already ran it for the same m_frame (identical output; performance only).
CAVE_RADAR_REFRESH PROC
    mov ecx, ds:[0DE4388h]
    push eax
    mov eax, [ecx + 10h]                ; m_frame
    cmp byte ptr [g_skipB], 0
    je recordA
    push edx
    mov edx, fs:[24h]
    cmp edx, [g_mainTid]
    pop edx
    jne stock
    cmp eax, [g_radarAFrame]
    jne stock
    pop eax
    SKIPCNT IDX_UI_RADAR_REFRESH_B
    jmp dword ptr [T_450177]
recordA:
    mov [g_radarAFrame], eax
stock:
    pop eax
    jmp dword ptr [T_450117]
CAVE_RADAR_REFRESH ENDP

; INT_FXEV_GATE 0x4C7819 'call 0x4BCE68' (animation frame events). In 60 mode the events are evaluated once per
; pair, on the B-render, which sees stock render k's (prev,cur] frame window (INT_FXEV_PREV). A-renders skip.
STUB_FXEV_GATE PROC
    cmp byte ptr [g_m60], 0
    je run
    cmp byte ptr [g_inB], 0
    jne run
    cmp byte ptr [g_inClientUpdate], 0
    je run
    IS_MAIN_KEEP_EAX
    jne run
    SKIPCNT IDX_INT_FXEV_GATE
    ret
run:
    RUNCNT IDX_INT_FXEV_GATE
    jmp dword ptr [T_4BCE68]
STUB_FXEV_GATE ENDP

; INT_FXEV_PREV 0x4BF74A 'mov [esi+4],eax / mov eax,[esi+0xC]' (animation channel prev = cur before the advance).
; B keeps prev at the frame before the A-render's advance.
STUB_FXEV_PREV PROC
    IF_B_GOTO bpath
    mov [esi + 4], eax
bpath:
    mov eax, [esi + 0Ch]
    ret
STUB_FXEV_PREV ENDP

; INT_ATTMDL_CHK 0x4C5CF1 'cmp dword [esi+0x1C],0 / jg 0x4C5D2D' (attached model expiry): B never removes.
CAVE_ATTMDL_CHK PROC
    IF_B_GOTO alive
    cmp dword ptr [esi + 1Ch], 0
    jg alive
    jmp dword ptr [T_4C5CF7]
alive:
    jmp dword ptr [T_4C5D2D]
CAVE_ATTMDL_CHK ENDP

; INT_ATTMDL_DEC 0x4C5F5B 'dec dword [esi+0x1C] / add esi,0x20' (attached model lifetime per draw): A-only.
STUB_ATTMDL_DEC PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_ATTMDL_DEC
    dec dword ptr [esi + 1Ch]
    add esi, 20h
    ret
bpath:
    SKIPCNT IDX_INT_ATTMDL_DEC
    add esi, 20h
    ret
STUB_ATTMDL_DEC ENDP

; INT_RIDER_SLEW 0x4B2520 entry of the cdecl heading slew (0.1 rad per call): B returns without stepping.
CAVE_RIDER_SLEW PROC
    IF_B_GOTO bpath
    mov eax, [esp + 4]
    movss xmm0, dword ptr [eax]
    jmp dword ptr [T_4B2528]
bpath:
    ret
CAVE_RIDER_SLEW ENDP

; INT_LASER_TEXCELL 0x4C90DD 'inc dword [esi+0x28] / mov ecx,[esi+0x18]' (laser texture cell per draw): A-only.
STUB_LASER_TEXCELL PROC
    IF_B_GOTO bpath
    inc dword ptr [esi + 28h]
bpath:
    mov ecx, [esi + 18h]
    ret
STUB_LASER_TEXCELL ENDP

; INT_SAIL_SLEW 0x4D020C 'cmp byte [edi+0x2EC],0 / je 0x4D0265' (sail swing per draw): B redraws A's angle.
CAVE_SAIL_SLEW PROC
    cmp byte ptr [edi + 2ECh], 0
    je first
    IF_B_GOTO bpath
    jmp dword ptr [T_4D0215]
bpath:
    movss xmm0, dword ptr [edi + 2E8h]
    movss dword ptr [ebp + 8], xmm0
first:
    jmp dword ptr [T_4D0265]
CAVE_SAIL_SLEW ENDP

; INT_TURRET_2A 0x4B6F81 'mov eax,[0xDE4324] / movss xmm1,[eax+0x3C]' (turret history shifts at fraction 1.0).
; Only without unit interpolation: a B-render whose fraction is 1.0 sees 0.0, so the history shifts once per tick.
CAVE_TURRET_FRAC PROC
    mov eax, ds:[0DE4324h]
    movss xmm1, dword ptr [eax + 3Ch]
    cmp byte ptr [g_skipB], 0
    je done
    cmp byte ptr [g_featPresent], 0
    jne done
    IS_MAIN_KEEP_EAX
    jne done
    ucomiss xmm1, dword ptr ds:[0BD1908h]
    jp done
    jne done
    xorps xmm1, xmm1
done:
    jmp dword ptr [T_4B6F8B]
CAVE_TURRET_FRAC ENDP

; INT_FLOOR_FADE 0x4E3F08 'addss xmm0,[edx] / comiss xmm1,xmm0' (floor death fade per scene render): A-only.
; The final comiss sets the flags the following jbe reads; ret keeps them.
STUB_FLOOR_FADE PROC
    IF_B_GOTO bpath
    addss xmm0, dword ptr [edx]
bpath:
    comiss xmm1, xmm0
    ret
STUB_FLOOR_FADE ENDP

; INT_TERRAIN_TILEUPD vtable slot 0xBE4794 (terrain On_Frame_Update 0x4E0A73: tile texture rebuild budget).
; B-renders do no tile work; the per-pair budget is the stock per-frame budget.
STUB_TERRAIN_OFU PROC
    IF_B_GOTO bpath
    RUNCNT IDX_INT_TERRAIN_TILEUPD
    jmp dword ptr [T_4E0A73]
bpath:
    SKIPCNT IDX_INT_TERRAIN_TILEUPD
    ret
STUB_TERRAIN_OFU ENDP

END
