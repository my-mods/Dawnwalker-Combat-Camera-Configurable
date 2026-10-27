# Building Combat Camera - Configurable

Use Windows x64, Visual Studio 2022 C++ build tools (MSVC 19.44), Windows SDK 10.0.26100.0, CMake 3.25 or newer, Git and Ninja. Start an **x64 Native Tools Command Prompt**. MASM is included with the C++ build tools.

Clone this repository and prepare the exact UE4SS SDK revision:

```bat
git clone https://github.com/my-mods/Dawnwalker-Combat-Camera-Configurable.git
git clone https://github.com/UE4SS-RE/RE-UE4SS.git ue4ss-sdk
git -C ue4ss-sdk checkout 97b7e501c19d8b2b7c662feee73aaa0dc1f0a4d1
git -C ue4ss-sdk config submodule.deps/first/Unreal.url https://github.com/UE4SS-RE/UEPseudo.git
git -C ue4ss-sdk submodule update --init deps/first/Unreal
```

The Unreal headers must resolve to `eb40a05f49509bdeb1ac39287032b60af585cca8`. CMake checks both revisions. It fetches the pinned MinHook, Zydis decoder and SDK header dependencies declared in `native/CMakeLists.txt`.

```bat
cd Dawnwalker-Combat-Camera-Configurable
cmake -S native -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUE4SS_SDK=../ue4ss-sdk
cmake --build build
```

The result is `build/main.dll`. The mod uses `/MD`, C++23 and reproducible compilation/link options. `Framecore2b.def` supplies the exact imported host symbols; a complete UE4SS rebuild is unnecessary.

## Runtime payload

To construct an archive, place these files below `Data/CombatCamera/`:

| Destination | Source |
| --- | --- |
| `dlls/main.dll` | `build/main.dll` |
| `Scripts/main.lua` | `Scripts/main.lua` |
| `Scripts/Settings.lua` | `Scripts/Settings.lua` |
| `Scripts/dmm_api.lua` | `Scripts/dmm_api.lua` |
| `Scripts/ModLogLevels.lua` | `Scripts/ModLogLevels.lua` |
| `Scripts/ModDmmApi.lua` | `Scripts/ModDmmApi.lua` |
| `mod_settings.ini` | `mod_settings.ini` |
| `enabled.txt` | `package/enabled.txt` |

Keep `package/mod.manifest` at archive root as `mod.manifest`, and include `README.md` as `README.txt`, the license, changelog, third-party notices and `LICENSES`. Include the supplied `package/vortex_override_instructions.json` at the root so Vortex retains the documentation as metadata. Use the stable filename `Combat-Camera-Configurable.zip`. Personal `settings.ini` is generated at startup and must not be added to the archive.

## Native contracts

This maintenance build targets Dawnwalker **1.1 / Steam 25915545**. Version **3.6.0** remains the older-game release. Keep its archive and source tag when updating the integration.

| Analysis reference | SHA-256 |
| --- | --- |
| Dawnwalker.exe, Steam 25915545 | `e6222b815b1c376f2c85a2189120c5a9b5cd30ac5688a34b61a2c18efbc9b117` |
| Framecore 2b UE4SS.dll import reference | `fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a` |

These hashes record analysis provenance. The mod does not hash the game or loader at runtime, identify storefronts, or restrict loader fork names.

`GameBuild.hpp` holds verified game-relative bindings and fifteen exact assembly instruction windows. `GameLayout.hpp` centralizes accessed fields and virtual slots. `GameCode.hpp` defines operation contracts for core camera/targeting, unlocked abilities, the spell wrapper, death events, focus-view protection, scripted effects, controller slowdown and the center dot. `CodeCompatibility.hpp` uses Zydis to check required operands, argument ownership, field widths, indexed addressing, relative callees, instruction relationships, reflected properties and virtual slots. Unwind metadata bounds each function; explicitly bounded leaves are checked separately.

There are no whole-function hashes or broad prologue signatures. Unrelated body instructions and equivalent encodings outside required operations do not invalidate a feature. Required return addresses come from matched call sequences, allowing movement within the verified caller. Entry addresses and assembly gate locations remain patch-specific: this is not automatic discovery for unknown updates. Exact overwritten instructions remain necessary because assembly gates rely on their live registers, stack alignment, flags and continuation.

A failed core contract prevents hook installation. Optional contracts fail independently and retain supported features. Named combat-start/end registrations resolve parameterless callbacks through RCX-preserving wrappers. Unexpected player lock-input expressions disable targeting and camera overrides for the session and restore native input; the independent center dot remains available. Diagnostics obey the chosen Logging level.

### Verified layout changes

