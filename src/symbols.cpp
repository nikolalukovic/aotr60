#include "symbols.h"

#include "runtime.h"

#include <cstring>

// Assembly stubs (src/stubs/stubs.asm). Declared as plain functions only to take their addresses.
extern "C" {
void C0_STUB();
void HALT_STUB();
void ISTICK_CAVE();
void RESET_CAVE();
void C4_KEY_STUB();
void STUB_INC_DD1BEC();
void STUB_INC_DD1A40();
void STUB_INC_DD1BFC();
void CAVE_SHORE();
void STUB_TREES();
void CAVE_SHRUB();
void CAVE_DECAL_SPIRAL();
void STUB_RECOIL();
void STUB_MB_ENDPAN();
void C3_SyncDelta();
void S0_FFGuard();
void C7A_PartMgrGate();
void C7B_PartMgrTail();
void SNOW_Gate_49405A();
void SNOW_Gate_492C35();
void PACER_CAVE();
void PRESENT_STUB();
void GAP_RESET_CAVE();
void LOD_STUB();
void MDRAW_CAVE();
void CAVE_S1();
void CAVE_S2();
void CAVE_S2E();
void CAVE_SCENE_OPEN();
void STUB_SCENE_RESTORE();
void CAVE_GUARD_SCT();
void CAVE_GUARD_MB();
void PalGate();
void LwmGate();
void Vt188Detour();
void IguiThunkTel();
void Gate_62B385();
void GateVt28_APT();
void GateVt28_Radar();
void GateVt28_KbdDrain();
void GateVt28_Audio();
void Gate_GC_LOOKAT();
void Gate_GC_LWXLAT();
void Gate_GC_GESTURE();
void Gate_GC_LWVIEW();
void Gate_GC_LWUI();
void Gate_GC_CLOUD();
void Gate_GC_FIRE();
void Gate_GC_ANIM2D();
void Sel_GC_KBD();
void Gate_GC_SKEVA();
void Gate_GC_EVA();
void Sel_GC_MOUSE();
void Gate_GC_POPUPS();
void Gate_GC_WM();
void Cave_GC_DRAWBLK();
void Gate_GC_PENDGAME2();
void Gate_GC_TERRAIN();
void Gate_GC_DISPUPD();
void Gate_GC_DISPUPD_LW();
void Gate_GC_DSM();
void Gate_GC_SHELL();
void Gate_GC_IGUI();
void Gate_GC_DISPUPD_INTRO();
void CAVE_LIGHT_PULSE();
void CAVE_ROPE();
void CAVE_TREAD();
void CAVE_TRUCK_CAB();
void CAVE_TRUCK_TRAILER();
void CAVE_TRUCK_WHEEL();
void CAVE_DEBRIS();
void CAVE_SUBOBJ_DELAY();
void CAVE_SUBOBJ_ALPHA();
void STUB_LIGHTPULSE();
void CAVE_LP_DBL();
void CAVE_OVL_FADE();
void STUB_SUBT_STATE();
void STUB_SUBT_SCROLL();
void CAVE_UIPART();
void CAVE_WANIM_RISE();
}

