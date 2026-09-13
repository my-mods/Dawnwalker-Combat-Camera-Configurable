# Combat Camera - Configurable

Free camera control during combat in **The Blood of Dawnwalker**, with camera-directed targeting and saved settings.

- Move the combat camera freely; manual lock-on retains the normal locked camera.
- Select enemies toward the camera, with an adjustable targeting cone and switch delay. Committed combat actions keep priority.
- Show a small center dot when a weapon is drawn, throughout gameplay, or never. The dot is drawn through the game's HUD.
- Add optional controller aim slowdown near the current target, with adjustable strength. Mouse movement is excluded.
- Change each feature independently in Mod Setting Menu and press **Apply**. Choices are saved and restored on subsequent launches.

## Requirements

- The Blood of Dawnwalker, Steam build **25232147**.
- [Dawnwalker Framecore UE4SS runtime](https://www.nexusmods.com/thebloodofdawnwalker/mods/283), specifically **2b**.
- [Mod Setting Menu 1.0.6 or later](https://www.nexusmods.com/thebloodofdawnwalker/mods/271) for the in-game controls and immediate Apply.

## Installation

- **Vortex:** Install **Combat-Camera-Configurable.zip** through Vortex, enable it and deploy.
- **Manual:** Copy the archive's **Data/CombatCamera** folder into **The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods**, preserving the folder structure.

## Configuration

Open Mod Settings, select **Combat Camera - Configurable**, adjust the controls and press **Apply**. Settings stay active across save loads and are read again at the next launch.

| Setting | Default | Values |
| --- | --- | --- |
| Enable mod | On | Off / On |
| Free combat camera | On | Off / On |
| Camera-directed targeting | On | Off / On |
| Target switch delay | 65 ms | 0–1000 ms |
| Targeting cone | 45° | 1–90°; 0 uses the native cone |
| Center dot | Off | Off / Weapon drawn / Always in gameplay |
| Controller aim slowdown | On | Off / On |
| Slowdown strength | 35% | 0–80% |
| Logging | Off | Off / On |

The mod creates `settings.ini` inside its `CombatCamera` folder on first launch. The archive does not include a replacement preferences file. Manual edits to that generated file take effect after restarting the game; edit existing keys under `[Settings]` and keep a backup. The settings-menu definition is `mod_settings.ini` and should not be used for personal preferences.

Logging writes Apply events and one aggregate diagnostic summary per ten seconds of active gameplay to `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. Leave it Off during normal play.

## Implementation

Automatic target requests have a shared 50 ms minimum interval. They make one full native selection pass per interval, deferring the fallback search to the following interval when needed. A single-target validity check can retain the current target during that wait or the switch delay. The game continues to handle candidate eligibility, occlusion and combat targeting rules. Settings updates are event-driven, and the center dot uses the native HUD canvas.

See [BUILD.md](BUILD.md) for the source build and supported binary fingerprints.

## Credits

Inspired by [Free Combat Camera – Camera Directed Targeting](https://www.nexusmods.com/thebloodofdawnwalker/mods/340) by xxxxxMIKxxxxx. This is an independent implementation: no code from the original mod was copied or reused. The original mod is not required, and its DLL, configuration and bootstrap are not included.

Thanks to the UE4SS contributors, the Dawnwalker Framecore maintainers, the Mod Setting Menu author for the documented Apply client, and Tsuda Kageyu and contributors for MinHook. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
