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

The executable and host are fingerprinted before installing hooks. Changed or already modified required instructions cause initialization to stop. The address table targets only:

| Input | SHA-256 |
| --- | --- |
| Dawnwalker.exe, Steam build 25232147 | `cb9b7d7bd88a6754c0a9c08318aa64d5013ddfd92d5badcae84e1b4ea980dcfc` |
| Framecore 2b UE4SS.dll | `fb1839ee91f71f83d508d44a2763a15ac1bb0c5fb4e504ac0fcfca64376a054a` |

`GameBuild.hpp` contains game-relative addresses and instruction guards. Updating it requires checking the function signatures, call-site relationships, UObject member offsets, virtual method slots and assembly live-register requirements against the new executable. A matching prologue alone is insufficient. Never simply disable the fingerprint check to support another build.

The camera and cone gates preserve volatile registers, SIMD values and the required comparison flags. The camera gate changes only the native free-camera flag and selected camera target when its predicate allows it. The cone gate modifies only the scoped native threshold comparison. The game still performs candidate scoring, combat predicates and occlusion checks.

Automatic requests use one full native picker pass per 50 ms budget interval. When the primary pass finds nothing, the game's fallback predicate runs in the following interval; a one-element fallback check can retain the current target in the meantime. Native/manual requests retain immediate fallback. A pending switch also validates the current target through a one-element candidate list. Controller assistance computes only the current target's angle, refreshes at most every 50 ms, expires after 150 ms and requires the game's controller sensitivity branch with no mouse input.

All game objects are accessed on the game thread. Persistent identities read existing object serials and are invalidated by deletion notifications; no weak-reference construction or serial allocation is used. The settings bridge uses host-registered Lua functions and the menu's documented Apply notification. Restart the game after changing native binaries or reloading mod scripts.