| Used field | 1.0.5 | 1.1 |
| --- | --- | --- |
| Player combat component | `0xc90` | `0xca0` |
| Player camera component | `0xc58` | `0xc68` |
| Selected combat target | `0x1380` | `0x13d0` |
| Camera detached flag | `0x137a` | `0x13ca` |
| Combat state | `0xb28` | `0xb40` |
| Hard lock flag | `0x14a9` | `0x14f9` |
| Combat configuration | `0x9b8` | `0x9d0` |
| Ability focus weak target | `0x230` | `0x238` |
| Ability focus mode | `0x270` | `0x278` |

Offsets changed unevenly. Verify consumers together; do not apply a blanket delta. Registrations, callers, property metadata and accessed slots establish identities where relocation candidates are ambiguous. The attachment wrapper starts at `0x5ff9c8c`; its intercepted store is `0x5ff9c99`. The attack target setter is `0x6044fc8`, settings query is `0x17a7f70`, and target-death Execute is `0x6036ae0`.

### Input and settings

`LockInput.hpp` validates expressions relative to the current FFrame cursor, including the owning player graph/package and called native function owners. The cooked 1.1 graph has 12,194 loaded bytes, but that length and its absolute call positions are not runtime requirements. Toggle, next-target and previous-target expressions are distinguished. Native thunks still decode their own arguments and preserve the switch result.

`_CCSetCombatV3` consumes seventeen arguments in `Settings.order`. The original nine positions retain the ignored legacy freeCamera slot; cameraMode, trackingSpeed, trackingResumeMs, lockLastAttacker, autoLockOnHit, afterTargetDeath, trackingCatchup and combatStartLock follow. The host consumes each integer at stack index 1. Sixteen visible controls retain General, Camera, Targeting, Controller and Diagnostics grouping, with Logging last. Schema, defaults, persisted choices and event-driven Apply delivery are unchanged.

### Behavior and lifetime

Free, Smooth and Native modes retain their policies. Native uses patch 1.1's normal tracking. Smooth changes only pitch/yaw input before native modifiers and limits; manual input pauses it, the engagement ramp and catch-up remain, and each eligible update uses at most one target aim-point query and two camera getters. Collision and positioning remain native.

Requests share the existing 50 ms budget and one full selection pass per interval; automatic fallback waits for another interval. Same-enemy disappearance recovery uses the existing tick at most every 100 gameplay milliseconds. Combat-start acquisition, hit priority and death handoff retain bounded readiness and cancellation. No global object discovery, settings polling, recurring worker or new search budget is added.

Nine attachment stores remain across direct, Blueprint, casting, ability, threat, combat, hard-lock and selection paths. Gates skip only the controlled player's attachment write when the policy requires detachment. Native target changes and notifications continue. Scripted actions and effect-camera blends retain native control until the component-owned camera stack finishes its handoff.

The ability-target getter still falls back to its owner when no target exists. Unlocked planning and instant execution therefore retain validated enemy selection and self-target rejection. Accepted self-centered abilities need no enemy. Native costs, cooldowns and eligibility remain.

Persistent references use indexed identities, existing serials and deletion notifications. Borrowed event/VM structures remain scoped to callbacks. Owner replacement, unlock, settings changes and shutdown revoke pending work; no weak serial is allocated. Camera worker callbacks consume only the published opaque owner identity. Object-array shutdown unregisters its listener once; hook teardown is separate and idempotent.

### Shared hit hook

Combat Camera detours PlayerCombatComponent::ReactToHit at `0x6042f18`, preserving `void(combat, animation, attack, response)`. Attack `+0x28` owns the attacker's combat component. Easier Parry owns virtual slot `+0x8e8` and forwards this native entry. Combat Camera neither owns nor requires the current contents of that shared slot. Both installation orders preserve one native reaction call, its output and queued attacker acquisition.

### Validation boundaries

Production validators are tested against the current relocated executable, equivalent instructions, a moved call, shared entry interception, and independently broken arguments, fields and slots. Optional failures are isolated. Native regressions use fresh 1.1 instruction bodies, production C++ and MASM, real MinHook trampolines and isolated engine fixtures. Current cooked lock expressions are tested with moved positions, unrelated edits, corrupt bytes and incorrect ownership.

Source and final-ZIP startup/menu tests cover fresh, previous-release and disposable current-user preferences, Apply, reopen/restart, exactly-once keys, invalid input and failed writes. Builds must be reproducible, expose only start_mod/uninstall_mod, satisfy host imports, include decoder licenses, pass Vortex routing and match the Latest Archives pickup hash.

Offline tests do not establish camera feel, live event delivery or FPS. Gameplay acceptance separately covers target switching, disappearance recovery, Voracious Bite transitions and unlocked spells on patch 1.1. Installation and deployment remain user-managed through Vortex.
