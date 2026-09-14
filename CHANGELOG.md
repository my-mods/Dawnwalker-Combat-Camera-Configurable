# Changelog

## Unreleased

- The normal target-lock button toggles untargeted combat and targeted combat.
- Camera-directed targeting selects automatic camera targeting or a fixed enemy while locked.
- The camera stays free in every targeting state, including when Camera-directed targeting is Off.
- Unlocked combat suppresses automatic target acquisition and target-based attack alignment while retaining directional defense checks.
- Stick and mouse movement no longer cycle a fixed target; unlock and lock again to choose another enemy.
- Applying targeting settings preserves the current lock state.

## 1.0.0

- Free combat camera with manual lock-on priority.
- Camera-directed targeting with adjustable cone and switch delay.
- Optional native HUD center dot, set to Off by default, and adjustable controller aim slowdown.
- Persistent Mod Setting Menu settings with immediate Apply.
- Shared 50 ms interval for automatic target searches and optional aggregate logging.
- Corrected native settings argument handling so Center dot Off and the other controls apply independently.
- Unregisters the object-deletion listener during shutdown.
