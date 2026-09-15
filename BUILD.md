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

The Unreal headers must resolve to `eb40a05f49509bdeb1ac39287032b60af585cca8`. CMake checks both revisions. It fetches the pinned MinHook and SDK header dependencies declared in `native/CMakeLists.txt`.

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
| `mod_settings.ini` | `mod_settings.ini` |
| `enabled.txt` | `package/enabled.txt` |

Keep `package/mod.manifest` at archive root as `mod.manifest`, and include `README.md` as `README.txt`, the license, changelog, third-party notices and `LICENSES`. Include the supplied `package/vortex_override_instructions.json` at the root so Vortex retains the documentation as metadata. Use the stable filename `Combat-Camera-Configurable.zip`. Personal `settings.ini` is generated at startup and must not be added to the archive.

## Native bindings

The object-array shutdown callback disables native activity, invalidates watched identities and unregisters its deletion listener before returning. Hook teardown remains in the separate stop path; repeated cleanup does not remove the listener twice or access dying game objects.

The native settings callback consumes twelve Lua arguments in `Settings.order`. The original nine values retain their order, including the ignored legacy `freeCamera` slot; `cameraMode`, `trackingSpeed` and `trackingResumeMs` are appended. The menu exposes eleven controls. The pinned host's `get_integer` removes the value it reads, so each successive setting is read at stack index 1. This applies to both saved startup settings and Mod Setting Menu Apply.

Before installing hooks, the mod validates 33 native code ranges (19,910 bytes), three camera virtual-table entries and 36 patch/call-site guards in the loaded game image. Required ranges must be readable, executable where appropriate, and belong to the game allocation. Function checks cover the complete recorded unwind ranges and four explicitly bounded leaf functions, including internal instructions beyond their prologues. These ranges contain no base relocations; virtual-table targets are compared relative to the actual load address. No game executable file is opened or hashed. The reference used to establish these contracts is:

| Input | SHA-256 |
| --- | --- |
| Dawnwalker.exe, Steam build 25232147 | `cb9b7d7bd88a6754c0a9c08318aa64d5013ddfd92d5badcae84e1b4ea980dcfc` |
| Framecore 2b UE4SS.dll | `fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a` |

`GameBuild.hpp` contains game-relative addresses and instruction guards; `GameCode.hpp` records the scoped code and virtual-table contracts. Unrelated executable metadata, resources or other file differences do not disable the mod. Changed required code, moved addresses or conflicting native patches still stop startup with the failing RVA (address relative to the game image). This is not an automatic port to changed game layouts. Updating the contracts requires checking function signatures, call-site relationships, UObject member offsets, virtual method slots and assembly live-register requirements. A matching prologue alone is insufficient. Keep the reference executable hash as analysis provenance, not a runtime allowlist.

The separate Framecore 2b library fingerprint is retained because the imported UE4SS C++ API and internal types are built against its pinned ABI. Removing the game-file check does not certify another UE4SS library.

Smooth tracking recognizes only the native temporary-loss route: `LoseTargetLock` at RVA `0x5e16ff4`, its target-clear return at `0x5e17071`, and `RegainTargetLock`'s switch return at `0x5e1ccf6`. The native clear, delegates and recovery timer still run. The game-thread bridge remembers the component/actor identities until that event, cancellation or expiry (native duration plus one second of gameplay time, including time dilation; only finite durations above zero and up to 60 seconds qualify). It never tracks an invisible actor or polls for one. The regain call validates only the remembered actor from the game's current in-range candidate list; context byte `+0x61` disables the angular rejection for that attempt, while native candidate predicates and visibility checks remain intact. No other acquisition widens its cone. Free and Native do not arm this recovery. Deletion, owner replacement, unlock, mode changes and invalid gameplay contexts cancel it. Reacquisition restarts the smooth ramp at the current view and retains the manual-input resume delay.

The camera, cone and untargeted-direction gates preserve volatile registers, SIMD values and the required comparison flags. The camera gate changes the native free-camera flag and selected camera target for the local player in Free/Smooth modes and while unlocked. Native tracking releases this suppression while a selected target is locked. When target lock is off, the direction gate replaces the computed planar XY values in XMM7/XMM8 with the current camera yaw, bypassing both persistent and temporary action-target facing. It uses 32 bytes of dedicated aligned scratch space, separate from saved volatile registers. The native output stores retain zero Z, and the native epilogue restores the caller's nonvolatile SIMD registers. Missing or nonfinite camera data retains the already computed untargeted character direction; locked modes follow the original trampoline without querying camera rotation. The cone gate modifies only the scoped native threshold comparison. The game still performs candidate scoring, combat predicates and occlusion checks.

