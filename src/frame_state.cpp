#include "frame_state.h"

void FrameState::BeginIteration(bool allowStay, bool allowStart, int stepperS, bool logicAdvanced)
{
    // Mode changes only at a pair boundary: before an X iteration (or between two stock iterations).
    if (m60 && nextIsX && !allowStay) {
        Reset();
    }
    else if (!m60 && allowStay && allowStart && stepperS == 1 && logicAdvanced) {
        m60 = 1;
        nextIsX = true;
        prevAdvancingA = false;
        owedSyncMs = 0;
    }

    ++renderId;
    if (!m60) {
        uiTick = 1;
        inB = 0;
        return;
    }
    uiTick = nextIsX ? 1 : 0;
    inB = nextIsX ? 0 : 1;
    nextIsX = !nextIsX;
}

void FrameState::Reset()
{
    m60 = 0;
    uiTick = 1;
    inB = 0;
    presWindow = 0;
    nextIsX = true;
    prevAdvancingA = false;
    owedSyncMs = 0;
}

int32_t FrameState::SyncDelta(int32_t frames, int32_t frameLengthMs)
{
    if (!m60) {
        return frameLengthMs * frames;
    }
    if (!inB) {
        if (frames > 0) {
            syncToggle = !syncToggle;
            int32_t half = syncToggle ? frameLengthMs / 2 : frameLengthMs - frameLengthMs / 2;
            owedSyncMs = frameLengthMs * frames - half;
            prevAdvancingA = true;
            return half;
        }
        prevAdvancingA = false;
        return 0;
    }
    if (prevAdvancingA) {
        prevAdvancingA = false;
        int32_t owed = owedSyncMs;
        owedSyncMs = 0;
        return owed;
    }
    return 0;
}

float FrameState::PresentationFraction(int s)
{
    if (s >= 7 || s <= 0) {
        return 1.0f;
    }
    float f = static_cast<float>(2 * s - 1) / 12.0f;
    return f > 1.0f ? 1.0f : f;
}

uint32_t FrameState::CacheKey(uint32_t mFrame) const
{
    if (!m60) {
        return mFrame;
    }
    return 0x80000000u | ((2u * mFrame - presWindow) & 0x7FFFFFFFu);
}
