#include "runtime.h"

#include <cstring>

extern "C" {

volatile uint8_t g_m60 = 0;
volatile uint8_t g_uiTick = 1;
volatile uint8_t g_inB = 0;
volatile uint8_t g_inClientUpdate = 0;
volatile uint8_t g_skipB = 0;
volatile uint8_t g_forceHalt = 0;
volatile uint32_t g_mainTid = 0;
volatile uint32_t g_renderId = 0;

volatile int32_t g_syncOwed = 0;
volatile uint32_t g_syncHalfPar = 0;

volatile uint8_t g_featPresent = 0;
volatile uint8_t g_featCamInterp = 0;
volatile uint32_t g_pwActive = 0;
volatile uint8_t g_pw1Open = 0;
volatile uint8_t g_pw2Open = 0;
volatile float g_presFrac = 1.0f;
volatile float g_fracSaved = 0.0f;
volatile uint8_t g_swapActive = 0;

volatile uint32_t g_pacerRanRid = 0;
volatile uint32_t g_mDrawRid = 0xFFFFFFFFu;
volatile int32_t g_mDrawAM = 0;
volatile uint8_t g_gapDevLost = 0;
volatile uint8_t g_gapReset = 0;

// Initialised from the game's own constants by InitIntegratorVariables() before any site is patched; the values
// here are the same constants, so 30 mode stays bit-identical even if that copy were skipped.
float g_ovlFrameStep = 0.6f;
float g_ovlStep003 = 0.03f;
float g_ovlStep00225 = 0.0225f;
float g_ovlStep002 = 0.02f;
double g_ovlStep0005d = 0.005;
float g_ovlStep005 = 0.05f;
float g_riverStepU = 8.25e-5f;
float g_riverStepV = 1.65e-4f;
float g_matPassDecay = 0.8f;
int32_t g_shoreStepA = 33;
int32_t g_shoreStepB = 0;
float g_lightPhaseNum = 1.0f;
float g_wakeStep = 0.005f;
float g_outlineStep = 1.0f / 60.0f;
float g_instFadeStep = 1.0f / 12.0f;
int32_t g_lightPulseLtr = 5;
volatile uint8_t g_lp4b = 0;
volatile uint8_t g_uiPart4b = 0;
volatile uint8_t g_wanim4b = 0;
volatile uint32_t g_radarAFrame = 0xFFFFFFFFu;
volatile uint32_t g_uiSeqLast = 0;
volatile uint8_t g_uniformScroll = 0;
volatile uint8_t g_claimSmooth = 0;
volatile uint32_t g_claimFrame = 0;

volatile uint32_t g_siteRun[sites::kSiteCount] = {};
volatile uint32_t g_siteSkip[sites::kSiteCount] = {};
volatile uint32_t g_tmFFin60 = 0;
volatile uint32_t g_anomaly = 0;
volatile uint32_t g_unknownPath = 0;
volatile uint32_t g_vt188Calls = 0;
volatile uint32_t g_vt188LastRet = 0;
volatile uint32_t g_palBadRet = 0;
volatile uint32_t g_iguiBadRet = 0;

} // extern "C"

namespace {

struct StockConstants {
    float ovlFrameStep, ovlStep003, ovlStep00225, ovlStep002;
    double ovlStep0005d;
    float ovlStep005, riverStepU, riverStepV, matPassDecay;
    float lightPhaseNum, wakeStep, outlineStep, instFadeStep;
    int32_t lightPulseLtr;
};

StockConstants g_stock{0.6f,     0.03f,    0.0225f, 0.02f,         0.005,        0.05f, 8.25e-5f,
                       1.65e-4f, 0.8f,     1.0f,    0.005f,        1.0f / 60.0f, 1.0f / 12.0f, 5};

template <typename T>
T CopyFrom(uintptr_t address)
{
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return value;
}

} // namespace

void InitIntegratorVariables()
{
    // The .rdata constants the redirected operands used to read (sites.json, group "render-internal integrators").
    g_stock.ovlFrameStep = CopyFrom<float>(0xBDAD70);
    g_stock.ovlStep003 = CopyFrom<float>(0xBDC540);
    g_stock.ovlStep00225 = CopyFrom<float>(0xBE5228);
    g_stock.ovlStep002 = CopyFrom<float>(0xBDC320);
    g_stock.ovlStep0005d = CopyFrom<double>(0xBE5220);
    g_stock.ovlStep005 = CopyFrom<float>(0xBDD760);
    g_stock.riverStepU = CopyFrom<float>(0xBE5608);
    g_stock.riverStepV = CopyFrom<float>(0xBE5604);
    g_stock.matPassDecay = CopyFrom<float>(0xBDE8D8);
    g_stock.lightPhaseNum = CopyFrom<float>(0xBD1908);
    g_stock.wakeStep = CopyFrom<float>(0xD9A3A0);
    g_stock.outlineStep = CopyFrom<float>(0xBDC1FC);
    g_stock.instFadeStep = CopyFrom<float>(0xBD88C8);
    g_stock.lightPulseLtr = CopyFrom<int32_t>(0xD9F608);
    g_lightPulseLtr = g_stock.lightPulseLtr;
    SetIntegratorVariablesForB(false);
}

void SetIntegratorVariablesForB(bool bRender)
{
    if (!bRender) {
        g_ovlFrameStep = g_stock.ovlFrameStep;
        g_ovlStep003 = g_stock.ovlStep003;
        g_ovlStep00225 = g_stock.ovlStep00225;
        g_ovlStep002 = g_stock.ovlStep002;
        g_ovlStep0005d = g_stock.ovlStep0005d;
        g_ovlStep005 = g_stock.ovlStep005;
        g_riverStepU = g_stock.riverStepU;
        g_riverStepV = g_stock.riverStepV;
        g_matPassDecay = g_stock.matPassDecay;
        g_shoreStepA = 33;
        g_shoreStepB = 0;
        g_lightPhaseNum = g_stock.lightPhaseNum;
        g_wakeStep = g_stock.wakeStep;
        g_outlineStep = g_stock.outlineStep;
        g_instFadeStep = g_stock.instFadeStep;
        return;
    }
    // Phase 2a: every visible integrator is A-only (exact 30 Hz steps); B-renders step by zero.
    g_ovlFrameStep = 0.0f;
    g_ovlStep003 = 0.0f;
    g_ovlStep00225 = 0.0f;
    g_ovlStep002 = 0.0f;
    g_ovlStep0005d = 0.0;
    g_ovlStep005 = 0.0f;
    g_riverStepU = 0.0f;
    g_riverStepV = 0.0f;
    g_matPassDecay = 1.0f;
    g_shoreStepB = 0;
    g_lightPhaseNum = 0.0f;
    g_wakeStep = 0.0f;
    g_outlineStep = 0.0f;
    g_instFadeStep = 0.0f;
}
