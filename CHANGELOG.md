# Changelog

## Unreleased

- Restore Smooth tracking after an enemy's temporary disappearance, including reappearance behind the camera, with fixed-target and camera-directed selection.
- Add Free, Smooth tracking and Native tracking camera behaviors, independently of target selection.
- Follow locked enemies horizontally and vertically with adjustable smooth tracking speed.
- Yield to manual camera movement and gently resume after an adjustable pause.
- Preserve the selected enemy when applying camera settings.

## 2.0.0

- Add three combat modes: camera-directed targeting, fixed enemy selection, and untargeted combat.
- Keep the camera free in every mode, including manual lock-on, spells and combat actions.
- Use the normal target-lock button to enter or leave targeting. Camera-directed targeting chooses automatic or fixed selection while locked.
- Aim unlocked attacks along the camera's horizontal heading, even when the character faces elsewhere.
- Stop automatic enemy selection while unlocked and retain the game's incoming-direction blocking checks.
- Keep the chosen enemy in fixed-target mode. Stick and mouse movement control the camera; unlock, aim and lock again to choose another enemy.
- Preserve the current lock state when applying targeting settings.
- Make camera freedom part of Enable mod and remove the separate Free combat camera setting.

## 1.0.0

- Free combat camera with manual lock-on priority.
- Camera-directed targeting with adjustable cone and switch delay.
- Optional native HUD center dot, set to Off by default, and adjustable controller aim slowdown.
- Persistent Mod Setting Menu settings with immediate Apply.
- Shared 50 ms interval for automatic target searches and optional aggregate logging.
- Corrected native settings argument handling so Center dot Off and the other controls apply independently.
- Unregisters the object-deletion listener during shutdown.
