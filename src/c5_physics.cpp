// C5: client-physics replay (PLAN §1.5). Drawable::calcPhysicsXform (0x67BDC0) steps the locomotor's visual
// pitch/roll/bob springs at most once per m_frame and returns 0 on later calls. A B-render must show what stock
// render k showed, so it replays the A-render's result instead of calling the original (which would also stamp
// persistent locomotor state).

#include "runtime.h"

#include <cstring>

using namespace game;

namespace {

struct C5Entry {
    uint8_t* loco;
    uint8_t* drawable;
    uint32_t frame;
    uint32_t renderIdA;
    uint32_t replayId;
    float out[4];
};

constexpr uint32_t kSlots = 4096;
constexpr uint32_t kProbe = 8;
C5Entry g_table[kSlots];

uint32_t Hash(const uint8_t* loco)
{
    return ((static_cast<uint32_t>(reinterpret_cast<uintptr_t>(loco)) >> 3) * 0x9E3779B1u) >> 20;
}

C5Entry* Find(const uint8_t* loco)
{
    uint32_t h = Hash(loco);
    for (uint32_t i = 0; i < kProbe; ++i) {
        C5Entry& e = g_table[(h + i) & (kSlots - 1)];
        if (e.loco == loco) {
            return &e;
        }
    }
    return nullptr;
}

void Store(uint8_t* loco, uint8_t* drawable, uint32_t frame, const float* out)
{
    uint32_t h = Hash(loco);
    C5Entry* slot = nullptr;
    for (uint32_t i = 0; i < kProbe; ++i) {
        C5Entry& e = g_table[(h + i) & (kSlots - 1)];
        if (e.loco == loco) {
            slot = &e;
            break;
        }
        if (!slot && (e.loco == nullptr || e.renderIdA != g_renderId)) {
            slot = &e;
        }
    }
    if (!slot) {
        ++g_c5Stats.drops;
        return;
    }
    slot->loco = loco;
    slot->drawable = drawable;
    slot->frame = frame;
    slot->renderIdA = g_renderId;
    slot->replayId = 0;
    std::memcpy(slot->out, out, sizeof(slot->out));
}

using CalcPhysicsXformFn = bool(__thiscall*)(uint8_t* drawable, float* out);

uint8_t* LocomotorOf(uint8_t* drawable)
{
    uint8_t* obj = *reinterpret_cast<uint8_t**>(drawable + 0xFC);
    uint8_t* ai = obj ? *reinterpret_cast<uint8_t**>(obj + 0x260) : nullptr;
    return ai ? *reinterpret_cast<uint8_t**>(ai + 0x1F0) : nullptr;
}

} // namespace

C5Stats g_c5Stats;

void C5Reset()
{
    std::memset(g_table, 0, sizeof(g_table));
}

extern "C" bool __fastcall C5_CalcPhysicsXform(uint8_t* drawable, void*, float* out)
{
    auto orig = reinterpret_cast<CalcPhysicsXformFn>(&C5_Tramp);
    if (!g_m60 || !OnMainThread()) {
        return orig(drawable, out);
    }
    uint8_t* loco = LocomotorOf(drawable);
    if (!g_inB) {
        bool r = orig(drawable, out);
        if (r && loco) {
            Store(loco, drawable, Field<uint32_t>(loco, 0xAC), out);
        }
        return r;
    }
    if (!loco) {
        return false;
    }
    C5Entry* e = Find(loco);
    if (e && e->drawable == drawable && e->renderIdA == g_renderId - 1 && e->frame == Field<uint32_t>(loco, 0xAC) &&
        e->replayId != g_renderId) {
        std::memcpy(out, e->out, sizeof(e->out));
        e->replayId = g_renderId;
        ++g_c5Stats.replays;
        return true;
    }
    uint8_t* gc = Ptr(kTheGameClient);
    if (gc && Field<uint32_t>(loco, 0xAC) != Field<uint32_t>(gc, 0x10)) {
        ++g_c5Stats.suppressed;
    }
    return false;
}
