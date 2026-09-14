# Combat Camera - Configurable

Choose free camera, smooth target tracking or native tracking during combat in **The Blood of Dawnwalker**, with untargeted combat, camera-directed targeting or a fixed enemy selection.

[Download on Nexus Mods](https://www.nexusmods.com/thebloodofdawnwalker/mods/480)

- Keep the camera free, gently follow a locked enemy, or use native tracking. Free is the default.
- Smooth tracking follows horizontally and vertically, yields to mouse/right-stick movement, and resumes after an adjustable pause. Tune its speed separately from controller aim slowdown.
- Fight without a selected target until you press the normal target-lock button. Unlocked attacks follow the camera's horizontal heading even when the character faces elsewhere. Blocking keeps the game's incoming-direction checks.
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
| Off | Either value | No selected enemy or automatic acquisition. Attack toward the camera's horizontal heading. |
| On | On | Select enemies toward the camera, using the configured cone and switch delay. |
| On | Off | Keep the selected enemy until you unlock or the target is cleared. |

Camera behavior is independent of target selection. Free keeps the camera under your control in all three states. Smooth tracking and Native tracking follow only while an enemy is locked; Smooth yields immediately when you move the camera. To choose a different enemy in fixed-target mode, unlock, look toward the new enemy, and press target lock again. Moving the right stick does not cycle targets. Changing camera or targeting settings with Apply preserves your current lock state. Leaving combat or loading a new player starts untargeted.

## Configuration

Open Mod Settings, select **Combat Camera - Configurable**, adjust the controls and press **Apply**. Settings stay active across save loads and are read again at the next launch.

| Setting | Default | Values |
| --- | --- | --- |
| Enable mod | On | Off / On |
| Camera behavior | Free | Free / Smooth tracking / Native tracking |
| Tracking speed | 50% | 10–100%, in 5% steps; Smooth only |
| Tracking resume delay | 750 ms | 0–3000 ms, in 250 ms steps; Smooth only |
| Camera-directed targeting | On | Off / On |
| Target switch delay | 65 ms | 0–1000 ms |
| Targeting cone | 45° | 1–90°; 0 uses the native cone |
| Center dot | Off | Off / Weapon drawn / Always in gameplay |
| Controller aim slowdown | On | Off / On |
| Slowdown strength | 35% | 0–80% |
| Logging | Off | Off / On |

Smooth tracking starts from the current view and eases toward the target without snapping. After manual camera movement, it waits for the resume delay (750 ms is 0.75 seconds) and eases back in over 200 ms. Tracking speed controls the mod's automatic turn strength, not a percentage of native tracking speed. At 50%, its maximum combined turn rate is 90 degrees per second.

When an enemy temporarily breaks target lock to disappear, Smooth tracking waits for the game's recovery signal and reacquires that same enemy if it is still a valid target, even behind the camera. This works with fixed-target and camera-directed selection. The camera pauses during the disappearance and resumes smoothly from your current view. Unlocking cancels recovery; ordinary target loss and Free/Native behavior keep their existing rules.

The saved camera keys are `cameraMode` (0 Free, 1 Smooth, 2 Native), `trackingSpeed` and `trackingResumeMs`. Older `freeCamera` preferences remain readable but do not control camera behavior. Missing camera preferences start with Free, 50% and 750 ms while existing preferences are retained.

The mod creates `settings.ini` inside its `CombatCamera` folder on first launch. The archive does not include a replacement preferences file. Manual edits to that generated file take effect after restarting the game; edit existing keys under `[Settings]` and keep a backup. The settings-menu definition is `mod_settings.ini` and should not be used for personal preferences.

Logging writes Apply events and one aggregate diagnostic summary per ten seconds of active gameplay to `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. This includes camera mode, tracking updates, manual-input pauses, temporary target losses and recoveries, camera-direction and fallback counts, and aggregate tracking/direction time in microseconds. Leave it Off during normal play.

## Implementation

Automatic target requests run only while target lock and Camera-directed targeting are both on. They have a shared 50 ms minimum interval. They make one full native selection pass per interval, deferring the fallback search to the following interval when needed. A single-target validity check can retain the current target during that wait or the switch delay. The game continues to handle candidate eligibility, occlusion and combat targeting rules. Settings updates are event-driven, and the center dot uses the native HUD canvas.

See [BUILD.md](BUILD.md) for the source build and supported binary fingerprints.

Smooth tracking runs within the native view-rotation update on the game thread, before the game's camera modifiers and rotation limits. It uses only the selected target, with at most one aim-point query and two camera getters per eligible update. It adds no target searches or settings polling. Free, Native and unlocked states skip the tracking helper.

Unlocked attack direction uses camera yaw, so looking up or down keeps attacks horizontal. If camera data is unavailable, the game uses the character's untargeted facing. Both locked modes keep their target-based attack direction.

## Credits

Inspired by [Free Combat Camera – Camera Directed Targeting](https://www.nexusmods.com/thebloodofdawnwalker/mods/340) by xxxxxMIKxxxxx. This is an independent implementation: no code from the original mod was copied or reused. The original mod is not required, and its DLL, configuration and bootstrap are not included.

Thanks to the UE4SS contributors, the Dawnwalker Framecore maintainers, the Mod Setting Menu author for the documented Apply client, and Tsuda Kageyu and contributors for MinHook. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
