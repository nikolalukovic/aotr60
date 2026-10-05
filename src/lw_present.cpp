// Living World strategic map presentation (phase 6). Armies, settlements and map objects on the LW map are W3D
// render objects moved by the LW client update 0x6C038B, a fixed step per update (A-only: GATE_GC_LWVIEW and
// GATE_LWM; LW logic reads the result). An A-render snapshots their transforms just before that update
// (LW_ICON_SNAP) and draws the LW scene with each moved object halfway between the snapshot and its new transform
// (LW_SCENE_PRESENT); the exact transforms are restored right after the scene draw, so picking, LW logic, saves and
// the B-render only ever see stock state. Units and camera share the battle presentation delay of half a frame.

#include "runtime.h"

#include <cmath>
#include <cstring>

using namespace game;

extern "C" {
volatile uint32_t g_lwRestoreN = 0;
}

namespace {

constexpr uint32_t kRoTransform = 0x18;   // Matrix3D 3x4, row-major
constexpr uint32_t kRoContainer = 0x7C;   // non-null: sub-object; its transform follows the container
constexpr uint32_t kRoSetTransform = 0x54;
constexpr uintptr_t kIconCount = 0x9A7ED6; // vt+0x34 of the LW client objects with an icon vector at +0x2C..+0x30
constexpr uintptr_t kIconAt = 0x9A7EE0;    // vt+0x3C
constexpr uint32_t kMaxObjects = 8192;
constexpr uint32_t kIndexSize = 16384;     // open addressing, power of two
constexpr float kCutoff = 400.0f;          // larger moves (teleports, spawns) are drawn as is
constexpr float kRotationAverageLimit = 0.25f;

struct Snap {
    uint8_t* ro;
    float m[12];
    bool presented;
};
Snap g_snap[kMaxObjects];
uint32_t g_snapCount = 0;
uint32_t g_index[kIndexSize]; // snapshot index + 1, 0 = empty
uint32_t g_snapRender = 0;    // g_renderId of the snapshot; 0 = none

struct Saved {
    uint8_t* ro;
    float m[12];
};
Saved g_saved[kMaxObjects];

using SetTransformFn = void(__thiscall*)(void* ro, const float* m);

void SetTransform(uint8_t* ro, const float* m)
{
    uint8_t* vtable = *reinterpret_cast<uint8_t**>(ro);
    (*reinterpret_cast<SetTransformFn*>(vtable + kRoSetTransform))(ro, m);
}

uint32_t Slot(const uint8_t* ro)
{
    uint32_t h = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ro)) * 2654435761u;
    return (h >> 18) & (kIndexSize - 1);
}

Snap* Find(const uint8_t* ro)
{
    for (uint32_t i = Slot(ro), n = 0; n < kIndexSize; i = (i + 1) & (kIndexSize - 1), ++n) {
        uint32_t e = g_index[i];
        if (!e) {
            return nullptr;
        }
        if (g_snap[e - 1].ro == ro) {
            return &g_snap[e - 1];
        }
    }
    return nullptr;
}

// Calls f(ro) for every top-level render object of every icon of every LW client object: the hash map at
// view+0x98 (bucket vector [view+0x9C, view+0xA0), chains through [node], object [node+8]; walked like
// 0x6C038B / 0x5E0A39 / 0x6BFEFD), icons [obj+0x2C, obj+0x30), render objects [icon+0x08] and [icon+0x14]
// (0x7FD3E5). Read-only. Returns false when a sanity limit is hit.
template <typename F>
bool ForEachObjectRo(uint8_t* view, F&& f)
{
    uint8_t** b0 = Field<uint8_t**>(view, 0x9C);
    uint8_t** b1 = Field<uint8_t**>(view, 0xA0);
    if (!b0 || b1 < b0 || b1 - b0 > 65536) {
        return false;
    }
    for (uint8_t** b = b0; b < b1; ++b) {
        int chain = 0;
        for (uint8_t* node = *b; node; node = *reinterpret_cast<uint8_t**>(node)) {
            if (++chain > 4096) {
                return false;
            }
            uint8_t* obj = Field<uint8_t*>(node, 8);
            if (!obj) {
                continue;
            }
            uint8_t* vt = *reinterpret_cast<uint8_t**>(obj);
            if (Field<uintptr_t>(vt, 0x34) != kIconCount || Field<uintptr_t>(vt, 0x3C) != kIconAt) {
                continue;
            }
            uint8_t** i0 = Field<uint8_t**>(obj, 0x2C);
            uint8_t** i1 = Field<uint8_t**>(obj, 0x30);
            if (!i0 || i1 < i0 || i1 - i0 > 64) {
                return false;
            }
            for (uint8_t** icon = i0; icon < i1; ++icon) {
                if (!*icon) {
                    continue;
                }
                static constexpr uint32_t kIconRo[] = {0x08, 0x14};
                for (uint32_t offset : kIconRo) {
                    uint8_t* ro = Field<uint8_t*>(*icon, offset);
                    if (ro && !Field<uint8_t*>(ro, kRoContainer)) {
                        f(ro);
                    }
                }
            }
        }
    }
    return true;
}

