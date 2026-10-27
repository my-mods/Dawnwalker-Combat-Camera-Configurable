// Reference: Steam 25915545 / Dawnwalker 1.1. GameCode.hpp validates operation contracts.
#pragma once
#include <array>
#include <cstdint>
namespace CombatCamera::Build {
inline constexpr uintptr_t EffectStart=0x60219e4;
inline constexpr uintptr_t EffectCameraVtable=0x780c7e8;
inline constexpr uintptr_t CanAbility=0x5fc04b0;
inline constexpr uintptr_t SpellCanAbility=0x6014a60;
inline constexpr uintptr_t PlanAbility=0x602ceb4;
inline constexpr uintptr_t InstantAbility=0x60176a4;
inline constexpr uintptr_t FocusActor=0x601e9a8;
inline constexpr uintptr_t BlendView=0x5513110;
inline uintptr_t FocusEnterViewReturn=0x6011f82;
inline uintptr_t FocusResumeViewReturn=0x6012031;
inline uintptr_t FocusTargetViewReturn=0x6025376;
inline constexpr uintptr_t TargetDeathDelegateVtable=0x8db7cd0;
inline constexpr uintptr_t TargetDeathExecute=0x6036ae0;
inline constexpr uintptr_t TargetDeathExecuteSafe=0x6036b7c;
inline constexpr uintptr_t Tick=0x1914234;
inline constexpr uintptr_t Switch=0x60467c8;
inline constexpr uintptr_t Picker=0x5fd084c;
inline constexpr uintptr_t Score=0x5fd0d64;
inline constexpr uintptr_t SetTarget=0x6045358;
inline constexpr uintptr_t Request=0x60441fc;
inline constexpr uintptr_t Lock=0x604521c;
inline constexpr uintptr_t LoseLock=0x603d658;
inline constexpr uintptr_t ReactToHit=0x6042f18;
inline uintptr_t LoseLockClearReturn=0x603d6d5;
inline uintptr_t RegainLockReturn=0x604340e;
inline constexpr uintptr_t DetachCamera=0x603642c;
inline constexpr uintptr_t AttachDirect=0x289ed70;
inline constexpr uintptr_t AttachRequest=0x5ff9c99;
inline constexpr uintptr_t AttachScript=0x5ffb1f1;
inline constexpr uintptr_t AttachCast=0x60118a6;
inline constexpr uintptr_t AttachAbility=0x601203b;
inline constexpr uintptr_t AttachThreat=0x6017172;
inline constexpr uintptr_t AttachCombat=0x6040e6c;
inline constexpr uintptr_t AttachLock=0x6045224;
inline constexpr uintptr_t AttachSelection=0x64adb5a;
inline constexpr uintptr_t LockScript=0x5ffc8c0;
inline constexpr uintptr_t SwitchScript=0x5ffcacc;
inline constexpr uintptr_t ActionTarget=0x60346ac;
inline constexpr uintptr_t AttackTarget=0x6044fc8;
inline constexpr uintptr_t Forward=0x6038cf9;
inline constexpr uintptr_t FreeDirection=0x60390da;
inline constexpr uintptr_t PlayerCombatVtable=0x805c1c0;
inline constexpr uintptr_t PlayerCameraVtable=0x7c311f8;
inline constexpr uintptr_t CameraComponentVtable=0x789d8c8;
inline constexpr uintptr_t ViewRotation=0x18807fc;
inline uintptr_t ViewRotationReturn=0x187ff89;
inline constexpr uintptr_t Camera=0x600db62;
inline constexpr uintptr_t CameraResume=0x600db81;
inline constexpr uintptr_t Cone=0x5fd0d75;
inline constexpr uintptr_t ConeResume=0x5fd0d7d;
inline constexpr uintptr_t DrawHUD=0x1db01d0;
inline constexpr uintptr_t DrawBox=0x566f4b8;
inline constexpr uintptr_t Modify=0x17a7278;
inline constexpr uintptr_t QuerySetting=0x17a7f70;
inline uintptr_t InputSettingReturn=0x3f410ce;
inline uintptr_t PickerReturn1=0x6046a1e;
inline uintptr_t PickerReturn2=0x6046b8b;
inline uintptr_t AutoSetReturn=0x6043328;
inline uintptr_t AutoRequestReturn=0x601717e;
inline uintptr_t CastingLockReturn=0x60118c0;
inline constexpr uintptr_t MouseDelta=0x5509718;
inline constexpr uintptr_t MakeName=0x1345d4c;
inline constexpr double nativeCone=0.0;
inline uint64_t lookName{},lookPackageName{},playerGraphName{},playerClassName{};
// Exact instruction windows are required only for mid-function assembly gates.
struct Guard{uintptr_t rva;size_t size;std::array<unsigned char,32> bytes;};
inline constexpr std::array<Guard,15> guards{{
{Score,11,{0x66,0x41,0xf,0x2f,0xf7,0xf,0x83,0xb,0x1,0x0,0x0}},
{Camera,31,{0x80,0xbf,0x18,0xc,0x0,0x0,0x1,0x48,0x8b,0xf0,0x75,0x5,0x45,0x8a,0xe5,0xeb,0xe,0x45,0x8a,0xa7,0xca,0x13,0x0,0x0,0x45,0x84,0xe4,0x49,0xf,0x45,0xf5}},
{Cone,8,{0x66,0xf,0x2f,0x15,0xf3,0xf5,0xe8,0x2}},
{Forward,16,{0x48,0x8b,0x8f,0x88,0x10,0x0,0x0,0x48,0x85,0xc9,0xf,0x84,0x90,0x1,0x0,0x0}},
{FreeDirection,21,{0xf,0x28,0xc7,0x66,0x41,0xf,0x14,0xc0,0xf,0x11,0x3,0xf2,0xf,0x10,0x45,0xb7,0xf2,0xf,0x11,0x43,0x10}},
{AttachDirect,7,{0xc6,0x81,0xca,0x13,0x0,0x0,0x0}},
{AttachRequest,7,{0xc6,0x81,0xca,0x13,0x0,0x0,0x0}},
{AttachScript,7,{0xc6,0x85,0xca,0x13,0x0,0x0,0x0}},
{AttachCast,7,{0xc6,0x80,0xca,0x13,0x0,0x0,0x0}},
{AttachAbility,7,{0xc6,0x80,0xca,0x13,0x0,0x0,0x0}},
{AttachThreat,7,{0xc6,0x80,0xca,0x13,0x0,0x0,0x0}},
{AttachCombat,7,{0x44,0x88,0xb7,0xca,0x13,0x0,0x0}},
{AttachLock,7,{0xc6,0x81,0xca,0x13,0x0,0x0,0x0}},
{AttachSelection,7,{0xc6,0x80,0xca,0x13,0x0,0x0,0x0}},
{AttachDirect+7,1,{0xc3}},
}};
}
