# Combat Camera - Configurable

Free camera control during combat in **The Blood of Dawnwalker**, with untargeted combat, camera-directed targeting or a fixed enemy selection.

[Download on Nexus Mods](https://www.nexusmods.com/thebloodofdawnwalker/mods/480)

- Keep the camera free with targeting on or off.
- Fight without a selected target until you press the normal target-lock button. Attacks use the untargeted forward direction, and blocking keeps the game's incoming-direction checks.
- While locked, select enemies toward the camera or keep your chosen enemy. Adjust the targeting cone and automatic switch delay.
- Show a small center dot when a weapon is drawn, throughout gameplay, or never. The dot is drawn through the game's HUD.
- Add optional controller aim slowdown during camera-directed targeting. Fixed-target and untargeted modes keep full camera sensitivity. Mouse movement is excluded.
- Choose the targeting behavior, center dot and slowdown in Mod Setting Menu and press **Apply**. Preferences are saved for subsequent launches.

## Requirements

- The Blood of Dawnwalker, Steam build **25232147**.
- [Dawnwalker Framecore UE4SS runtime](https://www.nexusmods.com/thebloodofdawnwalker/mods/283), specifically **2b**.
- [Mod Setting Menu 1.0.6 or later](https://www.nexusmods.com/thebloodofdawnwalker/mods/271) for the in-game controls and immediate Apply.

## Installation

- **Vortex:** Install **Combat-Camera-Configurable.zip** through Vortex, enable it and deploy.
- **Manual:** Copy the archive's **Data/CombatCamera** folder into **The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods**, preserving the folder structure.

## Controls

Use **R3 / right-stick click**, or your configured keyboard/controller **target-lock** binding, to toggle targeting. Stick axes and mouse movement remain camera controls.

| Target-lock state | Camera-directed targeting | Behavior |
| --- | --- | --- |
| Off | Either value | No selected enemy and no automatic acquisition. |
| On | On | Select enemies toward the camera, using the configured cone and switch delay. |
| On | Off | Keep the selected enemy until you unlock or the target is cleared. |

The camera stays free in all three states. To choose a different enemy in fixed-target mode, unlock, look toward the new enemy, and press target lock again. Moving the right stick does not cycle targets. Changing Camera-directed targeting with Apply preserves your current lock state. Leaving combat or loading a new player starts untargeted.

## Configuration

Open Mod Settings, select **Combat Camera - Configurable**, adjust the controls and press **Apply**. Settings stay active across save loads and are read again at the next launch.

| Setting | Default | Values |
| --- | --- | --- |
| Enable mod | On | Off / On |
| Camera-directed targeting | On | Off / On |
| Target switch delay | 65 ms | 0–1000 ms |
| Targeting cone | 45° | 1–90°; 0 uses the native cone |
| Center dot | Off | Off / Weapon drawn / Always in gameplay |
| Controller aim slowdown | On | Off / On |
| Slowdown strength | 35% | 0–80% |
| Logging | Off | Off / On |

Camera freedom is part of Enable mod. Older `freeCamera` preferences remain readable but no longer control camera locking.

The mod creates `settings.ini` inside its `CombatCamera` folder on first launch. The archive does not include a replacement preferences file. Manual edits to that generated file take effect after restarting the game; edit existing keys under `[Settings]` and keep a backup. The settings-menu definition is `mod_settings.ini` and should not be used for personal preferences.

Logging writes Apply events and one aggregate diagnostic summary per ten seconds of active gameplay to `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Leave it Off during normal play.

## Implementation

Automatic target requests run only while target lock and Camera-directed targeting are both on. They have a shared 50 ms minimum interval. They make one full native selection pass per interval, deferring the fallback search to the following interval when needed. A single-target validity check can retain the current target during that wait or the switch delay. The game continues to handle candidate eligibility, occlusion and combat targeting rules. Settings updates are event-driven, and the center dot uses the native HUD canvas.

See [BUILD.md](BUILD.md) for the source build and supported binary fingerprints.

## Credits

Inspired by [Free Combat Camera – Camera Directed Targeting](https://www.nexusmods.com/thebloodofdawnwalker/mods/340) by xxxxxMIKxxxxx. This is an independent implementation: no code from the original mod was copied or reused. The original mod is not required, and its DLL, configuration and bootstrap are not included.

Thanks to the UE4SS contributors, the Dawnwalker Framecore maintainers, the Mod Setting Menu author for the documented Apply client, and Tsuda Kageyu and contributors for MinHook. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