uint32_t Fnv1a(const void* data, size_t size)
{
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}

} // namespace

bool LwSceneShown()
{
    uint8_t* view = Ptr(kLwView);
    return view && Field<uint8_t>(view, 0x18) && !Field<uint8_t>(view, 0x19);
}

void LwPresentReset()
{
    g_snapRender = 0;
}

uint32_t LwIconHash()
{
    uint8_t* view = Ptr(kLwView);
    if (!view) {
        return 0;
    }
    uint32_t sum = 0;
    ForEachObjectRo(view, [&](uint8_t* ro) { sum += Fnv1a(ro + kRoTransform, 48); });
    return sum;
}

// LW_ICON_SNAP: 60-mode A-render, main thread, just before the LW client object update 0x6C038B.
extern "C" void __cdecl LwIconSnapshot(uint8_t* view)
{
    g_snapRender = 0;
    g_snapCount = 0;
    std::memset(g_index, 0, sizeof(g_index));
    bool overflow = false;
    bool ok = ForEachObjectRo(view, [&](uint8_t* ro) {
        if (overflow || Find(ro)) {
            return;
        }
        if (g_snapCount >= kMaxObjects) {
            overflow = true;
            return;
        }
        Snap& e = g_snap[g_snapCount];
        e.ro = ro;
        std::memcpy(e.m, ro + kRoTransform, 48);
        e.presented = false;
        ++g_snapCount;
        uint32_t i = Slot(ro);
        while (g_index[i]) {
            i = (i + 1) & (kIndexSize - 1);
        }
        g_index[i] = g_snapCount;
    });
    if (!ok || overflow) {
        ++g_lwStats.iconOverflow;
        return;
    }
    g_snapRender = g_renderId;
    ++g_lwStats.iconSnapshots;
}

// LW_SCENE_PRESENT, before the LW scene draw of the same A-render.
extern "C" void __cdecl LwPresentOpen()
{
    g_lwRestoreN = 0;
    uint8_t* view = Ptr(kLwView);
    if (!g_featPresent || !view || g_snapRender == 0 || g_snapRender != g_renderId) {
        return;
    }
    uint32_t n = 0;
    ForEachObjectRo(view, [&](uint8_t* ro) {
        Snap* e = Find(ro);
        if (!e || e->presented || n >= kMaxObjects) {
            return; // new since the snapshot, or already shown halfway
        }
        e->presented = true;
        const float* cur = reinterpret_cast<const float*>(ro + kRoTransform);
        if (std::memcmp(cur, e->m, 48) == 0) {
            return;
        }
        float dx = cur[3] - e->m[3];
        float dy = cur[7] - e->m[7];
        float dz = cur[11] - e->m[11];
        if (dx * dx + dy * dy + dz * dz > kCutoff * kCutoff) {
            ++g_lwStats.iconFar;
            return;
        }
        Saved& s = g_saved[n];
        s.ro = ro;
        std::memcpy(s.m, cur, 48);
        float mid[12];
        std::memcpy(mid, cur, 48);
        mid[3] = 0.5f * (e->m[3] + cur[3]);
        mid[7] = 0.5f * (e->m[7] + cur[7]);
        mid[11] = 0.5f * (e->m[11] + cur[11]);
        float maxDelta = 0.0f;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                float d = std::fabs(cur[4 * row + col] - e->m[4 * row + col]);
                maxDelta = d > maxDelta ? d : maxDelta;
            }
        }
        if (maxDelta <= kRotationAverageLimit) {
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    int i = 4 * row + col;
                    mid[i] = 0.5f * (e->m[i] + cur[i]);
                }
            }
        }
        ++n;
        g_lwRestoreN = n;
        SetTransform(ro, mid);
        ++g_lwStats.iconPresented;
    });
}

// LW_SCENE_PRESENT, right after the LW scene draw (and the drawFrame exit backstop): exact transforms back.
extern "C" void __cdecl LwPresentClose()
{
    for (uint32_t i = g_lwRestoreN; i-- > 0;) {
        const Saved& s = g_saved[i];
        SetTransform(s.ro, s.m);
        if (std::memcmp(s.ro + kRoTransform, s.m, 48) != 0) {
            ++g_lwStats.iconRestoreBad;
        }
    }
    g_lwRestoreN = 0;
}
