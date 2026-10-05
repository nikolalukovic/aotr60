**AUDIT 3: Dynamic LOD neutrality.** Under AotR's data the LOD level cannot change logic, in 30 mode or 60 mode. All bytes and addresses below were checked against game.dat with pefile and capstone, and no files were written.

**1. Where AotR's DynamicGameLOD values come from**
- The engine reads one fixed file, `Data\INI\GameLOD.ini` (string 0xBF995C, loader 0x602165).
- I scanned 240 .big archives (BIGF/BIG4) under aotr, rotwk and bfme2: 1875 .ini/.inc entries. The block appears only in `data\ini\gamelod.ini`, in four copies:
  - the loose `<game folder>\aotr\data\ini\gamelod.ini`
  - `rotwk\#aotr_patch202.big`
  - `rotwk\INI.big`
  - `bfme2\ini.big`
- No other loose INI under aotr\data has a DynamicGameLOD block.
- All four copies have the same five blocks. Neither MinParticlePriority nor MinParticleSkipPriority appears in them (both are StaticGameLOD fields):

| Level (index) | MinimumFPS | ParticleSkipMask | DebrisSkipMask | SlowDeathScale |
|---|---|---|---|---|
| VeryHigh (4) | 25 | 0 | 0 | 1.0 |
| High (3) | 20 | 0 | 0 | 1.0 |
| Medium (2) | 10 | 1 | 0 | 1.0 |
| Low (1) | 0 | 3 | 0 | 1.0 |
| VeryLow (0) | 0 | 3 | 0 | 1.0 |

**2. Parse table in game.dat**
- The parser is 0x602880. It writes into `[0xDE3B84]+0x1C8+16*idx`. The level names are at 0xD9E74C: {VeryLow, Low, Medium, High, VeryHigh} = 0..4.
- Entries are {token, parser, user, offset}, table at 0xBF95D8:
  - MinimumFPS: int parser 0x42EC5E, offset +0
  - ParticleSkipMask: int parser, +4
  - DebrisSkipMask: int parser, +8
  - SlowDeathScale: float parser 0x42ED00, +0xC
- The table has no NULL terminator. The lookup at 0x42B890 stops at the first NULL token, so the AudioLOD entries at 0xBF9618 are also accepted inside DynamicGameLOD: MaximumAmbientStreams (+0), AllowDolby (+4, byte), AllowReverb (+5, byte). The terminator is at 0xBF9648. AotR does not use these tokens there.
- Each level's defaults (constructor 0x6017A4) are 0, 0, 0, 1.0f.
- The manager constructor sets the current values: level +0x176C = 3, +0x179C = 1.0f, and +0x178C/+0x1790/+0x1794/+0x1798 = 0 (writes at 0x6018B1..0x6018FA).

**3. Selection and apply chain (render side only)**
- W3DDisplay::draw runs `0x44B7B3 8BCF / 0x44B7B5 E8 0179FFFF call 0x4430BB / 0x44B7BA 8BCF / 0x44B7BC E8 1981FFFF call 0x4438DA`.
- 0x4430BB, the FPS average:
  - it takes the QPC delta since its previous call, and the last time [0xDC7690] is updated on every call;
  - a sample with delta > 0.5 s is dropped (the double at 0xBD86A0 is 0.5);
  - samples go into a 30-entry ring at 0xDC7590, and the average goes to Display+0x180.
- 0x4438DA:
  - if `[GD+0x54] && [GL(0xDE412C)+0x9B]`, it calls `findDynamicLODLevel(Display+0x180)` (0x601F94); otherwise it uses level 4;
  - it then calls `setDynamicLODLevel` (0x6020E7), which does nothing when the level is unchanged and otherwise calls `apply` (0x601FB4);
  - its tail runs terrain auto-LOD 0x4436DD when GD+0x50 == 8.
- 0x601F94 truncates the average with `cvttss2si` and returns the highest i (4..0) with `MinimumFPS[i] < int(avg)`, else 0. With AotR's values:

| Average FPS | Level |
|---|---|
| 26 or more | VeryHigh |
| 21–25 | High |
| 11–20 | Medium |
| 1–10 | Low |
| below 1 | VeryLow |