namespace {

#define SYM(name) {#name, reinterpret_cast<const void*>(&name)}
#define VAR(name) {#name, const_cast<const void*>(reinterpret_cast<const volatile void*>(&name))}

const Symbol kSymbols[] = {
    // core
    SYM(C0_STUB),
    SYM(HALT_STUB),
    SYM(ISTICK_CAVE),
    SYM(RESET_CAVE),
    SYM(C4_KEY_STUB),
    SYM(LogicUpdateWrapper),
    // render-internal integrators
    SYM(STUB_INC_DD1BEC),
    SYM(STUB_INC_DD1A40),
    SYM(STUB_INC_DD1BFC),
    VAR(g_ovlFrameStep),
    VAR(g_ovlStep003),
    VAR(g_ovlStep00225),
    VAR(g_ovlStep002),
    VAR(g_ovlStep0005d),
    VAR(g_ovlStep005),
    VAR(g_riverStepU),
    VAR(g_riverStepV),
    SYM(CAVE_SHORE),
    SYM(STUB_TREES),
    SYM(CAVE_SHRUB),
    SYM(CAVE_DECAL_SPIRAL),
    VAR(g_matPassDecay),
    SYM(STUB_RECOIL),
    SYM(STUB_MB_ENDPAN),
    // clock
    SYM(C3_SyncDelta),
    SYM(S0_FFGuard),
    SYM(C5_CalcPhysicsXform),
    SYM(C7A_PartMgrGate),
    SYM(C7B_PartMgrTail),
    SYM(SNOW_Gate_49405A),
    SYM(SNOW_Gate_492C35),
    // pacing
    SYM(PACER_CAVE),
    SYM(PRESENT_STUB),
    SYM(GAP_RESET_CAVE),
    SYM(LOD_STUB),
    SYM(MDRAW_CAVE),
    // camera
    SYM(CAVE_S1),
    SYM(CAVE_S2),
    SYM(CAVE_S2E),
    SYM(CAVE_SCENE_OPEN),
    SYM(STUB_SCENE_RESTORE),
    SYM(CAVE_GUARD_SCT),
    SYM(CAVE_GUARD_MB),
    // A-only gates
    SYM(PalGate),
    SYM(LwmGate),
    SYM(Vt188Detour),
    SYM(IguiThunkTel),
    SYM(Gate_62B385),
    SYM(GateVt28_APT),
    SYM(GateVt28_Radar),
    SYM(GateVt28_KbdDrain),
    SYM(GateVt28_Audio),
    SYM(Gate_GC_LOOKAT),
    SYM(Gate_GC_LWXLAT),
    SYM(Gate_GC_GESTURE),
    SYM(Gate_GC_LWVIEW),
    SYM(Gate_GC_LWUI),
    SYM(Gate_GC_CLOUD),
    SYM(Gate_GC_FIRE),
    SYM(Gate_GC_ANIM2D),
    SYM(Sel_GC_KBD),
    SYM(Gate_GC_SKEVA),
    SYM(Gate_GC_EVA),
    SYM(Sel_GC_MOUSE),
    SYM(Gate_GC_POPUPS),
    SYM(Gate_GC_WM),
    SYM(Cave_GC_DRAWBLK),
    SYM(Gate_GC_PENDGAME2),
    SYM(Gate_GC_TERRAIN),
    SYM(Gate_GC_DISPUPD),
    SYM(Gate_GC_DISPUPD_LW),
    SYM(Gate_GC_DSM),
    SYM(Gate_GC_SHELL),
    SYM(Gate_GC_IGUI),
    SYM(Gate_GC_DISPUPD_INTRO),
    // render-internal integrators, part 2
    VAR(g_lightPhaseNum),
    SYM(CAVE_LIGHT_PULSE),
    VAR(g_lightPulseLtr),
    SYM(CAVE_ROPE),
    SYM(CAVE_TREAD),
    SYM(CAVE_TRUCK_CAB),
    SYM(CAVE_TRUCK_TRAILER),
    SYM(CAVE_TRUCK_WHEEL),
    VAR(g_wakeStep),
    SYM(CAVE_DEBRIS),
    SYM(CAVE_SUBOBJ_DELAY),
    SYM(CAVE_SUBOBJ_ALPHA),
    VAR(g_outlineStep),
    SYM(STUB_LIGHTPULSE),
    SYM(CAVE_LP_DBL),
    VAR(g_instFadeStep),
    SYM(CAVE_OVL_FADE),
    SYM(STUB_SUBT_STATE),
    SYM(STUB_SUBT_SCROLL),
    SYM(CAVE_UIPART),
    SYM(CAVE_WANIM_RISE),
};

#undef SYM
#undef VAR

} // namespace

const void* FindSymbol(const char* name)
{
    for (const Symbol& s : kSymbols) {
        if (std::strcmp(s.name, name) == 0) {
            return s.address;
        }
    }
    return nullptr;
}

const Symbol* AllSymbols(size_t* count)
{
    *count = sizeof(kSymbols) / sizeof(kSymbols[0]);
    return kSymbols;
}
