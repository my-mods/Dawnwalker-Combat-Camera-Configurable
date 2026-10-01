# Combat Camera - Configurable

Choose free camera, smooth target tracking or native tracking during combat in **The Blood of Dawnwalker**, with untargeted combat, camera-directed targeting or a fixed enemy selection.

[Download on Nexus Mods](https://www.nexusmods.com/thebloodofdawnwalker/mods/480)

- **Free Camera:** Move the camera yourself, with no automatic enemy tracking. This is the default.
- **Smooth Tracking Camera:** Gently follow your locked enemy horizontally and vertically. Move the mouse or right stick at any time to take control; tracking resumes after an adjustable pause. Adjust Tracking speed to set its strength, and Tracking catch-up to turn faster when the enemy moves far from your camera's aim.
- **Native Tracking Camera:** Use the game's normal target tracking speed.
- With Automatic lock on hit Off, fight without a selected target until you press the normal target-lock button. Unlocked attacks follow the camera's horizontal heading even when the character faces elsewhere. Blocking keeps the game's incoming-direction checks.
- Use abilities while unlocked. Self-centered abilities cast without automatic enemy selection. Abilities that require a primary enemy, including target-dependent area attacks, select an eligible enemy toward the camera when cast, ignoring any old ability target. Normal costs, cooldowns and target requirements still apply; combat lock stays off.
- While locked, select enemies toward the camera or keep your chosen enemy. Adjust the targeting cone and automatic switch delay.
- Optionally **Lock to last attacker** after a hit, block or parry while already locked on. This is Off by default and works with every camera behavior.
- **Automatic lock on hit**, On by default, acquires an attacker even while unlocked. **After target death** selects the nearest eligible enemy by default, or waits for the next hit. Smooth tracking keeps your chosen speed and manual-input behavior.
- Show a small center dot when a weapon is drawn, throughout gameplay, or never. The dot is drawn through the game's HUD.
- Add optional controller aim slowdown during camera-directed targeting. Fixed-target and untargeted modes keep full camera sensitivity. Mouse movement is excluded.
- Choose camera behavior, tracking strength, resume delay, target selection, center dot and slowdown in Mod Setting Menu and press **Apply**. Preferences are saved for subsequent launches.

## Requirements

- The Blood of Dawnwalker for PC, with the native functions supported by the bundled integration. Reference build: **1.0.5 / Steam 25232147**.
- A Dawnwalker-compatible UE4SS installation providing the imported C++ mod and Lua APIs. Framecore 2b and Vercadi RC6 are tested references; their version names and DLL hashes are not runtime restrictions.
- [Mod Setting Menu 1.0.6 or later](https://www.nexusmods.com/thebloodofdawnwalker/mods/271) for the in-game controls and immediate Apply.

## Installation

- **Vortex:** Install **Combat-Camera-Configurable.zip** through Vortex, enable it and deploy.
- **Manual:** Copy the archive's **Data/CombatCamera** folder into **The Blood of Dawnwalker/Dawnwalker/Binaries/Win64/ue4ss/Mods**, preserving the folder structure.

## Controls

Use **R3 / right-stick click**, or your configured keyboard/controller **target-lock** binding, to toggle targeting. Stick axes and mouse movement remain camera controls.

| Target-lock state | Camera-directed targeting | Behavior |
| --- | --- | --- |
| Off | Either value | No selected enemy. Automatic lock on hit can acquire an incoming attacker when enabled. Attack toward the camera's horizontal heading. |
| On | On | Select enemies toward the camera, using the configured cone and switch delay. |
| On | Off | Keep the selected enemy until you unlock or the target is cleared. |

Camera behavior is independent of target selection. Free keeps the camera under your control in all three states. Smooth tracking and Native tracking follow only while an enemy is locked; Smooth yields immediately when you move the camera. To choose a different enemy in fixed-target mode, unlock, look toward the new enemy, and press target lock again. Moving the right stick does not cycle targets. Changing camera or targeting settings with Apply preserves your current lock state. Leaving combat or loading a new player starts untargeted.

**Lock to last attacker** adds a priority to either locked targeting style. A hit, including a blocked or parried hit, switches to that attacker and gives them priority. With Camera-directed targeting On, move the camera toward another eligible enemy and hold aim for Target switch delay to choose them instead. Automatic tracking alone does not override the attacker. With Camera-directed targeting Off, camera movement keeps the attacker selected. The next attacker to hit you takes priority. Hits from behind do not need to pass the camera cone or target switch delay, but the enemy must still satisfy the game's range, visibility and target eligibility rules. An unavailable attacker leaves your current target alone. Unlocking cancels the priority; hits while unlocked only turn target lock on when Automatic lock on hit is enabled. Enabling the option starts listening for new hits and does not select an earlier attacker. Ordinary target loss releases the priority; Smooth tracking retains it during its supported temporary-disappearance recovery.

**Automatic lock on hit** works independently of Lock to last attacker and is On by default. Lock onto any enemy whose attack hits you, including successful blocks and parries, then follow later attackers in the same way. Select **Smooth tracking** for gentle following; the option keeps your chosen camera behavior. Camera-directed targeting On also lets deliberate camera aiming override the held attacker; turn it Off to keep the enemy until another hit, target loss or manual unlock.

With Automatic lock on hit On, **After target death** defaults to **Nearest enemy**. When the locked target dies, select the nearest eligible enemy in native targeting range, including behind you. Distance is measured from your character to each enemy's target point. If none qualifies, or you choose **Wait for next hit**, stay unlocked until another hit, block or parry, or use the lock button yourself. Native range, eligibility and visibility rules remain. Manual unlock cancels pending acquisition; a later incoming hit may lock again. Ordinary target loss and supported temporary disappearance keep their existing behavior.

## Configuration

Open Mod Settings, select **Combat Camera - Configurable**, adjust the controls and press **Apply**. Settings stay active across save loads and are read again at the next launch.

| Setting | Default | Values |
| --- | --- | --- |
| Enable mod | On | Off / On |
| Camera behavior | Free | Free / Smooth tracking / Native tracking |
| Tracking speed | 50% | 10Ã¢â‚¬â€œ100%, in 5% steps; Smooth only |
| Tracking resume delay | 750 ms | 0Ã¢â‚¬â€œ3000 ms, in 50 ms steps; Smooth only |
| Camera-directed targeting | On | Off / On |
| Lock to last attacker | Off | Off / On; requires active target lock |
| Target switch delay | 65 ms | 0Ã¢â‚¬â€œ1000 ms |
| Targeting cone | 45Ã‚Â° | 1Ã¢â‚¬â€œ90Ã‚Â°; 0 uses the native cone |
| Center dot | Off | Off / Weapon drawn / Always in gameplay |
| Controller aim slowdown | On | Off / On |
| Slowdown strength | 35% | 0Ã¢â‚¬â€œ80% |
| Automatic lock on hit | On | Off / On |
| After target death | Nearest enemy | Nearest enemy / Wait for next hit; Automatic lock on hit only |
| Tracking catch-up | Up to 3x | Off / Up to 2x / 3x / 4x / 5x; Smooth only |
| Logging | Off | Off / On |

Smooth tracking starts from the current view and eases toward the target without snapping. After manual camera movement, it waits for the resume delay (750 ms is 0.75 seconds) and eases back in over 200 ms. Tracking speed controls the mod's automatic turn strength, not a percentage of native tracking speed. With Tracking catch-up Off, the maximum combined turn rate is 90 degrees per second at 50% speed, or 180 degrees per second at 100% speed. Tracking catch-up defaults to Up to 3x: the boost increases smoothly between 45 and 180 degrees from the target, then fades back to normal as the camera catches up. At 50% speed and a full half-turn, the default maximum is 270 degrees per second. The camera always takes the shortest turn and keeps the smooth engagement ramp. Set catch-up to Off to retain the base turn rate. With Camera-directed targeting On, keep Tracking resume delay longer than Target switch delay and allow extra time for target selection: searches run at 50 ms intervals and a fallback can need another interval. Otherwise tracking can pull the camera away before your new target is selected.

When a tracked enemy disappears, Smooth tracking remembers that same living enemy for a short recovery window, including when the enemy reappears behind you. A temporary-loss signal uses the game's recovery timer with up to three extra gameplay seconds for late reappearance. A clear of the enemy already being followed allows up to three gameplay seconds to recover it. Retries retain native range, visibility and eligibility checks; an unavailable enemy receives no camera correction. Recovery works with fixed-target and camera-directed selection and resumes from your current view. Camera-directed searches pause for up to three seconds while the recovered camera catches up, ending earlier near the target or when you move the camera. Unlocking cancels recovery; death, a different selection, leaving combat or changing camera mode also ends it. Free and Native keep their existing behavior.

The saved camera keys are `cameraMode` (0 Free, 1 Smooth, 2 Native), `trackingSpeed`, `trackingResumeMs` and `trackingCatchup` (100 disables the boost; 200-500 sets its maximum percentage, default 300). Older `freeCamera` preferences remain readable but do not control camera behavior. Missing camera preferences start with Free, 50% and 750 ms while existing preferences are retained.

The saved attacker option is `lockLastAttacker` (0 Off, 1 On). Existing preferences receive the missing key with value 0. Changing this option with Apply preserves your lock state; turning it Off releases its targeting priority. Automatic lock on hit (`autoLockOnHit`, 0 Off / 1 On) also enables attacker priority and defaults to 1. Its death choice is `afterTargetDeath` (0 Nearest enemy / 1 Wait for next hit), defaulting to 0. Missing keys receive these defaults without replacing existing preferences, including an explicitly saved Off choice.

The mod creates `settings.ini` inside its `CombatCamera` folder on first launch. The archive does not include a replacement preferences file. Manual edits to that generated file take effect after restarting the game; edit existing keys under `[Settings]` and keep a backup. The settings-menu definition is `mod_settings.ini` and should not be used for personal preferences.

Logging writes Apply events and one aggregate diagnostic summary per ten seconds of active gameplay to `Dawnwalker/Binaries/Win64/ue4ss/UE4SS.log`. This includes camera mode, tracking updates, manual-input pauses, temporary target losses, tracked-target clears, recovery attempts and results, attacker hits/switches/rejections/manual overrides, automatic locks and target-death searches/switches/misses, camera-direction and fallback counts, and aggregate tracking/recovery/direction/attacker/death-selection time in microseconds. Leave it Off during normal play.

## Implementation

Camera-directed requests run only while target lock and Camera-directed targeting are both on. They have a shared 50 ms minimum interval. They make one full native selection pass per interval, deferring the fallback search to the following interval when needed. A single-target validity check can retain the current target during that wait or the switch delay. The game continues to handle candidate eligibility, occlusion and combat targeting rules. Settings updates are event-driven, and the center dot uses the native HUD canvas.

Last-attacker requests use the same interval and check only that attacker within the native candidate list. Rapid hits coalesce into the newest request. A pending hit can wait up to one second of gameplay time for an eligible player state; an attempted but rejected selection is not retried. Holding the attacker suspends camera-directed searches until manual camera movement requests an override. A different candidate must remain preferred for Target switch delay; a valid attacker can stay selected outside the camera cone during that wait. Looking back at the attacker or finding no alternative ends the override attempt. A new hit restarts attacker priority. Fixed-target mode holds the selected attacker until another hit or target loss. The selected camera behavior and optional controller slowdown remain active. The option adds no settings polling or object searches while idle.

Automatic hit acquisition uses the same coalesced request and 50 ms budget. A death handoff queues one nearest-enemy pass and, if needed, one fallback pass on the next budget interval. It waits up to three seconds of ordinary gameplay readiness; synchronized actions defer service until they finish. Failure leaves targeting unlocked, with no continuing nearest-enemy searches. No new timer, global object search or settings polling is added.

See [BUILD.md](BUILD.md) for the source build and supported binary fingerprints.

Startup checks the native functions, camera table and patch sites used by this mod. It does not require a particular executable-file hash or storefront. If required game code differs, the mod stops before installing hooks and logs the failing address in `UE4SS.log`. UE4SS version names and whole-file hashes do not disable the mod.

Smooth tracking runs within the native view-rotation update on the game thread, before the game's camera modifiers and rotation limits. It uses only the selected target, with at most one aim-point query and two camera getters per eligible update. It adds no target searches or settings polling to the view update. During a pending disappearance only, the existing player tick retries the remembered enemy at most every 100 gameplay milliseconds, also sharing the 50 ms selection budget. Free, Native and unlocked states skip the tracking helper.

Unlocked attack direction uses camera yaw, so looking up or down keeps attacks horizontal. If camera data is unavailable, the game uses the character's untargeted facing. Both locked modes keep their target-based attack direction.

## Credits

Inspired by [Free Combat Camera Ã¢â‚¬â€œ Camera Directed Targeting](https://www.nexusmods.com/thebloodofdawnwalker/mods/340) by xxxxxMIKxxxxx. This is an independent implementation: no code from the original mod was copied or reused. The original mod is not required, and its DLL, configuration and bootstrap are not included.

Thanks to the UE4SS contributors, the Dawnwalker Framecore maintainers, the Mod Setting Menu author for the documented Apply client, and Tsuda Kageyu and contributors for MinHook. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
