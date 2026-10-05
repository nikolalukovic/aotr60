// Generated from tools/sites.json by tools/gen_sites.py - do not edit.
#pragma once
#include <cstdint>

namespace sites {

enum class Kind : uint8_t { CallGate, JmpDetour, FuncDetour, OperandRedirect, PtrSlot, DataWrite };
enum class Phase : uint8_t { Telemetry, Phase1, Phase2a, Phase2b, Phase3, Phase4b, Phase6 };
enum class TokenType : uint8_t { Byte, Rel32, Abs32 };

struct Token {
    TokenType type;
    uint8_t value;      // TokenType::Byte
    const char* symbol; // Rel32 / Abs32
};

struct Site {
    const char* id;
    Phase phase;
    Kind kind;
    uint32_t address;
    uint32_t length;
    const uint8_t* original;
    const Token* replacement;
    uint32_t tokenCount;
};

inline constexpr uint8_t kC0_PRERENDER_original[] = {0xFF, 0x90, 0x9C, 0x00, 0x00, 0x00};
inline constexpr Token kC0_PRERENDER_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "C0_STUB"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kHALT_original[] = {0x84, 0xDB, 0xA1, 0x88, 0x43, 0xDE, 0x00};
inline constexpr Token kHALT_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "HALT_STUB"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kISTICK_original[] = {0xA1, 0x0C, 0xF6, 0xD9, 0x00};
inline constexpr Token kISTICK_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "ISTICK_CAVE"}};
inline constexpr uint8_t kRESET_original[] = {0xE9, 0xF2, 0x44, 0x1F, 0x00};
inline constexpr Token kRESET_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "RESET_CAVE"}};
inline constexpr uint8_t kC4_KEY_A_original[] = {0x8B, 0x0D, 0x88, 0x43, 0xDE, 0x00, 0x8B, 0x01, 0x53, 0xFF, 0x50, 0x7C};
inline constexpr Token kC4_KEY_A_replacement[] = {{TokenType::Byte, 0x53, nullptr}, {TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "C4_KEY_STUB"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kC4_KEY_B_original[] = {0x8B, 0x0D, 0x88, 0x43, 0xDE, 0x00, 0x8B, 0x01, 0xFF, 0x50, 0x7C};
inline constexpr Token kC4_KEY_B_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "C4_KEY_STUB"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kC4_KEY_C_original[] = {0x8B, 0x0D, 0x88, 0x43, 0xDE, 0x00, 0x8B, 0x01, 0x83, 0xC4, 0x10, 0xFF, 0x50, 0x7C};
inline constexpr Token kC4_KEY_C_replacement[] = {{TokenType::Byte, 0x83, nullptr}, {TokenType::Byte, 0xC4, nullptr}, {TokenType::Byte, 0x10, nullptr}, {TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "C4_KEY_STUB"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kTEL_LOGIC_UPDATE_original[] = {0xE8, 0xE4, 0x62, 0x00};
inline constexpr Token kTEL_LOGIC_UPDATE_replacement[] = {{TokenType::Abs32, 0, "LogicUpdateWrapper"}};
inline constexpr uint8_t kTEL_LW_LOGIC_UPDATE_original[] = {0x0E, 0xE5, 0x6B, 0x00};
inline constexpr Token kTEL_LW_LOGIC_UPDATE_replacement[] = {{TokenType::Abs32, 0, "LwLogicUpdateWrapper"}};
inline constexpr uint8_t kINT_VF_BW_S1_original[] = {0xFF, 0x05, 0xEC, 0x1B, 0xDD, 0x00};
inline constexpr Token kINT_VF_BW_S1_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_INC_DD1BEC"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_VF_BW_S2_original[] = {0xFF, 0x05, 0xEC, 0x1B, 0xDD, 0x00};
inline constexpr Token kINT_VF_BW_S2_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_INC_DD1BEC"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_VF_BW_N1_original[] = {0xFF, 0x05, 0xEC, 0x1B, 0xDD, 0x00};
inline constexpr Token kINT_VF_BW_N1_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_INC_DD1BEC"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_VF_BW_N2_original[] = {0xFF, 0x05, 0xEC, 0x1B, 0xDD, 0x00};
inline constexpr Token kINT_VF_BW_N2_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_INC_DD1BEC"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_VF_MASK_IN_original[] = {0xFF, 0x05, 0x40, 0x1A, 0xDD, 0x00};
inline constexpr Token kINT_VF_MASK_IN_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_INC_DD1A40"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_VF_MASK_OUT_original[] = {0x39, 0x05, 0x40, 0x1A, 0xDD, 0x00, 0x7D, 0x2C, 0xF3, 0x0F, 0x2A, 0x05, 0x40, 0x1A, 0xDD, 0x00, 0xFF, 0x05, 0x40, 0x1A, 0xDD, 0x00};
inline constexpr Token kINT_VF_MASK_OUT_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_VF_MASK_OUT"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_VF_MONO_1_original[] = {0xFF, 0x05, 0xFC, 0x1B, 0xDD, 0x00};
inline constexpr Token kINT_VF_MONO_1_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_INC_DD1BFC"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_VF_MONO_2_original[] = {0xFF, 0x05, 0xFC, 0x1B, 0xDD, 0x00};
inline constexpr Token kINT_VF_MONO_2_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_INC_DD1BFC"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_OVL_FRAME_original[] = {0xF3, 0x0F, 0x58, 0x05, 0x70, 0xAD, 0xBD, 0x00};
inline constexpr Token kINT_OVL_FRAME_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x58, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlFrameStep"}};
inline constexpr uint8_t kINT_OVL_U_original[] = {0xF3, 0x0F, 0x58, 0x05, 0x40, 0xC5, 0xBD, 0x00};
inline constexpr Token kINT_OVL_U_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x58, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep003"}};
inline constexpr uint8_t kINT_OVL_V_original[] = {0xF3, 0x0F, 0x5C, 0x05, 0x28, 0x52, 0xBE, 0x00};
inline constexpr Token kINT_OVL_V_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x5C, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep00225"}};
inline constexpr uint8_t kINT_OVL_W_original[] = {0xF3, 0x0F, 0x58, 0x05, 0x20, 0xC3, 0xBD, 0x00};
inline constexpr Token kINT_OVL_W_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x58, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep002"}};
inline constexpr uint8_t kINT_OVL2_U_original[] = {0xF3, 0x0F, 0x59, 0x05, 0x40, 0xC5, 0xBD, 0x00};
inline constexpr Token kINT_OVL2_U_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x59, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep003"}};
inline constexpr uint8_t kINT_OVL2_V_original[] = {0xF3, 0x0F, 0x59, 0x05, 0x28, 0x52, 0xBE, 0x00};
inline constexpr Token kINT_OVL2_V_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x59, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep00225"}};
inline constexpr uint8_t kINT_OVL2_W_original[] = {0xDC, 0x0D, 0x20, 0x52, 0xBE, 0x00};
inline constexpr Token kINT_OVL2_W_replacement[] = {{TokenType::Byte, 0xDC, nullptr}, {TokenType::Byte, 0x0D, nullptr}, {TokenType::Abs32, 0, "g_ovlStep0005d"}};
inline constexpr uint8_t kINT_OVL3_U_original[] = {0xF3, 0x0F, 0x59, 0x05, 0x40, 0xC5, 0xBD, 0x00};
inline constexpr Token kINT_OVL3_U_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x59, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep003"}};
inline constexpr uint8_t kINT_OVL3_V_original[] = {0xF3, 0x0F, 0x59, 0x05, 0x28, 0x52, 0xBE, 0x00};
inline constexpr Token kINT_OVL3_V_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x59, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep00225"}};
inline constexpr uint8_t kINT_OVL3_W_original[] = {0xDC, 0x0D, 0x20, 0x52, 0xBE, 0x00};
inline constexpr Token kINT_OVL3_W_replacement[] = {{TokenType::Byte, 0xDC, nullptr}, {TokenType::Byte, 0x0D, nullptr}, {TokenType::Abs32, 0, "g_ovlStep0005d"}};
inline constexpr uint8_t kINT_OVL2_SCROLL_original[] = {0xF3, 0x0F, 0x10, 0x05, 0x60, 0xD7, 0xBD, 0x00};
inline constexpr Token kINT_OVL2_SCROLL_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x10, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep005"}};
inline constexpr uint8_t kINT_OVL3_SCROLL_original[] = {0xF3, 0x0F, 0x10, 0x05, 0x60, 0xD7, 0xBD, 0x00};
inline constexpr Token kINT_OVL3_SCROLL_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x10, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_ovlStep005"}};
inline constexpr uint8_t kINT_RIVER_U_original[] = {0xF3, 0x0F, 0x58, 0x05, 0x08, 0x56, 0xBE, 0x00};
inline constexpr Token kINT_RIVER_U_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x58, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_riverStepU"}};
inline constexpr uint8_t kINT_RIVER_V_original[] = {0xF3, 0x0F, 0x58, 0x0D, 0x04, 0x56, 0xBE, 0x00};
inline constexpr Token kINT_RIVER_V_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x58, nullptr}, {TokenType::Byte, 0x0D, nullptr}, {TokenType::Abs32, 0, "g_riverStepV"}};
inline constexpr uint8_t kINT_SHORE_original[] = {0x83, 0x46, 0x74, 0x21, 0xEB, 0x08};
inline constexpr Token kINT_SHORE_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_SHORE"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_TREES_original[] = {0xE8, 0xA1, 0xE7, 0x01, 0x00};
inline constexpr Token kINT_TREES_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_TREES"}};
inline constexpr uint8_t kINT_SHRUBS_original[] = {0x80, 0x7D, 0xFF, 0x00, 0x75, 0x08};
inline constexpr Token kINT_SHRUBS_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_SHRUB"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_DECAL_SPIRAL_original[] = {0x8B, 0x46, 0x04, 0xF3, 0x0F, 0x10, 0x40, 0x5C};
inline constexpr Token kINT_DECAL_SPIRAL_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_DECAL_SPIRAL"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_MAT_DECAY_original[] = {0xF3, 0x0F, 0x59, 0x05, 0xD8, 0xE8, 0xBD, 0x00};
inline constexpr Token kINT_MAT_DECAY_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x59, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_matPassDecay"}};
inline constexpr uint8_t kINT_RECOIL_original[] = {0xE8, 0xC6, 0xCD, 0xFE, 0xFF};
inline constexpr Token kINT_RECOIL_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_RECOIL"}};
inline constexpr uint8_t kINT_MB_ENDPAN_original[] = {0xFF, 0x4B, 0x04, 0x83, 0x7B, 0x04, 0x02};
inline constexpr Token kINT_MB_ENDPAN_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_MB_ENDPAN"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kC3_SYNC_original[] = {0xA1, 0x8C, 0x7A, 0xDC, 0x00, 0x0F, 0xAF, 0xC6, 0x01, 0x05, 0x80, 0x75, 0xDC, 0x00};
inline constexpr Token kC3_SYNC_replacement[] = {{TokenType::Byte, 0x56, nullptr}, {TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "C3_SyncDelta"}, {TokenType::Byte, 0x01, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Byte, 0x80, nullptr}, {TokenType::Byte, 0x75, nullptr}, {TokenType::Byte, 0xDC, nullptr}, {TokenType::Byte, 0x00, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kS0_FFGUARD_original[] = {0x8B, 0x4D, 0xE4, 0xE8, 0x7B, 0xF0, 0x03, 0x00};
inline constexpr Token kS0_FFGUARD_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "S0_FFGuard"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kC5_PHYS_original[] = {0x55, 0x8B, 0xEC, 0x51, 0x57};
inline constexpr Token kC5_PHYS_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "C5_CalcPhysicsXform"}};
inline constexpr uint8_t kC7_PARTMGR_DRAW_original[] = {0x8B, 0x0D, 0x44, 0x37, 0xDE, 0x00, 0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kC7_PARTMGR_DRAW_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "C7A_PartMgrGate"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kC7_PARTMGR_TAIL_original[] = {0x8B, 0x0D, 0x44, 0x37, 0xDE, 0x00, 0x8B, 0x01, 0xFF, 0x60, 0x28};
inline constexpr Token kC7_PARTMGR_TAIL_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "C7B_PartMgrTail"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kSNOW_WEATHER_original[] = {0xE8, 0xC8, 0xFB, 0xFF, 0xFF};
inline constexpr Token kSNOW_WEATHER_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "SNOW_Gate_49405A"}};
inline constexpr uint8_t kSNOW_LIGHTNING_original[] = {0xE8, 0x9C, 0xE7, 0xFF, 0xFF};
inline constexpr Token kSNOW_LIGHTNING_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "SNOW_Gate_492C35"}};
inline constexpr uint8_t kPACER_original[] = {0xFF, 0x15, 0x20, 0x09, 0xBD, 0x00};
inline constexpr Token kPACER_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "PACER_CAVE"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kPRESENT_original[] = {0xA1, 0x74, 0x34, 0xDD, 0x00, 0x8B, 0x10, 0x53, 0x53, 0x53, 0x53, 0x50, 0xFF, 0x52, 0x44};
inline constexpr Token kPRESENT_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "PRESENT_STUB"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kGAP_RESET_original[] = {0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};
inline constexpr Token kGAP_RESET_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "GAP_RESET_CAVE"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kLOD_GATE_original[] = {0x8B, 0xCF, 0xE8, 0x01, 0x79, 0xFF, 0xFF, 0x8B, 0xCF, 0xE8, 0x19, 0x81, 0xFF, 0xFF};
inline constexpr Token kLOD_GATE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "LOD_STUB"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kMDRAW_original[] = {0x83, 0xF8, 0x01, 0x7E, 0x28, 0xFF, 0x0D, 0xB4, 0x8C, 0xD9, 0x00, 0x83, 0x3D, 0xB4, 0x8C, 0xD9, 0x00, 0x01, 0x0F, 0x8F, 0xE8, 0x02, 0x00, 0x00};
inline constexpr Token kMDRAW_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "MDRAW_CAVE"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kS1_VIEWUPDATE_SKIP_original[] = {0xC6, 0x45, 0xF2, 0x00, 0x0F, 0x84, 0xDD, 0x00, 0x00, 0x00};
inline constexpr Token kS1_VIEWUPDATE_SKIP_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_S1"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kS2_PASS_OPEN_original[] = {0xA1, 0x0C, 0x1E, 0xDD, 0x00};
inline constexpr Token kS2_PASS_OPEN_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_S2"}};
inline constexpr uint8_t kS2E_PASS_CLOSE_original[] = {0x8B, 0x4D, 0xF4, 0x5F, 0x5E, 0x5B};
inline constexpr Token kS2E_PASS_CLOSE_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_S2E"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kSCENE_OPEN_original[] = {0xA1, 0x2C, 0x41, 0xDE, 0x00};
inline constexpr Token kSCENE_OPEN_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_SCENE_OPEN"}};
inline constexpr uint8_t kSCENE_RESTORE_original[] = {0xE8, 0x5A, 0x66, 0x0D, 0x00};
inline constexpr Token kSCENE_RESTORE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_SCENE_RESTORE"}};
inline constexpr uint8_t kGUARD_SCT_original[] = {0xB8, 0x25, 0x3F, 0xB7, 0x00};
inline constexpr Token kGUARD_SCT_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_GUARD_SCT"}};
inline constexpr uint8_t kGUARD_MB_original[] = {0x8B, 0x0D, 0x7C, 0x44, 0xDE, 0x00};
inline constexpr Token kGUARD_MB_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_GUARD_MB"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kGATE_PALANTIR_original[] = {0xE9, 0x6D, 0x41, 0x1D, 0x00};
inline constexpr Token kGATE_PALANTIR_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "PalGate"}};
inline constexpr uint8_t kGATE_LWM_original[] = {0xE8, 0x74, 0x63, 0x22, 0x00};
inline constexpr Token kGATE_LWM_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "LwmGate"}};
inline constexpr uint8_t kGATE_DISPLAY_VT188_original[] = {0x8B, 0x0D, 0x08, 0x3C, 0xDE, 0x00};
inline constexpr Token kGATE_DISPLAY_VT188_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "Vt188Detour"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kTEL_IGUI_THUNK_original[] = {0xE9, 0x29, 0x35, 0x21, 0x00};
inline constexpr Token kTEL_IGUI_THUNK_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "IguiThunkTel"}};
inline constexpr uint8_t kGATE_CU_PENDING_GAME_original[] = {0xE8, 0x62, 0x8F, 0xFF, 0xFF};
inline constexpr Token kGATE_CU_PENDING_GAME_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_62B385"}};
inline constexpr uint8_t kGATE_CU_APT_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_CU_APT_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "GateVt28_APT"}};
inline constexpr uint8_t kGATE_CU_RADAR_original[] = {0x8B, 0x01, 0x33, 0xFF, 0x89, 0x7D, 0xFC, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_CU_RADAR_replacement[] = {{TokenType::Byte, 0x33, nullptr}, {TokenType::Byte, 0xFF, nullptr}, {TokenType::Byte, 0x89, nullptr}, {TokenType::Byte, 0x7D, nullptr}, {TokenType::Byte, 0xFC, nullptr}, {TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "GateVt28_Radar"}};
inline constexpr uint8_t kGATE_CU_UISEQ_original[] = {0xE8, 0x64, 0xDB, 0x1C, 0x00};
inline constexpr Token kGATE_CU_UISEQ_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_CU_UISEQ"}};
inline constexpr uint8_t kGATE_CU_KBD_DRAIN_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_CU_KBD_DRAIN_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "GateVt28_KbdDrain"}};
inline constexpr uint8_t kGATE_CU_AUDIO_original[] = {0x8B, 0x01, 0x89, 0x35, 0x30, 0x43, 0xDE, 0x00, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_CU_AUDIO_replacement[] = {{TokenType::Byte, 0x89, nullptr}, {TokenType::Byte, 0x35, nullptr}, {TokenType::Byte, 0x30, nullptr}, {TokenType::Byte, 0x43, nullptr}, {TokenType::Byte, 0xDE, nullptr}, {TokenType::Byte, 0x00, nullptr}, {TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "GateVt28_Audio"}};
inline constexpr uint8_t kGATE_GC_LOOKAT_original[] = {0xE8, 0xBA, 0x2F, 0x1F, 0x00};
inline constexpr Token kGATE_GC_LOOKAT_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_LOOKAT"}};
inline constexpr uint8_t kGATE_GC_LWXLAT_original[] = {0xE8, 0xE5, 0x0D, 0x1F, 0x00};
inline constexpr Token kGATE_GC_LWXLAT_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_LWXLAT"}};
inline constexpr uint8_t kGATE_GC_GESTURE_original[] = {0xE8, 0x6F, 0x66, 0xEC, 0xFF};
inline constexpr Token kGATE_GC_GESTURE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_GESTURE"}};
inline constexpr uint8_t kGATE_GC_LWVIEW_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x6C};
inline constexpr Token kGATE_GC_LWVIEW_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_LWVIEW"}};
inline constexpr uint8_t kGATE_GC_LWUI_original[] = {0xE8, 0x69, 0xD2, 0xFF, 0xFF};
inline constexpr Token kGATE_GC_LWUI_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_LWUI"}};
inline constexpr uint8_t kGATE_GC_CLOUD_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_CLOUD_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_CLOUD"}};
inline constexpr uint8_t kGATE_GC_FIRE_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_FIRE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_FIRE"}};
inline constexpr uint8_t kGATE_GC_ANIM2D_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_ANIM2D_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_ANIM2D"}};
inline constexpr uint8_t kGATE_GC_KBDSEL_original[] = {0x8B, 0x0D, 0x34, 0x43, 0xDE, 0x00, 0x85, 0xC9};
inline constexpr Token kGATE_GC_KBDSEL_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Sel_GC_KBD"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kGATE_GC_SKEVA_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_SKEVA_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_SKEVA"}};
inline constexpr uint8_t kGATE_GC_EVA_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_EVA_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_EVA"}};
inline constexpr uint8_t kGATE_GC_MOUSESEL_original[] = {0x8B, 0x0D, 0xE0, 0x36, 0xDE, 0x00, 0x85, 0xC9};
inline constexpr Token kGATE_GC_MOUSESEL_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Sel_GC_MOUSE"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kGATE_GC_POPUPS_original[] = {0xE8, 0x3B, 0xA6, 0x13, 0x00};
inline constexpr Token kGATE_GC_POPUPS_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_POPUPS"}};
inline constexpr uint8_t kGATE_GC_WM_original[] = {0x8B, 0x0D, 0x5C, 0x49, 0xDE, 0x00, 0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_WM_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_WM"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kGATE_GC_DRAWBLK_original[] = {0xA1, 0xF8, 0xF6, 0xD9, 0x00};
inline constexpr Token kGATE_GC_DRAWBLK_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "Cave_GC_DRAWBLK"}};
inline constexpr uint8_t kGATE_GC_PENDGAME2_original[] = {0xE8, 0x69, 0x2B, 0xFE, 0xFF};
inline constexpr Token kGATE_GC_PENDGAME2_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_PENDGAME2"}};
inline constexpr uint8_t kGATE_GC_TERRAIN_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_TERRAIN_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_TERRAIN"}};
inline constexpr uint8_t kGATE_GC_DISPUPD_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_DISPUPD_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_DISPUPD"}};
inline constexpr uint8_t kGATE_GC_DISPUPD_LW_original[] = {0xE8, 0x9A, 0x39, 0x01, 0x00};
inline constexpr Token kGATE_GC_DISPUPD_LW_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_DISPUPD_LW"}};
inline constexpr uint8_t kGATE_GC_DSM_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_DSM_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_DSM"}};
inline constexpr uint8_t kGATE_GC_SHELL_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_SHELL_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_SHELL"}};
inline constexpr uint8_t kGATE_GC_IGUI_original[] = {0x8B, 0x0D, 0x30, 0x48, 0xDE, 0x00, 0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_IGUI_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_IGUI"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kGATE_GC_DISPUPD_INTRO_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x28};
inline constexpr Token kGATE_GC_DISPUPD_INTRO_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "Gate_GC_DISPUPD_INTRO"}};
inline constexpr uint8_t kINT_LIGHT_PHASE_original[] = {0xF3, 0x0F, 0x10, 0x0D, 0x08, 0x19, 0xBD, 0x00};
inline constexpr Token kINT_LIGHT_PHASE_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x10, nullptr}, {TokenType::Byte, 0x0D, nullptr}, {TokenType::Abs32, 0, "g_lightPhaseNum"}};
inline constexpr uint8_t kINT_LIGHT_PULSE_original[] = {0x0F, 0x2F, 0x4F, 0x14, 0x72, 0x3F};
inline constexpr Token kINT_LIGHT_PULSE_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_LIGHT_PULSE"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_LIGHT_PULSE_LTR_original[] = {0xF3, 0x0F, 0x2A, 0x05, 0x08, 0xF6, 0xD9, 0x00};
inline constexpr Token kINT_LIGHT_PULSE_LTR_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x2A, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_lightPulseLtr"}};
inline constexpr uint8_t kINT_ROPE_original[] = {0xF3, 0x0F, 0x10, 0x46, 0x48};
inline constexpr Token kINT_ROPE_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_ROPE"}};
inline constexpr uint8_t kINT_TREAD_original[] = {0x33, 0xFF, 0x39, 0xBB, 0x54, 0x03, 0x00, 0x00};
inline constexpr Token kINT_TREAD_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_TREAD"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_TRUCK_CAB_original[] = {0xF3, 0x0F, 0x59, 0x81, 0xE4, 0x01, 0x00, 0x00, 0xF3, 0x0F, 0x58, 0x00};
inline constexpr Token kINT_TRUCK_CAB_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_TRUCK_CAB"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_TRUCK_TRAILER_original[] = {0xF3, 0x0F, 0x59, 0x88, 0xE4, 0x01, 0x00, 0x00, 0xF3, 0x0F, 0x58, 0x8E, 0x6C, 0x03, 0x00, 0x00};
inline constexpr Token kINT_TRUCK_TRAILER_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_TRUCK_TRAILER"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_TRUCK_WHEEL_original[] = {0x38, 0x9E, 0xEA, 0x02, 0x00, 0x00};
inline constexpr Token kINT_TRUCK_WHEEL_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_TRUCK_WHEEL"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_WAKE_THR_original[] = {0xD9, 0x05, 0xA0, 0xA3, 0xD9, 0x00};
inline constexpr Token kINT_WAKE_THR_replacement[] = {{TokenType::Byte, 0xD9, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_wakeStep"}};
inline constexpr uint8_t kINT_WAKE_ADD_original[] = {0xF3, 0x0F, 0x58, 0x05, 0xA0, 0xA3, 0xD9, 0x00};
inline constexpr Token kINT_WAKE_ADD_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x58, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_wakeStep"}};
inline constexpr uint8_t kINT_WAKE_SUB_original[] = {0xF3, 0x0F, 0x5C, 0x05, 0xA0, 0xA3, 0xD9, 0x00};
inline constexpr Token kINT_WAKE_SUB_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x5C, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_wakeStep"}};
inline constexpr uint8_t kINT_DEBRIS_original[] = {0xFF, 0x46, 0x3C, 0x5F, 0x5B};
inline constexpr Token kINT_DEBRIS_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_DEBRIS"}};
inline constexpr uint8_t kINT_SUBOBJ_DELAY_original[] = {0xF3, 0x0F, 0x5C, 0x4F, 0x10};
inline constexpr Token kINT_SUBOBJ_DELAY_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_SUBOBJ_DELAY"}};
inline constexpr uint8_t kINT_SUBOBJ_ALPHA_original[] = {0xF3, 0x0F, 0x58, 0x4F, 0x08};
inline constexpr Token kINT_SUBOBJ_ALPHA_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_SUBOBJ_ALPHA"}};
inline constexpr uint8_t kINT_OUTLINE_IN_original[] = {0xF3, 0x0F, 0x58, 0x05, 0xFC, 0xC1, 0xBD, 0x00};
inline constexpr Token kINT_OUTLINE_IN_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x58, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_outlineStep"}};
inline constexpr uint8_t kINT_OUTLINE_OUT_original[] = {0xF3, 0x0F, 0x5C, 0x05, 0xFC, 0xC1, 0xBD, 0x00};
inline constexpr Token kINT_OUTLINE_OUT_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x5C, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_outlineStep"}};
inline constexpr uint8_t kINT_LIGHTPULSE_original[] = {0x65, 0xD6, 0x46, 0x00};
inline constexpr Token kINT_LIGHTPULSE_replacement[] = {{TokenType::Abs32, 0, "STUB_LIGHTPULSE"}};
inline constexpr uint8_t kINT_LIGHTPULSE_DBL_original[] = {0x8B, 0x44, 0x24, 0x08, 0x89, 0x81, 0x50, 0x01, 0x00, 0x00, 0x89, 0x81, 0x48, 0x01, 0x00, 0x00, 0x8B, 0x44, 0x24, 0x04, 0x89, 0x81, 0x4C, 0x01, 0x00, 0x00, 0x89, 0x81, 0x54, 0x01, 0x00, 0x00};
inline constexpr Token kINT_LIGHTPULSE_DBL_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_LP_DBL"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_INSTFADE_IN_original[] = {0xF3, 0x0F, 0x58, 0x05, 0xC8, 0x88, 0xBD, 0x00};
inline constexpr Token kINT_INSTFADE_IN_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x58, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_instFadeStep"}};
inline constexpr uint8_t kINT_INSTFADE_OUT_original[] = {0xF3, 0x0F, 0x5C, 0x05, 0xC8, 0x88, 0xBD, 0x00};
inline constexpr Token kINT_INSTFADE_OUT_replacement[] = {{TokenType::Byte, 0xF3, nullptr}, {TokenType::Byte, 0x0F, nullptr}, {TokenType::Byte, 0x5C, nullptr}, {TokenType::Byte, 0x05, nullptr}, {TokenType::Abs32, 0, "g_instFadeStep"}};
inline constexpr uint8_t kINT_OVL_FADE_original[] = {0xF3, 0x0F, 0x10, 0x89, 0x20, 0x01, 0x00, 0x00};
inline constexpr Token kINT_OVL_FADE_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_OVL_FADE"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_SUBT_STATE_original[] = {0xE8, 0x67, 0xFB, 0xFF, 0xFF};
inline constexpr Token kINT_SUBT_STATE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_SUBT_STATE"}};
inline constexpr uint8_t kINT_SUBT_SCROLL_original[] = {0xE8, 0x4C, 0xFF, 0xFF, 0xFF};
inline constexpr Token kINT_SUBT_SCROLL_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_SUBT_SCROLL"}};
inline constexpr uint8_t kSUBT_DELETE_original[] = {0xFF, 0x90, 0x90, 0x01, 0x00, 0x00};
inline constexpr Token kSUBT_DELETE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_SUBT_DELETE"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_UIPART_original[] = {0xF3, 0x0F, 0x10, 0x46, 0x04, 0xF3, 0x0F, 0x58, 0x46, 0x14, 0xF3, 0x0F, 0x11, 0x46, 0x04, 0xF3, 0x0F, 0x10, 0x46, 0x14, 0xF3, 0x0F, 0x59, 0x46, 0x1C, 0xF3, 0x0F, 0x11, 0x46, 0x14, 0xF3, 0x0F, 0x10, 0x46, 0x08, 0xF3, 0x0F, 0x58, 0x46, 0x18, 0xF3, 0x0F, 0x11, 0x46, 0x08, 0xF3, 0x0F, 0x10, 0x46, 0x1C, 0xF3, 0x0F, 0x59, 0x46, 0x18, 0xF3, 0x0F, 0x11, 0x46, 0x18};
inline constexpr Token kINT_UIPART_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_UIPART"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_WANIM_RISE_original[] = {0xF3, 0x0F, 0x2A, 0x05, 0x08, 0xF6, 0xD9, 0x00};
inline constexpr Token kINT_WANIM_RISE_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_WANIM_RISE"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kUI_PBCLOCK_IMAGE_original[] = {0xC6, 0x06, 0x00, 0xE8, 0xDF, 0x8C, 0x4D, 0x00};
inline constexpr Token kUI_PBCLOCK_IMAGE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_PB_CLOCK_KEEP_B"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kUI_PBCLOCK_PLAIN_original[] = {0xC6, 0x06, 0x00, 0xE8, 0x05, 0x86, 0x4D, 0x00};
inline constexpr Token kUI_PBCLOCK_PLAIN_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_PB_CLOCK_KEEP_B_PLAIN"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kUI_PBCLOCK_RADIAL_original[] = {0xC6, 0x06, 0x00, 0xE8, 0x02, 0x7F, 0x4D, 0x00};
inline constexpr Token kUI_PBCLOCK_RADIAL_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_PB_CLOCK_KEEP_B_RADIAL"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kUI_TOOLTIP_LINGER_original[] = {0x8D, 0x86, 0x04, 0x13, 0x00, 0x00, 0xFF, 0x08};
inline constexpr Token kUI_TOOLTIP_LINGER_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_TOOLTIP_LINGER"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kUI_RADAR_REFRESH_B_original[] = {0x8B, 0x0D, 0x88, 0x43, 0xDE, 0x00};
inline constexpr Token kUI_RADAR_REFRESH_B_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_RADAR_REFRESH"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_FXEV_GATE_original[] = {0xE8, 0x4A, 0x56, 0xFF, 0xFF};
inline constexpr Token kINT_FXEV_GATE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_FXEV_GATE"}};
inline constexpr uint8_t kINT_FXEV_PREV_original[] = {0x89, 0x46, 0x04, 0x8B, 0x46, 0x0C};
inline constexpr Token kINT_FXEV_PREV_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_FXEV_PREV"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_ATTMDL_CHK_original[] = {0x83, 0x7E, 0x1C, 0x00, 0x7F, 0x36};
inline constexpr Token kINT_ATTMDL_CHK_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_ATTMDL_CHK"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_ATTMDL_DEC_original[] = {0xFF, 0x4E, 0x1C, 0x83, 0xC6, 0x20};
inline constexpr Token kINT_ATTMDL_DEC_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_ATTMDL_DEC"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_RIDER_SLEW_original[] = {0x8B, 0x44, 0x24, 0x04, 0xF3, 0x0F, 0x10, 0x00};
inline constexpr Token kINT_RIDER_SLEW_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_RIDER_SLEW"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_LASER_TEXCELL_original[] = {0xFF, 0x46, 0x28, 0x8B, 0x4E, 0x18};
inline constexpr Token kINT_LASER_TEXCELL_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_LASER_TEXCELL"}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_SAIL_SLEW_original[] = {0x80, 0xBF, 0xEC, 0x02, 0x00, 0x00, 0x00, 0x74, 0x50};
inline constexpr Token kINT_SAIL_SLEW_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_SAIL_SLEW"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_TURRET_2A_original[] = {0xA1, 0x24, 0x43, 0xDE, 0x00, 0xF3, 0x0F, 0x10, 0x48, 0x3C};
inline constexpr Token kINT_TURRET_2A_replacement[] = {{TokenType::Byte, 0xE9, nullptr}, {TokenType::Rel32, 0, "CAVE_TURRET_FRAC"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_FLOOR_FADE_original[] = {0xF3, 0x0F, 0x58, 0x02, 0x0F, 0x2F, 0xC8};
inline constexpr Token kINT_FLOOR_FADE_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_FLOOR_FADE"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kINT_TERRAIN_TILEUPD_original[] = {0x73, 0x0A, 0x4E, 0x00};
inline constexpr Token kINT_TERRAIN_TILEUPD_replacement[] = {{TokenType::Abs32, 0, "STUB_TERRAIN_OFU"}};
inline constexpr uint8_t kLW_ICON_SNAP_original[] = {0x8B, 0xCE, 0xE8, 0x19, 0xF5, 0xFF, 0xFF};
inline constexpr Token kLW_ICON_SNAP_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_LW_ICON_SNAP"}, {TokenType::Byte, 0x90, nullptr}, {TokenType::Byte, 0x90, nullptr}};
inline constexpr uint8_t kLW_SCENE_PRESENT_original[] = {0x8B, 0x01, 0xFF, 0x50, 0x20};
inline constexpr Token kLW_SCENE_PRESENT_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_LW_SCENE_PRESENT"}};
inline constexpr uint8_t kLW6_CAM_REC_original[] = {0xE8, 0x22, 0xFD, 0xFF, 0xFF};
inline constexpr Token kLW6_CAM_REC_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_LW6_CAM_REC"}};
inline constexpr uint8_t kLW6_CAM_SCENE_END_original[] = {0xE8, 0x6C, 0xC8, 0x07, 0x00};
inline constexpr Token kLW6_CAM_SCENE_END_replacement[] = {{TokenType::Byte, 0xE8, nullptr}, {TokenType::Rel32, 0, "STUB_LW6_CAM_SCENE_END"}};

inline constexpr Site kSites[] = {
    {"C0_PRERENDER", Phase::Phase2a, Kind::CallGate, 0x6325CF, 6, kC0_PRERENDER_original, kC0_PRERENDER_replacement, 3},
    {"HALT", Phase::Phase2a, Kind::CallGate, 0x6325D5, 7, kHALT_original, kHALT_replacement, 4},
    {"ISTICK", Phase::Phase2a, Kind::FuncDetour, 0x63252F, 5, kISTICK_original, kISTICK_replacement, 2},
    {"RESET", Phase::Phase2a, Kind::JmpDetour, 0x44181A, 5, kRESET_original, kRESET_replacement, 2},
    {"C4_KEY_A", Phase::Phase2b, Kind::CallGate, 0x6765D5, 12, kC4_KEY_A_original, kC4_KEY_A_replacement, 9},
    {"C4_KEY_B", Phase::Phase2b, Kind::CallGate, 0x67173B, 11, kC4_KEY_B_original, kC4_KEY_B_replacement, 8},
    {"C4_KEY_C", Phase::Phase2b, Kind::CallGate, 0x671774, 14, kC4_KEY_C_original, kC4_KEY_C_replacement, 11},
    {"TEL_LOGIC_UPDATE", Phase::Telemetry, Kind::PtrSlot, 0xBD85C4, 4, kTEL_LOGIC_UPDATE_original, kTEL_LOGIC_UPDATE_replacement, 1},
    {"TEL_LW_LOGIC_UPDATE", Phase::Telemetry, Kind::PtrSlot, 0xC1459C, 4, kTEL_LW_LOGIC_UPDATE_original, kTEL_LW_LOGIC_UPDATE_replacement, 1},
    {"INT_VF_BW_S1", Phase::Phase2a, Kind::CallGate, 0x4FB599, 6, kINT_VF_BW_S1_original, kINT_VF_BW_S1_replacement, 3},
    {"INT_VF_BW_S2", Phase::Phase2a, Kind::CallGate, 0x4FB5DD, 6, kINT_VF_BW_S2_original, kINT_VF_BW_S2_replacement, 3},
    {"INT_VF_BW_N1", Phase::Phase2a, Kind::CallGate, 0x4FBC7C, 6, kINT_VF_BW_N1_original, kINT_VF_BW_N1_replacement, 3},
    {"INT_VF_BW_N2", Phase::Phase2a, Kind::CallGate, 0x4FBCC0, 6, kINT_VF_BW_N2_original, kINT_VF_BW_N2_replacement, 3},
    {"INT_VF_MASK_IN", Phase::Phase2a, Kind::CallGate, 0x4F5C3A, 6, kINT_VF_MASK_IN_original, kINT_VF_MASK_IN_replacement, 3},
    {"INT_VF_MASK_OUT", Phase::Phase2a, Kind::JmpDetour, 0x4F5C7D, 22, kINT_VF_MASK_OUT_original, kINT_VF_MASK_OUT_replacement, 19},
    {"INT_VF_MONO_1", Phase::Phase2a, Kind::CallGate, 0x4FCBAC, 6, kINT_VF_MONO_1_original, kINT_VF_MONO_1_replacement, 3},
    {"INT_VF_MONO_2", Phase::Phase2a, Kind::CallGate, 0x4FCBF0, 6, kINT_VF_MONO_2_original, kINT_VF_MONO_2_replacement, 3},
    {"INT_OVL_FRAME", Phase::Phase2a, Kind::OperandRedirect, 0x4F9904, 8, kINT_OVL_FRAME_original, kINT_OVL_FRAME_replacement, 5},
    {"INT_OVL_U", Phase::Phase2a, Kind::OperandRedirect, 0x4F9A0D, 8, kINT_OVL_U_original, kINT_OVL_U_replacement, 5},
    {"INT_OVL_V", Phase::Phase2a, Kind::OperandRedirect, 0x4F9A22, 8, kINT_OVL_V_original, kINT_OVL_V_replacement, 5},
    {"INT_OVL_W", Phase::Phase2a, Kind::OperandRedirect, 0x4F9A34, 8, kINT_OVL_W_original, kINT_OVL_W_replacement, 5},
    {"INT_OVL2_U", Phase::Phase2a, Kind::OperandRedirect, 0x4F6C86, 8, kINT_OVL2_U_original, kINT_OVL2_U_replacement, 5},
    {"INT_OVL2_V", Phase::Phase2a, Kind::OperandRedirect, 0x4F6CA7, 8, kINT_OVL2_V_original, kINT_OVL2_V_replacement, 5},
    {"INT_OVL2_W", Phase::Phase2a, Kind::OperandRedirect, 0x4F6CC0, 6, kINT_OVL2_W_original, kINT_OVL2_W_replacement, 3},
    {"INT_OVL3_U", Phase::Phase2a, Kind::OperandRedirect, 0x4F8321, 8, kINT_OVL3_U_original, kINT_OVL3_U_replacement, 5},
    {"INT_OVL3_V", Phase::Phase2a, Kind::OperandRedirect, 0x4F8342, 8, kINT_OVL3_V_original, kINT_OVL3_V_replacement, 5},
    {"INT_OVL3_W", Phase::Phase2a, Kind::OperandRedirect, 0x4F835B, 6, kINT_OVL3_W_original, kINT_OVL3_W_replacement, 3},
    {"INT_OVL2_SCROLL", Phase::Phase2a, Kind::OperandRedirect, 0x4F678D, 8, kINT_OVL2_SCROLL_original, kINT_OVL2_SCROLL_replacement, 5},
    {"INT_OVL3_SCROLL", Phase::Phase2a, Kind::OperandRedirect, 0x4F7EEE, 8, kINT_OVL3_SCROLL_original, kINT_OVL3_SCROLL_replacement, 5},
    {"INT_RIVER_U", Phase::Phase4b, Kind::OperandRedirect, 0x5001F1, 8, kINT_RIVER_U_original, kINT_RIVER_U_replacement, 5},
    {"INT_RIVER_V", Phase::Phase4b, Kind::OperandRedirect, 0x50020C, 8, kINT_RIVER_V_original, kINT_RIVER_V_replacement, 5},
    {"INT_SHORE", Phase::Phase2a, Kind::JmpDetour, 0x4FDB4A, 6, kINT_SHORE_original, kINT_SHORE_replacement, 3},
    {"INT_TREES", Phase::Phase2a, Kind::CallGate, 0x449D55, 5, kINT_TREES_original, kINT_TREES_replacement, 2},
    {"INT_SHRUBS", Phase::Phase2a, Kind::JmpDetour, 0x4E83F0, 6, kINT_SHRUBS_original, kINT_SHRUBS_replacement, 3},
    {"INT_DECAL_SPIRAL", Phase::Phase2a, Kind::JmpDetour, 0x732AA9, 8, kINT_DECAL_SPIRAL_original, kINT_DECAL_SPIRAL_replacement, 5},
    {"INT_MAT_DECAY", Phase::Phase2a, Kind::OperandRedirect, 0x67C4C7, 8, kINT_MAT_DECAY_original, kINT_MAT_DECAY_replacement, 5},
    {"INT_RECOIL", Phase::Phase2a, Kind::CallGate, 0x4C78CE, 5, kINT_RECOIL_original, kINT_RECOIL_replacement, 2},
    {"INT_MB_ENDPAN", Phase::Phase2a, Kind::CallGate, 0x4FD1B4, 7, kINT_MB_ENDPAN_original, kINT_MB_ENDPAN_replacement, 4},
    {"C3_SYNC", Phase::Phase2a, Kind::CallGate, 0x44B911, 14, kC3_SYNC_original, kC3_SYNC_replacement, 11},
    {"S0_FFGUARD", Phase::Phase2a, Kind::JmpDetour, 0x44B8D0, 8, kS0_FFGUARD_original, kS0_FFGUARD_replacement, 5},
    {"C5_PHYS", Phase::Phase2a, Kind::FuncDetour, 0x67BDC0, 5, kC5_PHYS_original, kC5_PHYS_replacement, 2},
    {"C7_PARTMGR_DRAW", Phase::Phase2a, Kind::CallGate, 0x449D40, 11, kC7_PARTMGR_DRAW_original, kC7_PARTMGR_DRAW_replacement, 8},
    {"C7_PARTMGR_TAIL", Phase::Phase2a, Kind::JmpDetour, 0x444CF2, 11, kC7_PARTMGR_TAIL_original, kC7_PARTMGR_TAIL_replacement, 8},
    {"SNOW_WEATHER", Phase::Phase2a, Kind::CallGate, 0x49448D, 5, kSNOW_WEATHER_original, kSNOW_WEATHER_replacement, 2},
    {"SNOW_LIGHTNING", Phase::Phase2a, Kind::CallGate, 0x494494, 5, kSNOW_LIGHTNING_original, kSNOW_LIGHTNING_replacement, 2},
    {"PACER", Phase::Phase2a, Kind::JmpDetour, 0x63A196, 6, kPACER_original, kPACER_replacement, 3},
    {"PRESENT", Phase::Phase2a, Kind::CallGate, 0x522644, 15, kPRESENT_original, kPRESENT_replacement, 12},
    {"GAP_RESET", Phase::Phase2a, Kind::FuncDetour, 0x522000, 6, kGAP_RESET_original, kGAP_RESET_replacement, 3},
    {"LOD_GATE", Phase::Phase2a, Kind::CallGate, 0x44B7B3, 14, kLOD_GATE_original, kLOD_GATE_replacement, 11},
    {"MDRAW", Phase::Phase2a, Kind::JmpDetour, 0x44B95F, 24, kMDRAW_original, kMDRAW_replacement, 21},
    {"S1_VIEWUPDATE_SKIP", Phase::Phase2a, Kind::JmpDetour, 0x48BD1B, 10, kS1_VIEWUPDATE_SKIP_original, kS1_VIEWUPDATE_SKIP_replacement, 7},
    {"S2_PASS_OPEN", Phase::Phase2a, Kind::JmpDetour, 0x48C701, 5, kS2_PASS_OPEN_original, kS2_PASS_OPEN_replacement, 2},
    {"S2E_PASS_CLOSE", Phase::Phase2b, Kind::JmpDetour, 0x48C765, 6, kS2E_PASS_CLOSE_original, kS2E_PASS_CLOSE_replacement, 3},
    {"SCENE_OPEN", Phase::Phase2b, Kind::JmpDetour, 0x449DAB, 5, kSCENE_OPEN_original, kSCENE_OPEN_replacement, 2},
    {"SCENE_RESTORE", Phase::Phase2a, Kind::CallGate, 0x44A271, 5, kSCENE_RESTORE_original, kSCENE_RESTORE_replacement, 2},
    {"GUARD_SCT", Phase::Phase2a, Kind::FuncDetour, 0x48B7B1, 5, kGUARD_SCT_original, kGUARD_SCT_replacement, 2},
    {"GUARD_MB", Phase::Phase2a, Kind::JmpDetour, 0x4FD254, 6, kGUARD_MB_original, kGUARD_MB_replacement, 3},
    {"GATE_PALANTIR", Phase::Phase2a, Kind::JmpDetour, 0x5039C1, 5, kGATE_PALANTIR_original, kGATE_PALANTIR_replacement, 2},
    {"GATE_LWM", Phase::Phase2a, Kind::CallGate, 0x49AAD4, 5, kGATE_LWM_original, kGATE_LWM_replacement, 2},
    {"GATE_DISPLAY_VT188", Phase::Phase2a, Kind::FuncDetour, 0x444CD4, 6, kGATE_DISPLAY_VT188_original, kGATE_DISPLAY_VT188_replacement, 3},
    {"TEL_IGUI_THUNK", Phase::Telemetry, Kind::JmpDetour, 0x48EA1F, 5, kTEL_IGUI_THUNK_original, kTEL_IGUI_THUNK_replacement, 2},
    {"GATE_CU_PENDING_GAME", Phase::Phase2a, Kind::CallGate, 0x63241E, 5, kGATE_CU_PENDING_GAME_original, kGATE_CU_PENDING_GAME_replacement, 2},
    {"GATE_CU_APT", Phase::Phase2a, Kind::CallGate, 0x632449, 5, kGATE_CU_APT_original, kGATE_CU_APT_replacement, 2},
    {"GATE_CU_RADAR", Phase::Phase2a, Kind::CallGate, 0x632486, 10, kGATE_CU_RADAR_original, kGATE_CU_RADAR_replacement, 7},
    {"GATE_CU_UISEQ", Phase::Phase2a, Kind::CallGate, 0x6324A6, 5, kGATE_CU_UISEQ_original, kGATE_CU_UISEQ_replacement, 2},
    {"GATE_CU_KBD_DRAIN", Phase::Phase2a, Kind::CallGate, 0x6324BA, 5, kGATE_CU_KBD_DRAIN_original, kGATE_CU_KBD_DRAIN_replacement, 2},
    {"GATE_CU_AUDIO", Phase::Phase2a, Kind::CallGate, 0x6324F9, 11, kGATE_CU_AUDIO_original, kGATE_CU_AUDIO_replacement, 8},
    {"GATE_GC_LOOKAT", Phase::Phase2a, Kind::CallGate, 0x6484B2, 5, kGATE_GC_LOOKAT_original, kGATE_GC_LOOKAT_replacement, 2},
    {"GATE_GC_LWXLAT", Phase::Phase2a, Kind::CallGate, 0x6484BD, 5, kGATE_GC_LWXLAT_original, kGATE_GC_LWXLAT_replacement, 2},
    {"GATE_GC_GESTURE", Phase::Phase2a, Kind::CallGate, 0x6484C8, 5, kGATE_GC_GESTURE_original, kGATE_GC_GESTURE_replacement, 2},
    {"GATE_GC_LWVIEW", Phase::Phase2a, Kind::CallGate, 0x6484D7, 5, kGATE_GC_LWVIEW_original, kGATE_GC_LWVIEW_replacement, 2},
    {"GATE_GC_LWUI", Phase::Phase2a, Kind::CallGate, 0x6484E2, 5, kGATE_GC_LWUI_original, kGATE_GC_LWUI_replacement, 2},
    {"GATE_GC_CLOUD", Phase::Phase2a, Kind::CallGate, 0x64859E, 5, kGATE_GC_CLOUD_original, kGATE_GC_CLOUD_replacement, 2},
    {"GATE_GC_FIRE", Phase::Phase2a, Kind::CallGate, 0x6485BC, 5, kGATE_GC_FIRE_original, kGATE_GC_FIRE_replacement, 2},
    {"GATE_GC_ANIM2D", Phase::Phase2a, Kind::CallGate, 0x6485C7, 5, kGATE_GC_ANIM2D_original, kGATE_GC_ANIM2D_replacement, 2},
    {"GATE_GC_KBDSEL", Phase::Phase2a, Kind::CallGate, 0x6485CC, 8, kGATE_GC_KBDSEL_original, kGATE_GC_KBDSEL_replacement, 5},
    {"GATE_GC_SKEVA", Phase::Phase2a, Kind::CallGate, 0x6485EC, 5, kGATE_GC_SKEVA_original, kGATE_GC_SKEVA_replacement, 2},
    {"GATE_GC_EVA", Phase::Phase2a, Kind::CallGate, 0x6485F7, 5, kGATE_GC_EVA_original, kGATE_GC_EVA_replacement, 2},
    {"GATE_GC_MOUSESEL", Phase::Phase2a, Kind::CallGate, 0x6485FC, 8, kGATE_GC_MOUSESEL_original, kGATE_GC_MOUSESEL_replacement, 5},
    {"GATE_GC_POPUPS", Phase::Phase2a, Kind::CallGate, 0x648616, 5, kGATE_GC_POPUPS_original, kGATE_GC_POPUPS_replacement, 2},
    {"GATE_GC_WM", Phase::Phase2a, Kind::CallGate, 0x64863A, 11, kGATE_GC_WM_original, kGATE_GC_WM_replacement, 8},
    {"GATE_GC_DRAWBLK", Phase::Phase2a, Kind::JmpDetour, 0x648705, 5, kGATE_GC_DRAWBLK_original, kGATE_GC_DRAWBLK_replacement, 2},
    {"GATE_GC_PENDGAME2", Phase::Phase2a, Kind::CallGate, 0x648817, 5, kGATE_GC_PENDGAME2_original, kGATE_GC_PENDGAME2_replacement, 2},
    {"GATE_GC_TERRAIN", Phase::Phase2a, Kind::CallGate, 0x648833, 5, kGATE_GC_TERRAIN_original, kGATE_GC_TERRAIN_replacement, 2},
    {"GATE_GC_DISPUPD", Phase::Phase2a, Kind::CallGate, 0x64883E, 5, kGATE_GC_DISPUPD_original, kGATE_GC_DISPUPD_replacement, 2},
    {"GATE_GC_DISPUPD_LW", Phase::Phase2a, Kind::CallGate, 0x64884B, 5, kGATE_GC_DISPUPD_LW_original, kGATE_GC_DISPUPD_LW_replacement, 2},
    {"GATE_GC_DSM", Phase::Phase2a, Kind::CallGate, 0x648872, 5, kGATE_GC_DSM_original, kGATE_GC_DSM_replacement, 2},
    {"GATE_GC_SHELL", Phase::Phase2a, Kind::CallGate, 0x648891, 5, kGATE_GC_SHELL_original, kGATE_GC_SHELL_replacement, 2},
    {"GATE_GC_IGUI", Phase::Phase2a, Kind::CallGate, 0x6488A6, 11, kGATE_GC_IGUI_original, kGATE_GC_IGUI_replacement, 8},
    {"GATE_GC_DISPUPD_INTRO", Phase::Phase2a, Kind::CallGate, 0x6488CE, 5, kGATE_GC_DISPUPD_INTRO_original, kGATE_GC_DISPUPD_INTRO_replacement, 2},
    {"INT_LIGHT_PHASE", Phase::Phase2a, Kind::OperandRedirect, 0x4CF2C3, 8, kINT_LIGHT_PHASE_original, kINT_LIGHT_PHASE_replacement, 5},
    {"INT_LIGHT_PULSE", Phase::Phase2a, Kind::JmpDetour, 0x4CF3C9, 6, kINT_LIGHT_PULSE_original, kINT_LIGHT_PULSE_replacement, 3},
    {"INT_LIGHT_PULSE_LTR", Phase::Phase4b, Kind::OperandRedirect, 0x4CF3CF, 8, kINT_LIGHT_PULSE_LTR_original, kINT_LIGHT_PULSE_LTR_replacement, 5},
    {"INT_ROPE", Phase::Phase2a, Kind::JmpDetour, 0x4CA599, 5, kINT_ROPE_original, kINT_ROPE_replacement, 2},
    {"INT_TREAD", Phase::Phase2a, Kind::JmpDetour, 0x4CDE8B, 8, kINT_TREAD_original, kINT_TREAD_replacement, 5},
    {"INT_TRUCK_CAB", Phase::Phase2a, Kind::JmpDetour, 0x4CC1F5, 12, kINT_TRUCK_CAB_original, kINT_TRUCK_CAB_replacement, 9},
    {"INT_TRUCK_TRAILER", Phase::Phase2a, Kind::JmpDetour, 0x4CC313, 16, kINT_TRUCK_TRAILER_original, kINT_TRUCK_TRAILER_replacement, 13},
    {"INT_TRUCK_WHEEL", Phase::Phase2a, Kind::JmpDetour, 0x4CC4B3, 6, kINT_TRUCK_WHEEL_original, kINT_TRUCK_WHEEL_replacement, 3},
    {"INT_WAKE_THR", Phase::Phase2a, Kind::OperandRedirect, 0x4D0593, 6, kINT_WAKE_THR_original, kINT_WAKE_THR_replacement, 3},
    {"INT_WAKE_ADD", Phase::Phase2a, Kind::OperandRedirect, 0x4D05B3, 8, kINT_WAKE_ADD_original, kINT_WAKE_ADD_replacement, 5},
    {"INT_WAKE_SUB", Phase::Phase2a, Kind::OperandRedirect, 0x4D05BD, 8, kINT_WAKE_SUB_original, kINT_WAKE_SUB_replacement, 5},
    {"INT_DEBRIS", Phase::Phase2a, Kind::JmpDetour, 0x4B155C, 5, kINT_DEBRIS_original, kINT_DEBRIS_replacement, 2},
    {"INT_SUBOBJ_DELAY", Phase::Phase2a, Kind::JmpDetour, 0x4B3244, 5, kINT_SUBOBJ_DELAY_original, kINT_SUBOBJ_DELAY_replacement, 2},
    {"INT_SUBOBJ_ALPHA", Phase::Phase2a, Kind::JmpDetour, 0x4B3266, 5, kINT_SUBOBJ_ALPHA_original, kINT_SUBOBJ_ALPHA_replacement, 2},
    {"INT_OUTLINE_IN", Phase::Phase2a, Kind::OperandRedirect, 0xB53A81, 8, kINT_OUTLINE_IN_original, kINT_OUTLINE_IN_replacement, 5},
    {"INT_OUTLINE_OUT", Phase::Phase2a, Kind::OperandRedirect, 0xB53A5C, 8, kINT_OUTLINE_OUT_original, kINT_OUTLINE_OUT_replacement, 5},
    {"INT_LIGHTPULSE", Phase::Phase2a, Kind::PtrSlot, 0xBDC00C, 4, kINT_LIGHTPULSE_original, kINT_LIGHTPULSE_replacement, 1},
    {"INT_LIGHTPULSE_DBL", Phase::Phase4b, Kind::FuncDetour, 0x46D7A9, 32, kINT_LIGHTPULSE_DBL_original, kINT_LIGHTPULSE_DBL_replacement, 29},
    {"INT_INSTFADE_IN", Phase::Phase2a, Kind::OperandRedirect, 0x4D1463, 8, kINT_INSTFADE_IN_original, kINT_INSTFADE_IN_replacement, 5},
    {"INT_INSTFADE_OUT", Phase::Phase2a, Kind::OperandRedirect, 0x4D1436, 8, kINT_INSTFADE_OUT_original, kINT_INSTFADE_OUT_replacement, 5},
    {"INT_OVL_FADE", Phase::Phase2a, Kind::FuncDetour, 0x65CF05, 8, kINT_OVL_FADE_original, kINT_OVL_FADE_replacement, 5},
    {"INT_SUBT_STATE", Phase::Phase2a, Kind::CallGate, 0x66045E, 5, kINT_SUBT_STATE_original, kINT_SUBT_STATE_replacement, 2},
    {"INT_SUBT_SCROLL", Phase::Phase2a, Kind::CallGate, 0x660469, 5, kINT_SUBT_SCROLL_original, kINT_SUBT_SCROLL_replacement, 2},
    {"SUBT_DELETE", Phase::Phase2a, Kind::CallGate, 0x44A1FD, 6, kSUBT_DELETE_original, kSUBT_DELETE_replacement, 3},
    {"INT_UIPART", Phase::Phase2a, Kind::JmpDetour, 0x6A53FB, 60, kINT_UIPART_original, kINT_UIPART_replacement, 57},
    {"INT_WANIM_RISE", Phase::Phase2a, Kind::JmpDetour, 0x69DF0E, 8, kINT_WANIM_RISE_original, kINT_WANIM_RISE_replacement, 5},
    {"UI_PBCLOCK_IMAGE", Phase::Phase2a, Kind::CallGate, 0x4A49B7, 8, kUI_PBCLOCK_IMAGE_original, kUI_PBCLOCK_IMAGE_replacement, 5},
    {"UI_PBCLOCK_PLAIN", Phase::Phase2a, Kind::CallGate, 0x4A5091, 8, kUI_PBCLOCK_PLAIN_original, kUI_PBCLOCK_PLAIN_replacement, 5},
    {"UI_PBCLOCK_RADIAL", Phase::Phase2a, Kind::CallGate, 0x4A5794, 8, kUI_PBCLOCK_RADIAL_original, kUI_PBCLOCK_RADIAL_replacement, 5},
    {"UI_TOOLTIP_LINGER", Phase::Phase2a, Kind::JmpDetour, 0x5EE9A3, 8, kUI_TOOLTIP_LINGER_original, kUI_TOOLTIP_LINGER_replacement, 5},
    {"UI_RADAR_REFRESH_B", Phase::Phase2a, Kind::JmpDetour, 0x450111, 6, kUI_RADAR_REFRESH_B_original, kUI_RADAR_REFRESH_B_replacement, 3},
    {"INT_FXEV_GATE", Phase::Phase2a, Kind::CallGate, 0x4C7819, 5, kINT_FXEV_GATE_original, kINT_FXEV_GATE_replacement, 2},
    {"INT_FXEV_PREV", Phase::Phase2a, Kind::CallGate, 0x4BF74A, 6, kINT_FXEV_PREV_original, kINT_FXEV_PREV_replacement, 3},
    {"INT_ATTMDL_CHK", Phase::Phase2a, Kind::JmpDetour, 0x4C5CF1, 6, kINT_ATTMDL_CHK_original, kINT_ATTMDL_CHK_replacement, 3},
    {"INT_ATTMDL_DEC", Phase::Phase2a, Kind::CallGate, 0x4C5F5B, 6, kINT_ATTMDL_DEC_original, kINT_ATTMDL_DEC_replacement, 3},
    {"INT_RIDER_SLEW", Phase::Phase2a, Kind::FuncDetour, 0x4B2520, 8, kINT_RIDER_SLEW_original, kINT_RIDER_SLEW_replacement, 5},
    {"INT_LASER_TEXCELL", Phase::Phase2a, Kind::CallGate, 0x4C90DD, 6, kINT_LASER_TEXCELL_original, kINT_LASER_TEXCELL_replacement, 3},
    {"INT_SAIL_SLEW", Phase::Phase2a, Kind::JmpDetour, 0x4D020C, 9, kINT_SAIL_SLEW_original, kINT_SAIL_SLEW_replacement, 6},
    {"INT_TURRET_2A", Phase::Phase2a, Kind::JmpDetour, 0x4B6F81, 10, kINT_TURRET_2A_original, kINT_TURRET_2A_replacement, 7},
    {"INT_FLOOR_FADE", Phase::Phase2a, Kind::CallGate, 0x4E3F08, 7, kINT_FLOOR_FADE_original, kINT_FLOOR_FADE_replacement, 4},
    {"INT_TERRAIN_TILEUPD", Phase::Phase2a, Kind::PtrSlot, 0xBE4794, 4, kINT_TERRAIN_TILEUPD_original, kINT_TERRAIN_TILEUPD_replacement, 1},
    {"LW_ICON_SNAP", Phase::Phase6, Kind::CallGate, 0x6C0E6B, 7, kLW_ICON_SNAP_original, kLW_ICON_SNAP_replacement, 4},
    {"LW_SCENE_PRESENT", Phase::Phase6, Kind::CallGate, 0x449F43, 5, kLW_SCENE_PRESENT_original, kLW_SCENE_PRESENT_replacement, 2},
    {"LW6_CAM_REC", Phase::Phase6, Kind::CallGate, 0x49B77E, 5, kLW6_CAM_REC_original, kLW6_CAM_REC_replacement, 2},
    {"LW6_CAM_SCENE_END", Phase::Phase6, Kind::CallGate, 0x49B78F, 5, kLW6_CAM_SCENE_END_original, kLW6_CAM_SCENE_END_replacement, 2},
};

inline constexpr uint32_t kSiteCount = 135;

// Index of each site in kSites (telemetry counters are indexed the same way).
enum Index : uint32_t {
    C0_PRERENDER = 0,
    HALT = 1,
    ISTICK = 2,
    RESET = 3,
    C4_KEY_A = 4,
    C4_KEY_B = 5,
    C4_KEY_C = 6,
    TEL_LOGIC_UPDATE = 7,
    TEL_LW_LOGIC_UPDATE = 8,
    INT_VF_BW_S1 = 9,
    INT_VF_BW_S2 = 10,
    INT_VF_BW_N1 = 11,
    INT_VF_BW_N2 = 12,
    INT_VF_MASK_IN = 13,
    INT_VF_MASK_OUT = 14,
    INT_VF_MONO_1 = 15,
    INT_VF_MONO_2 = 16,
    INT_OVL_FRAME = 17,
    INT_OVL_U = 18,
    INT_OVL_V = 19,
    INT_OVL_W = 20,
    INT_OVL2_U = 21,
    INT_OVL2_V = 22,
    INT_OVL2_W = 23,
    INT_OVL3_U = 24,
    INT_OVL3_V = 25,
    INT_OVL3_W = 26,
    INT_OVL2_SCROLL = 27,
    INT_OVL3_SCROLL = 28,
    INT_RIVER_U = 29,
    INT_RIVER_V = 30,
    INT_SHORE = 31,
    INT_TREES = 32,
    INT_SHRUBS = 33,
    INT_DECAL_SPIRAL = 34,
    INT_MAT_DECAY = 35,
    INT_RECOIL = 36,
    INT_MB_ENDPAN = 37,
    C3_SYNC = 38,
    S0_FFGUARD = 39,
    C5_PHYS = 40,
    C7_PARTMGR_DRAW = 41,
    C7_PARTMGR_TAIL = 42,
    SNOW_WEATHER = 43,
    SNOW_LIGHTNING = 44,
    PACER = 45,
    PRESENT = 46,
    GAP_RESET = 47,
    LOD_GATE = 48,
    MDRAW = 49,
    S1_VIEWUPDATE_SKIP = 50,
    S2_PASS_OPEN = 51,
    S2E_PASS_CLOSE = 52,
    SCENE_OPEN = 53,
    SCENE_RESTORE = 54,
    GUARD_SCT = 55,
    GUARD_MB = 56,
    GATE_PALANTIR = 57,
    GATE_LWM = 58,
    GATE_DISPLAY_VT188 = 59,
    TEL_IGUI_THUNK = 60,
    GATE_CU_PENDING_GAME = 61,
    GATE_CU_APT = 62,
    GATE_CU_RADAR = 63,
    GATE_CU_UISEQ = 64,
    GATE_CU_KBD_DRAIN = 65,
    GATE_CU_AUDIO = 66,
    GATE_GC_LOOKAT = 67,
    GATE_GC_LWXLAT = 68,
    GATE_GC_GESTURE = 69,
    GATE_GC_LWVIEW = 70,
    GATE_GC_LWUI = 71,
    GATE_GC_CLOUD = 72,
    GATE_GC_FIRE = 73,
    GATE_GC_ANIM2D = 74,
    GATE_GC_KBDSEL = 75,
    GATE_GC_SKEVA = 76,
    GATE_GC_EVA = 77,
    GATE_GC_MOUSESEL = 78,
    GATE_GC_POPUPS = 79,
    GATE_GC_WM = 80,
    GATE_GC_DRAWBLK = 81,
    GATE_GC_PENDGAME2 = 82,
    GATE_GC_TERRAIN = 83,
    GATE_GC_DISPUPD = 84,
    GATE_GC_DISPUPD_LW = 85,
    GATE_GC_DSM = 86,
    GATE_GC_SHELL = 87,
    GATE_GC_IGUI = 88,
    GATE_GC_DISPUPD_INTRO = 89,
    INT_LIGHT_PHASE = 90,
    INT_LIGHT_PULSE = 91,
    INT_LIGHT_PULSE_LTR = 92,
    INT_ROPE = 93,
    INT_TREAD = 94,
    INT_TRUCK_CAB = 95,
    INT_TRUCK_TRAILER = 96,
    INT_TRUCK_WHEEL = 97,
    INT_WAKE_THR = 98,
    INT_WAKE_ADD = 99,
    INT_WAKE_SUB = 100,
    INT_DEBRIS = 101,
    INT_SUBOBJ_DELAY = 102,
    INT_SUBOBJ_ALPHA = 103,
    INT_OUTLINE_IN = 104,
    INT_OUTLINE_OUT = 105,
    INT_LIGHTPULSE = 106,
    INT_LIGHTPULSE_DBL = 107,
    INT_INSTFADE_IN = 108,
    INT_INSTFADE_OUT = 109,
    INT_OVL_FADE = 110,
    INT_SUBT_STATE = 111,
    INT_SUBT_SCROLL = 112,
    SUBT_DELETE = 113,
    INT_UIPART = 114,
    INT_WANIM_RISE = 115,
    UI_PBCLOCK_IMAGE = 116,
    UI_PBCLOCK_PLAIN = 117,
    UI_PBCLOCK_RADIAL = 118,
    UI_TOOLTIP_LINGER = 119,
    UI_RADAR_REFRESH_B = 120,
    INT_FXEV_GATE = 121,
    INT_FXEV_PREV = 122,
    INT_ATTMDL_CHK = 123,
    INT_ATTMDL_DEC = 124,
    INT_RIDER_SLEW = 125,
    INT_LASER_TEXCELL = 126,
    INT_SAIL_SLEW = 127,
    INT_TURRET_2A = 128,
    INT_FLOOR_FADE = 129,
    INT_TERRAIN_TILEUPD = 130,
    LW_ICON_SNAP = 131,
    LW_SCENE_PRESENT = 132,
    LW6_CAM_REC = 133,
    LW6_CAM_SCENE_END = 134,
};

} // namespace sites
