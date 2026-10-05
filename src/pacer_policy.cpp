#include "pacer_policy.h"

bool FallbackPolicy::AddWindow(const PacerWindow& w)
{
    bool overloaded = w.lateRatio > 0.5 || w.lostRatio > 0.25;
    bool slow = w.lostRatio > 0.05;
    bool skipping = w.skipRatio > 0.10;
    bool sustained = w.onTimeRatio < 0.5 && w.lostRatio > 0.003;
    overloadedWindows_ = overloaded ? overloadedWindows_ + 1 : 0;
    slowWindows_ = slow ? slowWindows_ + 1 : 0;
    skipWindows_ = skipping ? skipWindows_ + 1 : 0;
    sustainedWindows_ = sustained ? sustainedWindows_ + 1 : 0;
    cleanWindows_ = (overloaded || slow || skipping || sustained || w.lostRatio > 0.02) ? 0 : cleanWindows_ + 1;
    if (cleanWindows_ >= 60) {
        fallbacks_ = 0; // five clean minutes: the backoff starts at 30 s again
    }
    if (overloadedWindows_ >= 2 || slowWindows_ >= 3 || skipWindows_ >= 2 || sustainedWindows_ >= 2) {
        Reset();
        return true;
    }
    return false;
}

void FallbackPolicy::Reset()
{
    overloadedWindows_ = 0;
    slowWindows_ = 0;
    skipWindows_ = 0;
    sustainedWindows_ = 0;
}

int FallbackPolicy::NextBackoffSeconds()
{
    int backoff = 30 << (fallbacks_ < 4 ? fallbacks_ : 4);
    ++fallbacks_;
    return backoff;
}
