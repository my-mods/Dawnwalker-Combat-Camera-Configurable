// Verified operation layouts for Dawnwalker 1.1. Runtime contracts check their consumers.
#pragma once
#include <cstddef>
namespace CombatCamera::Layout {
inline constexpr size_t ObjectIndex=0xc, ObjectName=0x18, ObjectOuter=0x20;
inline constexpr size_t ComponentOwner=0xa8, ComponentFlags=0x8e;
inline constexpr size_t PawnCinematic=0x660, PawnController=0x2e8;
inline constexpr size_t PawnCombat=0xca0, PawnCamera=0xc68;
inline constexpr size_t ControllerLocal=0x6ec, ControllerCharacter=0x8b8, ControllerPawn=0x2f8;
inline constexpr size_t ControllerInputBlocks=0x8c0, ControllerCinematic=0x4c8;
inline constexpr size_t ControllerCamera=0x370, ControllerHUD=0x368, ControllerInput=0x430;
inline constexpr size_t CombatState=0xb40, CombatMode=0xdd0, CombatModeBlocked=0xdd4;
inline constexpr size_t CombatTarget=0x13d0, CameraDetached=0x13ca, HardLock=0x14f9;
inline constexpr size_t CombatIneligible=0x16a2, CombatConfig=0x9d0, TargetingRange=0x26c;
inline constexpr size_t CameraStack=0xa70, EffectBlendState=0x814, FocusMode=0x278;
inline constexpr size_t ActorAimPointSlot=0x6f0, CameraOriginSlot=0x828, CameraRotationSlot=0x820;
inline constexpr size_t AbilityLevelSlot=0x4e0, AbilityTargetTypeSlot=0x540, AbilityValidatorSlot=0x4d0;
inline constexpr size_t AttackSource=0x28, DeathDelegateOwner=0x18;
inline constexpr size_t PickPawn=0x48, PickPrevious=0x50, PickList=0x58, PickFlags=0x60;
inline constexpr size_t PickAngle=0x61, PickDirection=0x62, PickDistance=0x63, PickVisibility=0x64, PickBias=0x65;
inline constexpr size_t FrameNode=0x10, FrameObject=0x18, FrameCode=0x20, ScriptData=0x60, ScriptSize=0x68;
inline constexpr size_t HUDController=0x2b8, HUDCanvas=0x308, CanvasWidth=0x40, CanvasHeight=0x44;
}