Camera heading is queried on the gameplay thread only when the native combat-direction function requests an unlocked direction. It uses the validated player's camera manager rotation getter at vtable slot `0x820`, without a new timer, target search or actor-rotation setter. Pitch is excluded, including at vertical camera angles. Optional logging aggregates successful resolutions, fallbacks and rotation-resolution microseconds; this timing excludes the preceding ownership validation and does not measure total mod or frame time.

Automatic requests use one full native picker pass per 50 ms budget interval. When the primary pass finds nothing, the game's fallback predicate runs in the following interval; a one-element fallback check can retain the current target in the meantime. An explicit target-lock press retains immediate fallback. A pending switch also validates the current target through a one-element candidate list. Controller assistance is limited to camera-directed targeting. It computes only the current target's angle, refreshes at most every 50 ms, expires after 150 ms and requires the game's controller sensitivity branch with no mouse input.

The lock choice is owned by the local player component and survives settings Apply. Two native Blueprint thunks identify the target-lock action at the checked player graph call sites, retaining the game's existing controller and keyboard bindings. Their parameter decoding remains native. An unexpected player graph length or lock-action call site disables the mod for that session and logs the reason, allowing native input to resume. SwitchLockTarget preserves its boolean return type. Other automatic, look-axis and next/previous target requests cannot replace a fixed target or create an unlocked target. Native target clearing still performs its delegate and notification cleanup. Temporary ability locks cannot enable player targeting.

Native hard lock remains available to combat rules. Nine guarded native attachment stores are intercepted individually, including the direct setter, Blueprint actions, casting, abilities, threat requests, combat transitions, hard lock and target selection. Only the camera-attachment store is skipped for the controlled player when the camera policy requires detachment; the surrounding native target changes, spell behavior and notifications continue. Constructors and native detachment stores are unchanged. An already attached camera uses the native detach transition once when a free/smooth combat context is acquired or resumed. Native mode uses the verified direct attachment setter once when entering native tracking with a locked target. Camera-policy changes reconcile attachment without changing the selected enemy or rebroadcasting hard lock. There is no recurring detached-flag check or detach repair in player ticks or target events.

Free/Smooth attachment prevention includes AbilityCasting and player-controlled Synchronised combat states. Synchronised actions retain their native targeting rules. Death, menus and controller-owned cinematics remain outside the player camera context. Camera evaluation consumes an atomic opaque owner identity validated on the game thread; it performs no UObject reads, registry lookups or Lua calls and does not require the gameplay thread. Attachment events validate current ownership on the game thread; other threads use only the published identity. Owner deletion, rejected player context, master disable and shutdown revoke that identity. Logging includes aggregate attachment-prevention and initial-detach counts. The nine attachment gates preserve volatile registers, flags and XMM values; the two attachment leaves use additional stack padding for Windows x64 call alignment. Scoped native code and layout checks run before installing twenty-seven hooks and thirty-six instruction guards, including the untargeted-direction output stores.

All game objects are accessed on the game thread. Persistent identities read existing object serials and are invalidated by deletion notifications; no weak-reference construction or serial allocation is used. The settings bridge uses host-registered Lua functions and the menu's documented Apply notification. Restart the game after changing native binaries or reloading mod scripts.

## Smooth tracking

The scoped `ProcessViewRotation` hook at RVA `0x17d22f0` uses the native `void(camera, float delta, Vec3* rotation, Vec3* input)` ABI. RebelPlayerCameraManager vtable `0x76a6c58`, slot `0x880`, retains this base implementation. The controller's rotation update calls it at `0x1600de3` with the final accumulated look input from controller offset `0x538`. Tracking accepts only that caller and a validated local player on the game thread. It changes only pitch/yaw input; the original camera modifiers and pitch/yaw/roll limit calls still run. Camera positioning, collision and zoom remain in the native camera stack.

Look input is observed after native deadzones. Controller aim slowdown multiplies input by a strictly positive factor, preserving whether input is zero, so manual override also works with slowdown enabled. Tracking yields on either nonzero look axis, waits for `trackingResumeMs`, then integrates a linear 200 ms engagement ramp. At speed fraction `s`, exponential responsiveness is `8s` per second and the combined angular-speed limit is `180s` degrees/second. Valid frame deltas are capped at 50 ms; invalid deltas or stalls over 250 ms clear timing without a correction. Each update starts from the current view and computes the selected target's error relative to the current camera. It does not retain rotation pointers or write actor rotation directly.

Only Smooth while locked enters this helper. Each eligible update performs at most one current-target aim-point query and two camera getters, with no global searches, native target selection or configuration I/O. Target component and actor identities use the existing deletion listener. Target changes restart the engagement ramp; invalid contexts clear tracking. Optional diagnostics aggregate tracking steps, corrections, manual-input frames and total helper time; disabled Logging performs no tracking timer/counter work for diagnostics.
