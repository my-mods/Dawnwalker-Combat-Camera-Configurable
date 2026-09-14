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

The native settings callback consumes nine Lua arguments in `Settings.order`, including the retained legacy `freeCamera` slot. The menu exposes eight controls; free camera behavior is intrinsic to Enable mod. The pinned host's `get_integer` removes the value it reads, so each successive setting is read at stack index 1. This applies to both saved startup settings and Mod Setting Menu Apply.

The executable and host are fingerprinted before installing hooks. Changed or already modified required instructions cause initialization to stop. The address table targets only:

| Input | SHA-256 |
| --- | --- |
| Dawnwalker.exe, Steam build 25232147 | `cb9b7d7bd88a6754c0a9c08318aa64d5013ddfd92d5badcae84e1b4ea980dcfc` |
| Framecore 2b UE4SS.dll | `fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a` |

`GameBuild.hpp` contains game-relative addresses and instruction guards. Updating it requires checking the function signatures, call-site relationships, UObject member offsets, virtual method slots and assembly live-register requirements against the new executable. A matching prologue alone is insufficient. Never simply disable the fingerprint check to support another build.

The camera, cone and untargeted-direction gates preserve volatile registers, SIMD values and the required comparison flags. The camera gate changes the native free-camera flag and selected camera target for the local player, regardless of the target-lock state. The direction gate uses the game's existing planar forward result when targeting is off, bypassing both the persistent target and temporary action-target facing. The cone gate modifies only the scoped native threshold comparison. The game still performs candidate scoring, combat predicates and occlusion checks.

Automatic requests use one full native picker pass per 50 ms budget interval. When the primary pass finds nothing, the game's fallback predicate runs in the following interval; a one-element fallback check can retain the current target in the meantime. An explicit target-lock press retains immediate fallback. A pending switch also validates the current target through a one-element candidate list. Controller assistance is limited to camera-directed targeting. It computes only the current target's angle, refreshes at most every 50 ms, expires after 150 ms and requires the game's controller sensitivity branch with no mouse input.

The lock choice is owned by the local player component and survives settings Apply. Two native Blueprint thunks identify the target-lock action at the checked player graph call sites, retaining the game's existing controller and keyboard bindings. Their parameter decoding remains native. An unexpected player graph length or lock-action call site disables the mod for that session and logs the reason, allowing native input to resume. SwitchLockTarget preserves its boolean return type. Other automatic, look-axis and next/previous target requests cannot replace a fixed target or create an unlocked target. Native target clearing still performs its delegate and notification cleanup. Temporary ability locks cannot enable player targeting.

Native hard lock remains available to combat rules. Nine guarded native attachment stores are intercepted individually, including the direct setter, Blueprint actions, casting, abilities, threat requests, combat transitions, hard lock and target selection. Only the camera-attachment store is skipped for the controlled player; the surrounding native target changes, spell behavior and notifications continue. Constructors and native detachment stores are unchanged. An already attached camera uses the native detach transition once when a valid combat context is acquired or resumed. There is no recurring detached-flag check or detach repair in player ticks or target events.

Camera freedom includes AbilityCasting and player-controlled Synchronised combat states. Synchronised actions retain their native targeting rules. Death, menus and controller-owned cinematics remain outside the player camera context. Camera evaluation consumes an atomic opaque owner identity validated on the game thread; it performs no UObject reads, registry lookups or Lua calls and does not require the gameplay thread. Attachment events validate current ownership on the game thread; other threads use only the published identity. Owner deletion, rejected player context, master disable and shutdown revoke that identity. Logging includes aggregate attachment-prevention and initial-detach counts. The nine attachment gates preserve volatile registers, flags and XMM values; the two attachment leaves use additional stack padding for Windows x64 call alignment. The exact executable is checked before installing twenty-five hooks and twenty-nine instruction guards.

All game objects are accessed on the game thread. Persistent identities read existing object serials and are invalidated by deletion notifications; no weak-reference construction or serial allocation is used. The settings bridge uses host-registered Lua functions and the menu's documented Apply notification. Restart the game after changing native binaries or reloading mod scripts.