- 0x601FB4 is the only reader of the per-level array apart from 0x601F94 (which reads only MinimumFPS). It zeroes the counters +0x178C and +0x1794 and copies ParticleSkipMask to +0x1790, DebrisSkipMask to +0x1798 (0x601FE3) and SlowDeathScale to +0x179C (0x601FEF).
- The only writers of +0x1798 and +0x179C are the constructor and 0x601FB4. The current level +0x176C is read only at 0x6020F0.
- No save or CRC code reads offsets 0x176C..0x179C. The other hits for those offsets (0x7D86C8, 0xAAxxxx/0xABxxxx) belong to other objects.
- Display+0x180 has one other reader: its getter 0x4438AE (slot +0x170 of W3DDisplay's vtable 0xBD9C28), called only at 0x819E9A in the stats-file writer 0x819E12. No network run-ahead code reads it: GD+0xC08 (NetworkFPSHistoryLength) and GD+0xC10 (NetworkRunAheadMetricsTime) are written only by the GlobalData constructor (0x6432FF / 0x643313).

**4. Who reads the current values**

| Value | Reader | Code | Side | What it does |
|---|---|---|---|---|
| ParticleSkipMask (+0x1790) | 0x5FC414 | `8B8990170000` | Client | ParticleSystem createParticle (0x5FC39A). Only priorities in [StaticLOD +0x17A0, +0x17A4) are subject to the skip. Visual only. |
| DebrisSkipMask (+0x1798) | 0x5F1295 | `8B8998170000; add eax,0x1794; inc [eax]; and edx,[eax]; cmp edx,ecx; jne 0x5F18AB` | **Logic** | OCL generic nugget 0x5F0EE6 (ObjectCreationList.cpp; it picks templates with the logic RNG 0x6D328E at 0x5F11CD). If `(++cnt & mask) != mask`, the object is not created. |
| SlowDeathScale (+0x179C) | 0x860B62 | `F30F10889C170000` | **Logic** | SlowDeathBehavior update 0x860B39. If scale ≠ 1.0, the object's stored scale +0x28 is 1.0 and flag 0x18C&6 is clear: scale 0 destroys the object (0x62BBAB); any other scale rescales its pending death timers. |
| SlowDeathScale (+0x179C) | 0x860F7B | `F30F10889C170000` | **Logic** | beginSlowDeath 0x860E93. Scale 0.0 (0xC1B594) destroys the object at once; otherwise the death delays are multiplied by the scale (FMUL at 0x861023, 0x861058, 0x861073, 0x861107). |
| MinimumFPS | 0x601F94 only | | Render | Level selection only. |

**5. Conclusion: neutral with AotR's data**
- Every level, and the default, has DebrisSkipMask 0 and SlowDeathScale 1.0. So `(cnt & 0) == 0` never skips an object; SlowDeath always takes the scale == 1.0 path (no rescale, no instant destroy, and multiplying by 1.0 changes nothing); and the counter resets in 0x601FB4 have no effect.
- The level only changes client particle shedding (Medium/Low/VeryLow) and the terrain auto-LOD tail.
- Stock multiplayer already depends on this neutrality, because the level is chosen from each machine's own FPS.

**6. What the 60-mode LOD sampling rule must guarantee**
- **Phase 0 assert.** Check the parsed values in memory, not the INI text, because of the missing terminator and the possible override files. After GameLOD.ini has loaded ([0xDE3B84] != 0), for i = 0..4:
  - `dword[m+0x1D0+16i] == 0`
  - `dword[m+0x1D4+16i] == 0x3F800000`
  - also `dword[m+0x1798] == 0` and `dword[m+0x179C] == 0x3F800000`

  If any check fails, stay in 30 mode.
- **Logic equivalence**, which matters only if those values ever stop being neutral: the level that logic step s reads must be set only by the A-render, from the A→A interval. That interval equals stock's render interval, including while paused or frozen. B-renders must not change the level.
- **Visual parity.** On B-renders skip both 0x4430BB and 0x4438DA. Then [0xDC7690] measures A→A, the 0.5 s guard behaves as in stock, and the thresholds 25/20/10 apply to the tick rate. 0x4438DA, including the 0x4436DD tail, then runs 30 times a second.
  - If every render is sampled instead, the average is about 2× the tick rate, so High/Medium only kick in once the game is at roughly 42% / 33% speed, instead of 83% / 67% in stock.
- **Proposed hook.** call_gate on span 0x44B7B3..0x44B7C1 (14 bytes, `8BCF E80179FFFF 8BCF E81981FFFF`), replaced with `E8 <rel32:LOD_STUB>` followed by 9 × 90.
  - The stub: if `g_m60 == 0 || g_uiTick`, set `ecx = edi`, call 0x4430BB, set `ecx = edi`, call 0x4438DA; otherwise do nothing.
  - Neither callee takes stack arguments.
  - Live after the span: EDI, EBX, ESI, EBP and a balanced stack. EAX, ECX, EDX and the flags are dead, because the next instruction (0x44B7C1) reloads EAX and ECX is reloaded before use. The x87 stack is empty.
  - The only branch into the region is `0x44B7A2 jz 0x44B7B3`, which targets the span start, so it is allowed. The span does not overlap any AotR hook.