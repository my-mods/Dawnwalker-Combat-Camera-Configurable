// Native interoperability for Steam build 25232147. All UObject access is on
// the game thread. No object enumeration, worker, overlay window or INI watch.
#include <Mod/CppUserModBase.hpp>
#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObject.hpp>
#include <DynamicOutput/Output.hpp>
#include <Unreal/Core/Windows/AllowWindowsPlatformTypes.hpp>
#include <Windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <bit>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include "Bridge.hpp"
#include "GameBuild.hpp"
#include "GameCode.hpp"

extern "C" {
    void CameraGate(); void ConeGate(); void ForwardGate();
    void ScoreGate(); void* ScoreOriginal{};
    void AttachDirectGate(); void* AttachDirectOriginal{}; void* AttachDirectContinue{};
    void AttachRequestGate(); void* AttachRequestOriginal{}; void* AttachRequestContinue{};
    void AttachScriptGate(); void* AttachScriptOriginal{}; void* AttachScriptContinue{};
    void AttachCastGate(); void* AttachCastOriginal{}; void* AttachCastContinue{};
    void AttachAbilityGate(); void* AttachAbilityOriginal{}; void* AttachAbilityContinue{};
    void AttachThreatGate(); void* AttachThreatOriginal{}; void* AttachThreatContinue{};
    void AttachCombatGate(); void* AttachCombatOriginal{}; void* AttachCombatContinue{};
    void AttachLockGate(); void* AttachLockOriginal{}; void* AttachLockContinue{};
    void AttachSelectionGate(); void* AttachSelectionOriginal{}; void* AttachSelectionContinue{};
    void* CameraOriginal{}; void* CameraContinue{};
    void* ConeContinue{};
    void* ForwardOriginal{}; void* ForwardContinue{};
}
namespace CombatCamera {
namespace {
using namespace RC::Unreal;
using Object=void;
template<class T> T& field(void* p,size_t offset){return *reinterpret_cast<T*>(static_cast<char*>(p)+offset);}
uintptr_t moduleBase{};
template<class Fn> Fn at(uintptr_t rva){return reinterpret_cast<Fn>(moduleBase+rva);}
template<class Fn> Fn method(void* p,size_t offset){return field<Fn>(*static_cast<void**>(p),offset);}
std::atomic_bool active{},installed{};
// Camera evaluation may run separately from gameplay. Publish only an opaque
// identity match; camera callbacks never inspect UObjects or gameplay fields.
std::atomic<void*> cameraOwner{};
std::atomic_bool cameraLogging{};
struct CameraMetrics {std::atomic_uint64_t checks{},accepted{},otherThread{},attachPrevented{},initialDetaches{},viewChecks{},viewOtherThread{};} cameraMetrics;
bool cameraInitialized{};
Settings settings;
DWORD gameThread{};
bool attempted{};
bool inputSupported{true};
std::wstring startError;
std::array<void*,32> hooked{};size_t hookCount{};
RequestBudget requestBudget;
RequestBudget assistBudget;
double coneThreshold{-1};
struct Identity {
    uintptr_t address{};int index{-1},serial{};
    bool operator==(const Identity&)const=default;
};
Identity identity(void* object) {
    if(!object)return {};
    auto index=field<int>(object,0xc);
    auto item=FUObjectArray::IndexToObject(index);
    if(!item||item->GetUObject()!=object||!FUObjectArray::IsValid(item,false))return {};
    return {reinterpret_cast<uintptr_t>(object),index,item->GetSerialNumber()};
}
struct Session final:FUObjectDeleteListener {
    std::array<std::atomic_int,14> watched;
    std::atomic_uint32_t invalidated{};
    Session(){for(auto& x:watched)x.store(-1);}
    void NotifyUObjectDeleted(const UObjectBase*,int32_t index) override {
        uint32_t mask=0;
        for(size_t i=0;i<14;++i)if(watched[i].load(std::memory_order_relaxed)==index)mask|=1u<<i;
        if(mask&1)cameraOwner.store(nullptr,std::memory_order_release);
        if(mask)invalidated.fetch_or(mask,std::memory_order_release);
    }
    void OnUObjectArrayShutdown()override;
} session;
bool listening{};
void Session::OnUObjectArrayShutdown() {
    active=false;cameraOwner.store(nullptr,std::memory_order_release);invalidated.fetch_or(0x3fff);
    // Unregister before the engine checks its shutdown listener registry.
    // Leave hook teardown to stop(); no dying game objects are accessed here.
    if(listening){FUObjectArray::RemoveUObjectDeleteListener(this);listening=false;}
}
Identity ownerId,castLockId,assistTargetId,pendingId;
// Shared only by on-demand enemy checks, with indexed lifetime validation.
Identity abilityTargetId,abilityActorId;
uint64_t abilityQueryAt{};bool abilityQueried{};
Identity trackingTargetId,trackingActorId;
Tracking tracking;
double recoveryHold{};
bool nativeCamera{};
void clearTracking() {
    tracking.clear();trackingTargetId={};trackingActorId={};recoveryHold=0;
    session.watched[4]=-1;session.watched[5]=-1;
}
Dwell dwell;
bool nextFallback{};
bool playerLocked{};
uint64_t lockIntent{};
double deathRemaining{};
bool deathFallback{};
void clearDeath(){deathRemaining=0;deathFallback=false;}
bool clearAttackPending{true};
// Hit callbacks retain only indexed identities. The existing player tick makes
// one bounded selection attempt after the native hit reaction has completed.
Identity attackerPendingTargetId,attackerPendingActorId,attackerHeldTargetId,attackerHeldActorId;
double attackerRemaining{};
bool attackerSelectionInvalidated{};
bool attackerLookOverride{};
void clearAttackerLook() {
    attackerLookOverride=false;dwell.clear();pendingId={};session.watched[3]=-1;
}
void clearAttackerPending() {
    if(!attackerPendingTargetId.address)return;
    attackerPendingTargetId={};attackerPendingActorId={};attackerRemaining=0;
    session.watched[8]=-1;session.watched[9]=-1;
}
void clearAttackerHeld() {
    if(!attackerHeldTargetId.address)return;
    clearAttackerLook();
    attackerHeldTargetId={};attackerHeldActorId={};
    session.watched[10]=-1;session.watched[11]=-1;
}
void clearAttacker(){clearAttackerPending();clearAttackerHeld();}
// A native temporary loss is different from an ordinary target clear. Keep
// identities through a bounded recovery window; never follow a hidden actor.
Identity recoveryTargetId,recoveryActorId;
double recoveryRemaining{};
double recoveryRetry{};
bool recoveryReady{};
uint64_t recoveryLookAt{};
bool recoveryHadLook{};
void clearRecovery() {
    if(!recoveryRemaining)return;
    recoveryTargetId={};recoveryActorId={};recoveryRemaining=0;recoveryLookAt=0;recoveryHadLook=false;
    recoveryReady=false;recoveryRetry=0;
    session.watched[6]=-1;session.watched[7]=-1;
}
void rememberRecoveryLook() {
    if(trackingTargetId==recoveryTargetId&&trackingActorId==recoveryActorId&&
       tracking.quiet<settings.trackingResumeMs*0.001){
        const auto now=GetTickCount64();
        const auto elapsed=static_cast<uint64_t>(std::max(0.0,tracking.quiet)*1000.0);
        recoveryHadLook=true;recoveryLookAt=now-std::min(now,elapsed);
    }
}
void abandonRecovery() {
    if(recoveryRemaining&&!settings.targeting){playerLocked=false;clearAttackPending=true;}
    if(recoveryRemaining)clearAttackerHeld();
    clearRecovery();
}
void* resolve(const Identity& id) {
    // Resolve the saved slot before dereferencing a possibly deleted address.
    if(!id.address)return nullptr;
    auto item=FUObjectArray::IndexToObject(id.index);
    return item&&reinterpret_cast<uintptr_t>(item->GetUObject())==id.address&&
        FUObjectArray::IsValid(item,false)&&item->GetSerialNumber()==id.serial?item->GetUObject():nullptr;
}
void* resolveAttacker(const Identity& id) {
    // The native selector can assign the first serial. Deletion watches remain
    // armed across native calls, so accepting that assignment cannot accept reuse.
    if(!id.address)return nullptr;
    auto item=FUObjectArray::IndexToObject(id.index);
    return item&&reinterpret_cast<uintptr_t>(item->GetUObject())==id.address&&
        FUObjectArray::IsValid(item,false)&&(!id.serial||item->GetSerialNumber()==id.serial)?item->GetUObject():nullptr;
}
double assistScale{1.0};uint64_t assistAt{};
void clearSession() {
    ++lockIntent;clearDeath();
    attackerSelectionInvalidated=true;
    abilityTargetId={};abilityActorId={};abilityQueried=false;
    cameraInitialized=false;
    nativeCamera=false;clearTracking();clearRecovery();clearAttacker();
    cameraOwner.store(nullptr,std::memory_order_release);
    ownerId={};castLockId={};assistTargetId={};pendingId={};dwell.clear();nextFallback=false;assistScale=1;assistAt=0;playerLocked=false;clearAttackPending=true;
    for(auto& x:session.watched)x.store(-1,std::memory_order_relaxed);
}
void syncSession() {
    auto mask=session.invalidated.exchange(0,std::memory_order_acq_rel);
    if(mask&1){clearSession();return;}
    if(mask&2){castLockId={};session.watched[1]=-1;}
    if(mask&4){assistTargetId={};assistScale=1;assistAt=0;session.watched[2]=-1;}
    if(mask&8){pendingId={};dwell.clear();session.watched[3]=-1;}
    if(mask&48)clearTracking();
    if(mask&192)abandonRecovery();
    if(mask&768)clearAttackerPending();
    if(mask&3072)clearAttackerHeld();
    if(mask&12288)attackerSelectionInvalidated=true;
}
bool live(){return active.load(std::memory_order_relaxed)&&GetCurrentThreadId()==gameThread;}
bool sameCurrent(void* object,const Identity& id){return id.address && identity(object)==id;}
// These relations are checked together, rather than treating a non-null
// controller/pawn/component pointer as a sufficient gameplay context.
int gameplay(void* pawn,bool cameraContext=false) {
    if(!pawn||field<uint8_t>(pawn,0x660))return 0;
    auto pc=field<void*>(pawn,0x2e8);auto combat=field<void*>(pawn,0xc90);
    if(!pc||!combat||field<uint8_t>(pc,0x6ec)!=1||field<void*>(pc,0x8b8)!=pawn||
       field<void*>(pc,0x2f8)!=pawn||field<void*>(combat,0xa8)!=pawn||field<int>(pc,0x8c0)!=0)return 0;
    if(field<uint8_t>(pc,0x4c8)&2)return 0; // Native cursor/menu gate.
    const auto action=field<uint8_t>(combat,0xb28);
    // Dead is outside gameplay. Synchronised actions retain native targeting
    // rules, but are not an exception to free camera while player-controlled.
    if(action==10||(!cameraContext&&action==11))return 0;
    return (field<uint8_t>(combat,0x8e)&0x10)&&field<uint8_t>(combat,0xdb8)<=2&&field<uint8_t>(combat,0xdbc)==0?2:1;
}
bool managed(void* combat,bool cameraContext=false) {
    if(!live()||!combat)return false;
    if(field<void*>(combat,0)!=at<void*>(Build::PlayerCombatVtable))return false;
    syncSession();
    auto pawn=field<void*>(combat,0xa8);
    if(!gameplay(pawn,true)||field<void*>(pawn,0xc90)!=combat){
        auto expected=combat;cameraOwner.compare_exchange_strong(expected,nullptr,std::memory_order_acq_rel);
        if(ownerId.address==reinterpret_cast<uintptr_t>(combat)){cameraInitialized=false;clearTracking();abandonRecovery();clearAttacker();clearDeath();}
        return false;
    }
    auto id=identity(combat);if(!id.address)return false;
    if(id!=ownerId){clearSession();ownerId=id;session.watched[0]=id.index;}
    const bool native=settings.cameraMode==2&&playerLocked&&field<void*>(combat,0x1380);
    cameraOwner.store(native?nullptr:combat,std::memory_order_release);
    // Reconcile once on adoption or a policy transition. No repeated detach
    // repair, native lock broadcast or per-frame attachment writes.
    if((!cameraInitialized||nativeCamera!=native)&&(field<uint8_t>(combat,0x8e)&0x10)){
        cameraInitialized=true;nativeCamera=native;
        if(native){
            at<void(*)(void*)>(Build::AttachDirect)(combat);
        }else if(!field<uint8_t>(combat,0x137a)){
            at<void(*)(void*)>(Build::DetachCamera)(combat);
            if(settings.debugLogging)cameraMetrics.initialDetaches.fetch_add(1,std::memory_order_relaxed);
        }
    }
    if(!cameraContext&&field<uint8_t>(combat,0xb28)==11)return false;
    return true;
}
bool eligible(void* combat,bool acquiring=false) {
    if(!managed(combat)||(!playerLocked&&!acquiring)||!(field<uint8_t>(combat,0x8e)&0x10)||
       field<uint8_t>(combat,0x1612))return false;
    auto action=field<uint8_t>(combat,0xb28);
    return action!=1&&action!=2&&action!=10&&action!=11&&action!=12;
}
using Tick=void(*)(void*,float);Tick originalTick{};
using Switch=bool(*)(void*,uint8_t,float,bool,bool,bool,bool,bool);Switch originalSwitch{};
using Pick=void*(*)(void*);Pick originalPick{};
using SetTarget=void(*)(void*,void*);SetTarget originalSetTarget{};
using Request=void(*)(void*);Request originalRequest{};
using Lock=void(*)(void*,bool);Lock originalLock{};
using LoseLock=void(*)(void*,void*,float);LoseLock originalLoseLock{};
using ReactToHit=void(*)(void*,void*,void*,void*);ReactToHit originalReactToHit{};
using Script=void(*)(void*,void*,void*);Script originalLockScript{},originalSwitchScript{};
using ActionTarget=bool(*)(void*);ActionTarget originalActionTarget{};
SetTarget originalAttackTarget{};
using CanAbility=bool(*)(void*,void*,void*,uint8_t*,bool*);CanAbility originalCanAbility{};
using PlanAbility=bool(*)(void*,void*);PlanAbility originalPlanAbility{};
using FocusActor=void*(*)(void*);FocusActor originalFocusActor{};
using Draw=void(*)(void*);Draw originalDraw{};
struct InputValue{Vec3 value;int type;int padding;};static_assert(sizeof(InputValue)==32);
using Modify=InputValue*(*)(void*,InputValue*,void*,const InputValue*,float);Modify originalModify{};
using ViewRotation=void(*)(void*,float,Vec3*,Vec3*);ViewRotation originalViewRotation{};
using SettingQuery=bool(*)(void*,uint8_t,void*);SettingQuery originalSettingQuery{};
struct InputRoute{bool gamepad{},mouse{};};
thread_local InputRoute* inputRoute{};
thread_local void* selecting{};
thread_local void* coneContext{};
thread_local bool automaticRequest{};
thread_local bool fallbackRequest{};
thread_local bool inRequest{};
thread_local void* lockButton{};
struct TemporaryLoss {void* combat;float duration;};
thread_local const TemporaryLoss* temporaryLoss{};
thread_local void* recovering{};
thread_local uint64_t recoveryIntent{};
thread_local void* attackerSelecting{};
thread_local bool autoAcquiring{},nearestSelecting{},nearestFallback{};
thread_local uint64_t acquireIntent{};
thread_local Identity nearestTargetId,nearestActorId;
// Capture-only selection, requested only after an ability needs an enemy.
struct FocusQuery {void* combat;Identity target;};
thread_local FocusQuery* focusQuery{};
struct AbilityPlan {void* focus;Identity actor;};
thread_local AbilityPlan* abilityPlan{};
thread_local bool checkingAbility{};
struct Metrics {
    uint64_t requests{},pickCalls{},candidates{},dwellChecks{},skipped{},draw{},assist{},ticks{},micros{};
    uint64_t lockChanges{},blockedTargets{};
    uint64_t cameraDirections{},directionFallbacks{},directionMicros{};
    uint64_t trackingAttempts{},trackingSteps{},trackingMoves{},trackingInput{},trackingMicros{};
    uint64_t temporaryLosses{},targetRecoveries{},recoveryMisses{},trackedClears{},recoveryAttempts{},recoveryMicros{};
    uint64_t attackerHits{},attackerSwitches{},attackerRejected{},attackerOverrides{},attackerMicros{};
    uint64_t autoLocks{},targetDeaths{},deathSearches{},deathSwitches{},deathMisses{},deathMicros{};
    uint64_t focusQueries{},focusTargets{},focusMicros{};
    uint64_t since{};
} metrics;
uint64_t clockMicros(){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return static_cast<uint64_t>(t.QuadPart/f.QuadPart)*1000000+static_cast<uint64_t>(t.QuadPart%f.QuadPart)*1000000/f.QuadPart;}
void report(uint64_t now) {
    if(!settings.debugLogging)return;
    if(!metrics.since){metrics.since=now;return;}
    if(now-metrics.since<10000)return;
    auto message=L"[CombatCamera] 10s: requests="+std::to_wstring(metrics.requests)+L" picker="+std::to_wstring(metrics.pickCalls)+
        L" candidates="+std::to_wstring(metrics.candidates)+L" dwellChecks="+std::to_wstring(metrics.dwellChecks)+
        L" budgetSkips="+std::to_wstring(metrics.skipped)+L" camera="+std::to_wstring(cameraMetrics.accepted.exchange(0))+
        L" cameraChecks="+std::to_wstring(cameraMetrics.checks.exchange(0))+L" cameraOtherThread="+std::to_wstring(cameraMetrics.otherThread.exchange(0))+
        L" attachPrevented="+std::to_wstring(cameraMetrics.attachPrevented.exchange(0))+
        L" initialDetaches="+std::to_wstring(cameraMetrics.initialDetaches.exchange(0))+
        L" crosshair="+std::to_wstring(metrics.draw)+L" assist="+std::to_wstring(metrics.assist)+
        L" lockChanges="+std::to_wstring(metrics.lockChanges)+L" blockedTargets="+std::to_wstring(metrics.blockedTargets)+
        L" cameraDirections="+std::to_wstring(metrics.cameraDirections)+L" directionFallbacks="+std::to_wstring(metrics.directionFallbacks)+
        L" focusQueries="+std::to_wstring(metrics.focusQueries)+L" focusTargets="+std::to_wstring(metrics.focusTargets)+
        L" focusUs="+std::to_wstring(metrics.focusMicros)+
        L" directionUs="+std::to_wstring(metrics.directionMicros)+
        L" cameraMode="+std::to_wstring(settings.cameraMode)+L" trackingSteps="+std::to_wstring(metrics.trackingSteps)+
        L" viewChecks="+std::to_wstring(cameraMetrics.viewChecks.exchange(0))+L" viewOtherThread="+std::to_wstring(cameraMetrics.viewOtherThread.exchange(0))+
        L" trackingUnavailable="+std::to_wstring(metrics.trackingAttempts-metrics.trackingSteps)+
        L" trackingMoves="+std::to_wstring(metrics.trackingMoves)+L" trackingInput="+std::to_wstring(metrics.trackingInput)+
        L" trackingUs="+std::to_wstring(metrics.trackingMicros)+
        L" temporaryLosses="+std::to_wstring(metrics.temporaryLosses)+L" targetRecoveries="+std::to_wstring(metrics.targetRecoveries)+
        L" recoveryMisses="+std::to_wstring(metrics.recoveryMisses)+
        L" trackedClears="+std::to_wstring(metrics.trackedClears)+L" recoveryAttempts="+std::to_wstring(metrics.recoveryAttempts)+
        L" recoveryUs="+std::to_wstring(metrics.recoveryMicros)+
        L" targeting="+(playerLocked?(settings.targeting?std::wstring(L"camera"):std::wstring(L"fixed")):std::wstring(L"off"))+
        L" attackerHits="+std::to_wstring(metrics.attackerHits)+L" attackerSwitches="+std::to_wstring(metrics.attackerSwitches)+
        L" attackerRejected="+std::to_wstring(metrics.attackerRejected)+L" attackerOverrides="+std::to_wstring(metrics.attackerOverrides)+L" attackerUs="+std::to_wstring(metrics.attackerMicros)+
        L" autoLocks="+std::to_wstring(metrics.autoLocks)+L" targetDeaths="+std::to_wstring(metrics.targetDeaths)+
        L" deathSearches="+std::to_wstring(metrics.deathSearches)+L" deathSwitches="+std::to_wstring(metrics.deathSwitches)+
        L" deathMisses="+std::to_wstring(metrics.deathMisses)+L" deathUs="+std::to_wstring(metrics.deathMicros)+
        L" selectionUs="+std::to_wstring(metrics.micros)+L"\n";
    RC::Output::send(message);metrics={};metrics.since=now;
}
Vec3 cameraVector(void* camera,size_t slot) {
    Vec3 out{};auto value=method<Vec3*(*)(void*,Vec3*)>(camera,slot)(camera,&out);
    return value?*value:out;
}
bool readCameraVector(void* camera,size_t slot,Vec3& out) {
    const auto getter=method<Vec3*(*)(void*,Vec3*)>(camera,slot);
    const auto value=getter?getter(camera,&out):nullptr;
    if(!value)return false;
    out=*value;return finite(out);
}
void smoothView(void* camera,float delta,Vec3* view,Vec3* input) {
    syncSession();
    auto combat=reinterpret_cast<void*>(ownerId.address);
    if(!sameCurrent(combat,ownerId)||!managed(combat)||!playerLocked||
       !(field<uint8_t>(combat,0x8e)&0x10)||!view||!input||!finite(*view)||!finite(*input)){
        clearTracking();abandonRecovery();return;
    }
    auto pawn=field<void*>(combat,0xa8),pc=field<void*>(pawn,0x2e8);
    if(field<void*>(pc,0x370)!=camera||field<void*>(camera,0)!=at<void*>(Build::PlayerCameraVtable)){
        clearTracking();return;
    }
    if(recoveryRemaining&&(input->x!=0.0||input->y!=0.0)){recoveryHadLook=true;recoveryLookAt=GetTickCount64();}
    auto target=field<void*>(combat,0x1380);
    const auto targetId=identity(target);
    if(!targetId.address){clearTracking();return;}
    auto actor=field<void*>(target,0xa8);const auto actorId=identity(actor);
    if(!actorId.address){clearTracking();return;}
    if(targetId!=trackingTargetId||actorId!=trackingActorId){
        const double quiet=trackingTargetId.address?tracking.quiet:settings.trackingResumeMs*0.001;
        clearTracking();tracking.quiet=quiet;trackingTargetId=targetId;trackingActorId=actorId;
        session.watched[4]=targetId.index;session.watched[5]=actorId.index;
    }
    // Controller RotationInput is after native deadzones and before camera
    // modifiers. Optional slowdown is strictly positive, preserving the exact
    // input/noninput distinction. This also covers mouse and fixed-target input.
    const bool manual=input->x!=0.0||input->y!=0.0;
    if(manual)recoveryHold=0;
    else if(recoveryHold){
        recoveryHold=std::isfinite(delta)&&delta>0&&delta<=0.25f?std::max(0.0,recoveryHold-delta):0;
    }
    Vec3 desired=*view;
    if(!manual){
        Vec3 point{};auto getPoint=method<Vec3*(*)(void*,Vec3*,void*)>(actor,0x6f0);
        auto value=getPoint?getPoint(actor,&point,pawn):nullptr;
        if(!value){clearTracking();return;}point=*value;
        Vec3 origin{},current{};
        if(!readCameraVector(camera,0x828,origin)||!readCameraVector(camera,0x820,current)){clearTracking();return;}
        Vec3 offset{point.x-origin.x,point.y-origin.y,point.z-origin.z};
        const double planar=std::hypot(offset.x,offset.y),distance=std::hypot(planar,offset.z);
        if(!finite(point)||!finite(origin)||!finite(current)||!std::isfinite(distance)||distance<1e-4){clearTracking();return;}
        constexpr double degrees=180.0/3.14159265358979323846;
        const double pitch=std::atan2(offset.z,planar)*degrees;
        const double yaw=planar>1e-6?std::atan2(offset.y,offset.x)*degrees:current.y;
        // Correct actual camera error without snapping ControlRotation to the
        // camera-stack offset. Native modifiers and limits run afterwards.
        desired.x=view->x+angleDelta(pitch-current.x);
        desired.y=view->y+angleDelta(yaw-current.y);
        if(recoveryHold&&std::hypot(angleDelta(desired.x-view->x),angleDelta(desired.y-view->y))<=10.0)recoveryHold=0;
    }
    const auto correction=tracking.step(*view,desired,delta,manual,settings.trackingSpeed,settings.trackingResumeMs,settings.trackingCatchup);
    input->x+=correction.x;input->y+=correction.y;
    if(settings.debugLogging){++metrics.trackingSteps;if(manual)++metrics.trackingInput;if(correction.x||correction.y)++metrics.trackingMoves;}
}
void viewRotation(void* camera,float delta,Vec3* view,Vec3* input) {
    // Worker calls and inactive modes perform no new object reads. All tracking
    // state belongs to the game thread; the worker camera gates stay opaque.
    if(cameraLogging.load(std::memory_order_relaxed)){
        cameraMetrics.viewChecks.fetch_add(1,std::memory_order_relaxed);
        if(GetCurrentThreadId()!=gameThread)cameraMetrics.viewOtherThread.fetch_add(1,std::memory_order_relaxed);
    }
    if(live()&&playerLocked&&
       reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase==Build::ViewRotationReturn){
        // Observe manual look before Smooth adds its own correction. Only the
        // current player's camera can arm an override, in all camera modes.
        if(camera&&settings.targeting&&attackerHeldTargetId.address&&!attackerLookOverride&&
           !attackerRemaining&&!recoveryRemaining&&view&&finite(*view)&&input&&finite(*input)&&
           std::isfinite(delta)&&delta>0&&delta<=0.25f&&
           (input->x!=0.0||input->y!=0.0)){
            auto combat=resolve(ownerId);
            if(combat&&managed(combat)&&attackerHeldTargetId.address){
                auto pawn=field<void*>(combat,0xa8),pc=field<void*>(pawn,0x2e8);
                if(field<void*>(pc,0x370)==camera&&field<void*>(camera,0)==at<void*>(Build::PlayerCameraVtable)){
                    clearAttackerLook();attackerLookOverride=true;nextFallback=false;
                }
            }
        }
        if(settings.cameraMode==1){
            const auto before=settings.debugLogging?clockMicros():0;
            if(settings.debugLogging)++metrics.trackingAttempts;
            smoothView(camera,delta,view,input);
            if(settings.debugLogging)metrics.trackingMicros+=clockMicros()-before;
        }
    }
    originalViewRotation(camera,delta,view,input);
}
void updateAssist(void* combat,uint64_t now) {
    assistScale=1;assistTargetId={};assistAt=0;session.watched[2]=-1;
    if(!playerLocked||!settings.targeting||!settings.aimAssist||!settings.assistStrength)return;
    auto targetComponent=field<void*>(combat,0x1380);
    if(!targetComponent)return;
    auto target=field<void*>(targetComponent,0xa8),pawn=field<void*>(combat,0xa8);
    if(!target||!pawn)return;
    auto pc=field<void*>(pawn,0x2e8);auto camera=pc?field<void*>(pc,0x370):nullptr;
    if(!camera)return;
    auto id=identity(target);if(!id.address)return;
    Vec3 point{};method<Vec3*(*)(void*,Vec3*,void*)>(target,0x6f0)(target,&point,pawn);
    auto location=cameraVector(camera,0x828),rotation=cameraVector(camera,0x820);
    if(!finite(location)||!finite(rotation)||!finite(point))return;
    assistScale=slowdown(directionDot(location,forward(rotation),point),settings.assistStrength);
    assistTargetId=id;assistAt=now;session.watched[2]=id.index;
}
void request(void* combat) {
    syncSession();
    if(inRequest||!settings.targeting||recoveryRemaining||recoveryHold||attackerRemaining||
       (attackerHeldTargetId.address&&!attackerLookOverride)||!eligible(combat))return;
    auto config=field<void*>(combat,0x9b8);if(!config)return;
    float parameter=field<float>(config,0x26c);
    if(!std::isfinite(parameter)||parameter<=0||parameter>=1000000)return;
    const auto now=GetTickCount64();
    if(!requestBudget.take(now)){if(settings.debugLogging)++metrics.skipped;return;}
    auto id=identity(combat);if(!id.address)return;
    inRequest=true;automaticRequest=true;fallbackRequest=nextFallback;nextFallback=false;auto previous=selecting;selecting=combat;
    uint64_t before=settings.debugLogging?clockMicros():0;
    if(settings.debugLogging)++metrics.requests;
    originalSwitch(combat,2,parameter,false,false,false,false,false);
    selecting=previous;automaticRequest=false;fallbackRequest=false;inRequest=false;
    updateAssist(combat,now);
    if(settings.debugLogging)metrics.micros+=clockMicros()-before;
}
void clearTarget(void* combat,bool preserveAutomatic=false) {
    clearAttackerHeld();
    if(!preserveAutomatic){clearAttackerPending();clearDeath();}
    clearRecovery();
    if(trackingTargetId.address)clearTracking();
    // The native setter removes delegates and publishes the target change.
    if(field<void*>(combat,0x1380)){originalSetTarget(combat,nullptr);clearAttackPending=true;}
    if(clearAttackPending){originalAttackTarget(combat,nullptr);clearAttackPending=false;}
    if((field<uint8_t>(combat,0x14a9)!=0)!=playerLocked)originalLock(combat,playerLocked);
    assistScale=1;assistAt=0;dwell.clear();nextFallback=false;
}
void reactToHit(void* combat,void* animation,void* attack,void* response) {
    // PlayerCombatComponent::ReactToHit: R8 is InAttackData, whose +0x28
    // owns the attacker's combat component. Native reactions include block,
    // parry and omniblock. No health-loss threshold or borrowed struct survives.
    if(live()&&attack){
        const bool attacker=settings.lockLastAttacker||settings.autoLockOnHit;
        if(attacker&&managed(combat)&&(field<uint8_t>(combat,0x8e)&0x10)){
            if(attacker&&(playerLocked||settings.autoLockOnHit)){
                auto target=field<void*>(attack,0x28);const auto targetId=identity(target);
                auto actor=targetId.address?field<void*>(target,0xa8):nullptr;const auto actorId=identity(actor);
                if(targetId.address&&actorId.address&&target!=combat&&actor!=field<void*>(combat,0xa8)){
                    clearDeath();clearAttackerLook();nextFallback=false;
                    attackerPendingTargetId=targetId;attackerPendingActorId=actorId;attackerRemaining=1.0;
                    session.watched[8]=targetId.index;session.watched[9]=actorId.index;
                    if(settings.debugLogging)++metrics.attackerHits;
                }
            }
        }
    }
    originalReactToHit(combat,animation,attack,response);
}
void applyAttacker(void* combat,float delta) {
    if(!attackerRemaining)return;
    if((!settings.lockLastAttacker&&!settings.autoLockOnHit)||(!playerLocked&&!settings.autoLockOnHit)||!std::isfinite(delta)||delta<0||delta>=attackerRemaining){
        if(settings.debugLogging)++metrics.attackerRejected;
        clearAttackerPending();return;
    }
    attackerRemaining-=delta;
    if(inRequest||!eligible(combat,settings.autoLockOnHit))return;
    const auto targetId=attackerPendingTargetId,actorId=attackerPendingActorId;
    auto target=resolveAttacker(targetId),actor=resolveAttacker(actorId);
    if(!target||!actor||field<void*>(target,0xa8)!=actor){clearAttackerPending();return;}
    if(playerLocked&&field<void*>(combat,0x1380)==target){
        attackerHeldTargetId=identity(target);attackerHeldActorId=identity(actor);
        session.watched[10]=targetId.index;session.watched[11]=actorId.index;
        clearAttackerPending();return;
    }
    auto config=field<void*>(combat,0x9b8);const float distance=config?field<float>(config,0x26c):0;
    if(!std::isfinite(distance)||distance<=0||distance>=1000000){clearAttackerPending();return;}
    if(!requestBudget.take(GetTickCount64()))return;
    clearAttackerPending(); // A newer nested hit must remain pending.
    const auto before=settings.debugLogging?clockMicros():0;
    const auto expectedOwner=ownerId;
    attackerSelectionInvalidated=false;
    session.watched[12]=targetId.index;session.watched[13]=actorId.index;
    const auto oldSelecting=selecting,oldAttacker=attackerSelecting;
    const bool oldAcquiring=autoAcquiring;const auto oldIntent=acquireIntent;
    autoAcquiring=settings.autoLockOnHit;acquireIntent=lockIntent;
    const bool wasRequest=inRequest;selecting=combat;attackerSelecting=actor;inRequest=true;
    originalSwitch(combat,2,distance,false,false,false,false,false);
    selecting=oldSelecting;attackerSelecting=oldAttacker;inRequest=wasRequest;
    autoAcquiring=oldAcquiring;acquireIntent=oldIntent;
    syncSession();
    session.watched[12]=-1;session.watched[13]=-1;
    if(!attackerSelectionInvalidated&&ownerId==expectedOwner&&resolve(expectedOwner)==combat&&
       live()&&(settings.lockLastAttacker||settings.autoLockOnHit)&&playerLocked&&field<void*>(combat,0x1380)==target&&resolveAttacker(targetId)&&resolveAttacker(actorId)){
        clearRecovery();attackerHeldTargetId=identity(target);attackerHeldActorId=identity(actor);
        session.watched[10]=targetId.index;session.watched[11]=actorId.index;
        dwell.clear();nextFallback=false;
        if(settings.debugLogging)++metrics.attackerSwitches;
    }else if(settings.debugLogging)++metrics.attackerRejected;
    if(settings.debugLogging)metrics.attackerMicros+=clockMicros()-before;
}
void applyDeath(void* combat,float delta) {
    if(!deathRemaining)return;
    if(!settings.autoLockOnHit||settings.afterTargetDeath!=0||playerLocked||attackerRemaining||
       !std::isfinite(delta)||delta<0||delta>=deathRemaining){
        if(settings.debugLogging)++metrics.deathMisses;
        clearDeath();return;
    }
    deathRemaining-=delta;
    if(inRequest||!eligible(combat,true))return;
    auto config=field<void*>(combat,0x9b8);const float distance=config?field<float>(config,0x26c):0;
    if(!std::isfinite(distance)||distance<=0||distance>=1000000){clearDeath();return;}
    if(!requestBudget.take(GetTickCount64()))return;
    const auto expectedOwner=ownerId;const auto expectedIntent=lockIntent;
    const bool fallback=deathFallback;
    const auto before=settings.debugLogging?clockMicros():0;
    if(settings.debugLogging)++metrics.deathSearches;
    const auto oldSelecting=selecting;const auto oldIntent=acquireIntent;
    const bool wasRequest=inRequest,oldAcquiring=autoAcquiring;
    selecting=combat;inRequest=true;autoAcquiring=true;acquireIntent=expectedIntent;
    nearestSelecting=true;nearestFallback=fallback;nearestTargetId={};nearestActorId={};
    attackerSelectionInvalidated=false;
    originalSwitch(combat,2,distance,false,false,false,false,false);
    nearestSelecting=false;nearestFallback=false;
    selecting=oldSelecting;inRequest=wasRequest;autoAcquiring=oldAcquiring;acquireIntent=oldIntent;
    syncSession();session.watched[12]=-1;session.watched[13]=-1;
    if(!attackerSelectionInvalidated&&expectedOwner==ownerId&&expectedIntent==lockIntent&&
       resolve(expectedOwner)==combat&&live()&&settings.autoLockOnHit&&
       playerLocked&&nearestTargetId.address&&resolveAttacker(nearestTargetId)&&resolveAttacker(nearestActorId)&&
       field<void*>(combat,0x1380)==reinterpret_cast<void*>(nearestTargetId.address)){
        clearDeath();attackerHeldTargetId=identity(reinterpret_cast<void*>(nearestTargetId.address));
        attackerHeldActorId=identity(reinterpret_cast<void*>(nearestActorId.address));
        session.watched[10]=attackerHeldTargetId.index;session.watched[11]=attackerHeldActorId.index;
        clearAttackerLook();nextFallback=false;
        if(settings.debugLogging)++metrics.deathSwitches;
    }else if(deathRemaining){
        if(fallback){clearDeath();if(settings.debugLogging)++metrics.deathMisses;}
        else deathFallback=true;
    }
    nearestTargetId={};nearestActorId={};
    if(settings.debugLogging)metrics.deathMicros+=clockMicros()-before;
}
bool applyRecovery(void* combat) {
    if(!recoveryRemaining||!recoveryReady||recoveryRetry||inRequest||attackerRemaining)return false;
    const auto targetId=recoveryTargetId,actorId=recoveryActorId,expectedOwner=ownerId;
    auto target=resolveAttacker(targetId),actor=resolveAttacker(actorId);
    if(settings.cameraMode!=1||!playerLocked||field<void*>(combat,0x1380)||
       !target||!actor||field<void*>(target,0xa8)!=actor||field<uint8_t>(target,0xb28)==10){
        if(settings.debugLogging)++metrics.recoveryMisses;
        abandonRecovery();return false;
    }
    // Readiness and transient native rejection may outlast the disappearance
    // timer. Retry only this identity, at most every 100 gameplay milliseconds,
    // sharing the existing 50ms selection budget and finite loss deadline.
    if(!eligible(combat))return false;
    auto config=field<void*>(combat,0x9b8);const float distance=config?field<float>(config,0x26c):0;
    if(!std::isfinite(distance)||distance<=0||distance>=1000000){abandonRecovery();return false;}
    if(!requestBudget.take(GetTickCount64()))return false;
    const auto now=GetTickCount64(),lastLook=recoveryLookAt;const bool hadLook=recoveryHadLook;
    const auto before=settings.debugLogging?clockMicros():0;
    if(settings.debugLogging)++metrics.recoveryAttempts;
    recoveryRetry=0.1;
    const auto oldSelecting=selecting,oldRecovering=recovering;
    const auto oldIntent=recoveryIntent;const bool wasRequest=inRequest;
    selecting=combat;recovering=actor;recoveryIntent=lockIntent;inRequest=true;
    const auto expectedIntent=lockIntent;
    originalSwitch(combat,2,distance,false,false,false,false,false);
    selecting=oldSelecting;recovering=oldRecovering;recoveryIntent=oldIntent;inRequest=wasRequest;
    syncSession();
    const bool restored=live()&&settings.cameraMode==1&&playerLocked&&expectedIntent==lockIntent&&
        expectedOwner==ownerId&&resolve(expectedOwner)==combat&&recoveryRemaining&&
        recoveryTargetId==targetId&&recoveryActorId==actorId&&
        resolveAttacker(targetId)&&resolveAttacker(actorId)&&field<void*>(combat,0x1380)==target;
    if(restored){
        // Native selection may assign the initial serial. The deletion watches
        // remain armed throughout; capture current serials only after success.
        const auto currentTarget=identity(target),currentActor=identity(actor);
        clearRecovery();clearTracking();trackingTargetId=currentTarget;trackingActorId=currentActor;
        session.watched[4]=currentTarget.index;session.watched[5]=currentActor.index;
        tracking.quiet=hadLook?(now>=lastLook?(now-lastLook)*0.001:0):settings.trackingResumeMs*0.001;
        // Give the camera time to face the restored enemy before camera-directed
        // selection can lose it again. Manual look releases this immediately.
        recoveryHold=hadLook?0:3.0;
        if(attackerHeldTargetId.address){attackerHeldTargetId=currentTarget;attackerHeldActorId=currentActor;}
        if(settings.debugLogging)++metrics.targetRecoveries;
    }
    if(settings.debugLogging)metrics.recoveryMicros+=clockMicros()-before;
    return restored;
}
void tick(void* pawn,float delta) {
    auto combat=live()&&pawn?field<void*>(pawn,0xc90):nullptr;
    if(managed(combat)){
        if(recoveryRemaining){
            // Native recovery timers use gameplay time, including time dilation.
            // Only an outstanding loss adds this scalar watchdog work.
            if(!std::isfinite(delta)||delta<0||delta>=recoveryRemaining){
                if(settings.debugLogging)++metrics.recoveryMisses;
                abandonRecovery();
            }else{recoveryRemaining-=delta;recoveryRetry=std::max(0.0,recoveryRetry-delta);}
        }
        if(!(field<uint8_t>(combat,0x8e)&0x10)){
            if(playerLocked){playerLocked=false;clearAttackPending=true;}
            clearAttacker();clearDeath();
        }
        if(!playerLocked)clearTarget(combat,settings.autoLockOnHit);
    }
    originalTick(pawn,delta);
    if(!live())return;
    syncSession();
    combat=field<void*>(pawn,0xc90);
    if(managed(combat)){
        applyRecovery(combat);
        if(!playerLocked)clearTarget(combat,settings.autoLockOnHit);
        else{
            if(attackerHeldTargetId.address&&!recoveryRemaining&&reinterpret_cast<uintptr_t>(field<void*>(combat,0x1380))!=attackerHeldTargetId.address)clearAttackerHeld();
        }
        applyAttacker(combat,delta);
        applyDeath(combat,delta);
    }
    if(settings.targeting&&!attackerRemaining&&(!attackerHeldTargetId.address||attackerLookOverride)&&gameplay(pawn))request(combat);
    else if(settings.targeting&&settings.aimAssist&&eligible(combat)){
        auto now=GetTickCount64();if(assistBudget.take(now))updateAssist(combat,now);
    }else{assistScale=1;dwell.clear();}
    if(settings.debugLogging)report(GetTickCount64());
}
bool switchTarget(void* combat,uint8_t mode,float distance,bool a,bool b,bool c,bool d,bool e) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
    if(!managed(combat))return originalSwitch(combat,mode,distance,a,b,c,d,e);
    if(caller==Build::RegainLockReturn&&recoveryRemaining){
        recoveryReady=true;
        return applyRecovery(combat);
    }
    // Native look/next/previous and threat/ability reacquisition never get to
    // replace a manual selection or create an unlocked soft target.
    if(lockButton!=combat)return false;
    lockButton=nullptr;
    ++lockIntent;clearDeath();clearAttackerPending();
    playerLocked=!playerLocked;
    if(settings.debugLogging)++metrics.lockChanges;
    clearAttackPending=!playerLocked;
    if(!playerLocked){clearTarget(combat);return false;}
    originalLock(combat,true);
    auto previous=selecting;const auto wasRequest=inRequest;inRequest=true;
    selecting=combat;
    const auto result=originalSwitch(combat,2,distance,false,false,false,false,false);
    selecting=previous;inRequest=wasRequest;
    if(!field<void*>(combat,0x1380)&&!settings.targeting){playerLocked=false;originalLock(combat,false);}
    return result;
}
struct List{void** data;int size,capacity;};static_assert(sizeof(List)==16);
void* pick(void* context) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
    if(!live()||!selecting||(caller!=Build::PickerReturn1&&caller!=Build::PickerReturn2)||field<void*>(context,0x48)!=field<void*>(selecting,0xa8))return originalPick(context);
    // Spread the game's primary/fallback full searches across budget slots.
    // Native/manual requests still execute both immediately. An existing target
    // can be checked alone while waiting, avoiding a visible target clear.
    if(automaticRequest&&fallbackRequest&&caller==Build::PickerReturn1)return nullptr;
    const bool retainOnly=automaticRequest&&!fallbackRequest&&caller==Build::PickerReturn2;
    auto candidates=field<List*>(context,0x58);
    if(!candidates||candidates->size<0||candidates->size>candidates->capacity||(!candidates->data&&candidates->size))return nullptr;
    auto previous=field<void*>(context,0x50);auto flags=field<std::array<uint8_t,6>>(context,0x60);
    if(retainOnly&&!previous)return nullptr;
    struct Restore {
        void* context;void* previous;std::array<uint8_t,6> flags;List* list;void* oldCone;
        ~Restore(){field<void*>(context,0x50)=previous;field<std::array<uint8_t,6>>(context,0x60)=flags;field<List*>(context,0x58)=list;coneContext=oldCone;}
    } restore{context,previous,flags,candidates,coneContext};
    coneContext=context;
    field<void*>(context,0x50)=nullptr;
    field<uint8_t>(context,0x61)=1;field<uint8_t>(context,0x62)=2;
    field<uint8_t>(context,0x63)=0;field<uint8_t>(context,0x64)=1;field<uint8_t>(context,0x65)=0;
    if(nearestSelecting){
        // One native pass per budget slot, with fallback deferred. Distance is
        // measured from the pawn; the score gate changes only candidate rank.
        if((caller==Build::PickerReturn2)!=nearestFallback)return nullptr;
        field<uint8_t>(context,0x61)=0;field<uint8_t>(context,0x64)=0;
        if(settings.debugLogging){++metrics.pickCalls;metrics.candidates+=candidates->size;}
        return originalPick(context);
    }
    if(recovering||attackerSelecting){
        // No search outside the native distance-filtered list, and no substitute
        // enemy if the remembered one is dead, unavailable or occluded.
        auto wanted=recovering?recovering:attackerSelecting;
        bool present=false;for(int i=0;i<candidates->size;++i)if(candidates->data[i]==wanted){present=true;break;}
        if(!present)return nullptr;
        void* actor=wanted;List singleton{&actor,1,1};field<List*>(context,0x58)=&singleton;
        field<uint8_t>(context,0x61)=0; // Verified native angular-rejection gate only.
        if(settings.debugLogging)++metrics.dwellChecks;
        return originalPick(context)==actor?actor:nullptr;
    }
    List waiting{&previous,1,1};
    if(retainOnly){field<List*>(context,0x58)=&waiting;if(settings.debugLogging)++metrics.dwellChecks;}
    else if(settings.debugLogging){++metrics.pickCalls;metrics.candidates+=candidates->size;}
    void* chosen=originalPick(context);
    if(automaticRequest&&!fallbackRequest&&!retainOnly&&!chosen)nextFallback=true;
    if(automaticRequest&&attackerLookOverride&&reinterpret_cast<uintptr_t>(previous)==attackerHeldActorId.address){
        // Hover switching must finish its dwell even when the attacker is now
        // behind the camera. Retain only an in-range, natively eligible attacker;
        // omit the angular test for retention, never for the replacement.
        if(chosen&&chosen!=previous){
            const auto nextId=identity(chosen);
            if(!nextId.address)return nullptr;
            if(nextId!=pendingId){dwell.clear();pendingId=nextId;session.watched[3]=nextId.index;}
            if(dwell.ready(reinterpret_cast<uintptr_t>(selecting),reinterpret_cast<uintptr_t>(chosen),GetTickCount64(),settings.delayMs))return chosen;
        }else{
            dwell.clear();pendingId={};session.watched[3]=-1;
            if(chosen==previous||fallbackRequest)clearAttackerLook();
        }
        bool present=false;for(int i=0;i<candidates->size;++i)if(candidates->data[i]==previous){present=true;break;}
        if(!present)return chosen;
        field<List*>(context,0x58)=&waiting;field<uint8_t>(context,0x61)=0;
        if(settings.debugLogging)++metrics.dwellChecks;
        return originalPick(context)==previous?previous:chosen;
    }
    if(!automaticRequest||!previous||!chosen||chosen==previous){dwell.clear();pendingId={};session.watched[3]=-1;return chosen;}
    auto nextId=identity(chosen);
    if(!nextId.address)return nullptr;
    if(nextId!=pendingId){dwell.clear();pendingId=nextId;session.watched[3]=nextId.index;}
    if(dwell.ready(reinterpret_cast<uintptr_t>(selecting),reinterpret_cast<uintptr_t>(chosen),GetTickCount64(),settings.delayMs))return chosen;
    // Validate retention through the game's predicate and visibility checks;
    // the temporary list contains only the current target, never a full rescan.
    List singleton{&previous,1,1};field<List*>(context,0x58)=&singleton;
    if(settings.debugLogging)++metrics.dwellChecks;
    return originalPick(context)?previous:chosen;
}
void setTarget(void* combat,void* target) {
    // SwitchLockTarget already performs native range, visibility and eligibility
    // checks and assigns a native weak serial before calling this setter.
    if(focusQuery&&focusQuery->combat==combat){focusQuery->target=identity(target);return;}
    // A cancelled automatic request must not fall through as an ordinary
    // native target write after a nested Apply, unlock or owner invalidation.
    if(autoAcquiring&&(!live()||acquireIntent!=lockIntent||!settings.autoLockOnHit))return;
    const bool local=managed(combat,target==nullptr&&settings.autoLockOnHit);
    if(autoAcquiring&&!local)return;
    if(recovering&&selecting==combat&&(!local||!live()||settings.cameraMode!=1||!playerLocked||
       recoveryIntent!=lockIntent||!recoveryRemaining||
       (target&&(resolveAttacker(recoveryTargetId)!=target||resolveAttacker(recoveryActorId)!=recovering))))return;
    if(local){
        if(target&&(selecting!=combat||(!playerLocked&&!autoAcquiring)||
           (autoAcquiring&&(acquireIntent!=lockIntent||!settings.autoLockOnHit)))){
            if(settings.debugLogging)++metrics.blockedTargets;return;
        }
        if(target&&nearestSelecting){
            nearestTargetId=identity(target);
            nearestActorId=nearestTargetId.address?identity(field<void*>(target,0xa8)):Identity{};
            if(!nearestTargetId.address||!nearestActorId.address)return;
            session.watched[12]=nearestTargetId.index;session.watched[13]=nearestActorId.index;
        }
        if(target&&autoAcquiring&&!playerLocked){
            playerLocked=true;originalLock(combat,true);
            if(settings.debugLogging)++metrics.autoLocks;
        }
        const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
        bool temporary=false;
        auto previous=field<void*>(combat,0x1380);
        const bool dead=!target&&previous&&playerLocked&&
            identity(previous).address&&field<uint8_t>(previous,0xb28)==10;
        if(!dead&&!target&&playerLocked&&settings.cameraMode==1&&temporaryLoss&&temporaryLoss->combat==combat&&caller==Build::LoseLockClearReturn){
            auto previous=field<void*>(combat,0x1380);const auto id=identity(previous);
            auto actor=id.address?field<void*>(previous,0xa8):nullptr;const auto actorId=identity(actor);
            const float duration=temporaryLoss->duration;
            if(id.address&&actorId.address&&std::isfinite(duration)&&duration>0&&duration<=60){
                clearRecovery();recoveryTargetId=id;recoveryActorId=actorId;
                recoveryRemaining=duration+3.0;
                rememberRecoveryLook();
                session.watched[6]=id.index;session.watched[7]=actorId.index;temporary=true;
                if(settings.debugLogging)++metrics.temporaryLosses;
            }
        }
        // Some enemy abilities clear their target without LoseTargetLock. Only
        // remember a living enemy that Smooth was actually following, never a
        // new candidate, death, explicit unlock, or a failed special-loss call.
        if(!dead&&!target&&previous&&playerLocked&&settings.cameraMode==1&&!temporaryLoss&&
           sameCurrent(previous,trackingTargetId)){
            auto actor=field<void*>(previous,0xa8);
            if(sameCurrent(actor,trackingActorId)){
                const auto id=trackingTargetId,actorId=trackingActorId;
                clearRecovery();recoveryTargetId=id;recoveryActorId=actorId;
                recoveryRemaining=3.0;recoveryReady=true;temporary=true;
                rememberRecoveryLook();
                session.watched[6]=id.index;session.watched[7]=actorId.index;
                if(settings.debugLogging)++metrics.trackedClears;
            }
        }
        if((target&&!recovering)||(!target&&!temporary&&field<void*>(combat,0x1380)))clearRecovery();
        if(!temporary&&reinterpret_cast<uintptr_t>(target)!=attackerHeldTargetId.address){
            if(target&&automaticRequest&&attackerLookOverride&&settings.debugLogging)++metrics.attackerOverrides;
            clearAttackerHeld();
        }
        // Losing a manual target returns to untargeted combat. Camera targeting
        // keeps the player's request and may find another enemy on its budget.
        if(dead&&settings.autoLockOnHit){
            playerLocked=false;clearAttackPending=true;clearDeath();
            if(settings.afterTargetDeath==0&&!attackerRemaining)deathRemaining=3.0;
            if(settings.debugLogging)++metrics.targetDeaths;
        }else if(!target&&!temporary&&!settings.targeting&&previous){playerLocked=false;clearAttackPending=true;}
        if(target)clearDeath();
    }
    originalSetTarget(combat,target);
    if(local){if(!target)clearTracking();managed(combat,true);}
}
void* abilityCombat(void* pawn) {
    if(!live()||!pawn)return nullptr;
    syncSession();
    auto combat=resolve(ownerId);
    return combat&&field<void*>(combat,0xa8)==pawn&&managed(combat)&&!playerLocked&&
        (field<uint8_t>(combat,0x8e)&0x10)&&!field<uint8_t>(combat,0x1612)?combat:nullptr;
}
void* abilityEnemy(void* combat,bool fresh) {
    if(inRequest||focusQuery)return nullptr;
    const auto now=GetTickCount64();
    if(!fresh&&abilityQueried&&now-abilityQueryAt<50){
        return resolve(abilityTargetId)?resolve(abilityActorId):nullptr;
    }
    abilityQueried=true;abilityQueryAt=now;abilityTargetId={};abilityActorId={};
    auto config=field<void*>(combat,0x9b8);if(!config)return nullptr;
    const float distance=field<float>(config,0x26c);
    if(!std::isfinite(distance)||distance<=0||distance>=1000000)return nullptr;
    const auto before=settings.debugLogging?clockMicros():0;
    if(settings.debugLogging)++metrics.focusQueries;
    FocusQuery query{combat,{}};
    const auto owner=ownerId;
    struct Restore {
        void* selection;bool request,automatic,fallback;FocusQuery* query;
        ~Restore(){selecting=selection;inRequest=request;automaticRequest=automatic;fallbackRequest=fallback;focusQuery=query;}
    } restore{selecting,inRequest,automaticRequest,fallbackRequest,focusQuery};
    selecting=combat;inRequest=true;automaticRequest=false;fallbackRequest=false;focusQuery=&query;
    originalSwitch(combat,2,distance,false,false,false,false,false);
    syncSession();
    void* actor{};
    if(live()&&owner==ownerId&&!playerLocked&&query.target.serial>0){
        if(auto target=resolve(query.target)){
            auto actorId=identity(field<void*>(target,0xa8));
            if(actorId.address){abilityTargetId=query.target;abilityActorId=actorId;actor=resolve(actorId);}
        }
    }
    if(settings.debugLogging){if(actor)++metrics.focusTargets;metrics.focusMicros+=clockMicros()-before;}
    return actor;
}
// Try the native no-target reference first. The same level/type dispatch
// used by CanBeActivated distinguishes Self (1) from Single/All/AoE (0/2/3).
// Those three types reject their owner as a primary target in native code.
bool routeAbility(void* ability,void* pawn,void* combat,uint8_t* reason,bool* detail,void*& target,bool fresh=false) {
    target=pawn;
    const bool initialDetail=detail?*detail:false;
    const bool accepted=originalCanAbility(ability,pawn,pawn,reason,detail);
    if(accepted)return true;
    if(!abilityCombat(pawn)||resolve(ownerId)!=combat)return false;
    const int level=method<int(*)(void*,void*)>(ability,0x4e0)(ability,pawn);
    const auto type=method<uint8_t(*)(void*,int)>(ability,0x540)(ability,level);
    if(type!=0&&type!=2&&type!=3)return false;
    // Unlocked casts follow camera aim, never a leftover focus actor.
    target=abilityEnemy(combat,fresh);
    if(!target)return false;
    if(detail)*detail=initialDetail;
    return originalCanAbility(ability,pawn,target,reason,detail);
}
bool canAbility(void* ability,void* pawn,void* target,uint8_t* reason,bool* detail) {
    auto combat=abilityCombat(pawn);
    if(!ability||!reason||!combat||checkingAbility)
        return originalCanAbility(ability,pawn,target,reason,detail);
    struct Restore {bool previous;~Restore(){checkingAbility=previous;}} restore{checkingAbility};
    checkingAbility=true;
    void* chosen{};return routeAbility(ability,pawn,combat,reason,detail,chosen);
}
void* focusActor(void* focus) {
    if(live()&&abilityPlan&&abilityPlan->focus==focus)return resolve(abilityPlan->actor);
    return originalFocusActor(focus);
}
bool planAbility(void* focus,void* ability) {
    auto pawn=live()&&focus?field<void*>(focus,0xa8):nullptr;
    auto combat=abilityCombat(pawn);
    // Respect specialised native activation overrides rather than calling a
    // base validator on an unknown ability implementation.
    if(!ability||!combat||checkingAbility||field<uint8_t>(focus,0x270)!=0||
       method<CanAbility>(ability,0x4d0)!=at<CanAbility>(Build::CanAbility))
        return originalPlanAbility(focus,ability);
    struct Restore {bool checking;AbilityPlan* plan;~Restore(){checkingAbility=checking;abilityPlan=plan;}} restore{checkingAbility,abilityPlan};
    checkingAbility=true;
    uint8_t reason{};bool detail{};void* target{};
    // Recheck current aim at activation, even inside the availability cache window.
    if(!routeAbility(ability,pawn,combat,&reason,&detail,target,true))return false;
    AbilityPlan plan{focus,identity(target)};
    if(!plan.actor.address||!abilityCombat(pawn))return false;
    abilityPlan=&plan;
    return originalPlanAbility(focus,ability);
}
void loseLock(void* combat,void* target,float duration) {
    const TemporaryLoss loss{combat,duration};const auto previous=temporaryLoss;
    temporaryLoss=live()&&settings.cameraMode==1?&loss:nullptr;
    originalLoseLock(combat,target,duration);temporaryLoss=previous;
}
void nativeRequest(void* combat) {
    if(managed(combat)){request(combat);return;}
    originalRequest(combat);
}
void lockTarget(void* combat,bool locked) {
    const bool local=managed(combat);
    if(local){
        if(lockButton==combat){
            ++lockIntent;clearDeath();clearAttackerPending();
            // Consume the input once, including recursive native clear calls.
            lockButton=nullptr;playerLocked=!playerLocked;
            clearAttackPending=!playerLocked;
            if(settings.debugLogging)++metrics.lockChanges;
            if(!playerLocked)clearTarget(combat);
        }
        locked=playerLocked;
    }
    originalLock(combat,locked);
    if(local){if(!playerLocked)clearTracking();managed(combat,true);}
}
bool fromLockButton(void* combat,void* frame,uintptr_t offset) {
    if(!managed(combat)||!frame)return false;
    // UE 5.5 FFrame and UStruct::Script layouts, checked against this build.
    // These two exact bytecode calls belong to IA_Combat_LockTarget. The native
    // thunks still decode all parameters and advance the VM normally.
    auto node=field<void*>(frame,0x10),pawn=field<void*>(combat,0xa8);
    if(!node||field<void*>(frame,0x18)!=pawn||field<uint64_t>(node,0x18)!=Build::playerGraphName)return false;
    auto outer=field<void*>(node,0x20);
    if(!outer||field<uint64_t>(outer,0x18)!=Build::playerClassName)return false;
    auto script=field<uintptr_t>(node,0x60),code=field<uintptr_t>(frame,0x20);
    const auto pc=code>=script?code-script:UINTPTR_MAX;
    if(script&&field<int>(node,0x68)==12194){
        if(pc==offset)return true;
        if(offset==0x8a9&&(pc==0x528||pc==0x6ba))return false; // Stock next/previous actions.
    }
    // A changed player Blueprint must not leave the player unable to lock.
    inputSupported=false;active=false;clearSession();
    RC::Output::send(L"[CombatCamera] Player target-lock input graph differs from the supported build; mod disabled for this session.\n");
    return false;
}
void lockScript(void* combat,void* frame,void* result) {
    auto previous=lockButton;
    lockButton=fromLockButton(combat,frame,0x84b)?combat:nullptr;
    originalLockScript(combat,frame,result);lockButton=previous;
}
void switchScript(void* combat,void* frame,void* result) {
    auto previous=lockButton;
    lockButton=fromLockButton(combat,frame,0x8a9)?combat:nullptr;
    originalSwitchScript(combat,frame,result);lockButton=previous;
}
bool actionTarget(void* combat) {
    return managed(combat)&&!playerLocked?false:originalActionTarget(combat);
}
void attackTarget(void* combat,void* target) {
    originalAttackTarget(combat,managed(combat)&&!playerLocked?nullptr:target);
}
void draw(void* hud) {
    originalDraw(hud);
    if(!live()||!settings.crosshair)return;
    auto pc=field<void*>(hud,0x2b8);auto pawn=pc?field<void*>(pc,0x8b8):nullptr;
    if(!pawn||field<void*>(pc,0x368)!=hud)return;
    auto state=gameplay(pawn);
    if(!state||(settings.crosshair==1&&state!=2))return;
    auto canvas=field<void*>(hud,0x308);if(!canvas)return;
    auto width=field<int>(canvas,0x40),height=field<int>(canvas,0x44);
    if(width<16||height<16||width>32768||height>32768)return;
    struct Vec2{double x,y;};struct Color{float r,g,b,a;};
    using Box=void(*)(void*,const Vec2*,const Vec2*,float,const Color*);
    auto box=at<Box>(Build::DrawBox);Vec2 outside{width*0.5-2,height*0.5-2},outerSize{4,4};
    Color black{0,0,0,0.8f};box(canvas,&outside,&outerSize,2,&black);
    Vec2 inside{width*0.5-1,height*0.5-1},innerSize{2,2};Color white{1,1,1,0.95f};box(canvas,&inside,&innerSize,2,&white);
    if(settings.debugLogging)++metrics.draw;
}
bool querySetting(void* owner,uint8_t id,void* value) {
    auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
    auto result=originalSettingQuery(owner,id,value);
    if(inputRoute&&caller==Build::InputSettingReturn){if(id==59&&result)inputRoute->gamepad=true;if(id==53)inputRoute->mouse=true;}
    return result;
}
InputValue* modify(void* modifier,InputValue* result,void* input,const InputValue* value,float delta) {
    if(!live()||!settings.targeting||!settings.aimAssist||!settings.assistStrength)return originalModify(modifier,result,input,value,delta);
    syncSession();
    auto action=field<void*>(modifier,0x20);
    // Exact action/package FNames are supplied by the startup binding, and are
    // numeric values only. Never retain a borrowed Lua object or engine struct.
    if(!action||field<uint64_t>(action,0x18)!=Build::lookName||!field<void*>(action,0x20)||
       field<uint64_t>(field<void*>(action,0x20),0x18)!=Build::lookPackageName)return originalModify(modifier,result,input,value,delta);
    auto pc=input?field<void*>(input,0x20):nullptr;auto pawn=pc?field<void*>(pc,0x8b8):nullptr;
    auto combat=pawn?field<void*>(pawn,0xc90):nullptr;
    if(!pc||field<void*>(pc,0x430)!=input||!eligible(combat))return originalModify(modifier,result,input,value,delta);
    InputRoute route;auto old=inputRoute;inputRoute=&route;
    auto output=originalModify(modifier,result,input,value,delta);inputRoute=old;
    auto target=field<void*>(combat,0x1380);auto now=GetTickCount64();
    if(output!=result||!output||output->type!=2||!route.gamepad||route.mouse||!target||
       !sameCurrent(field<void*>(target,0xa8),assistTargetId)||now<assistAt||now-assistAt>150||assistScale>=1)return output;
    float mx{},my{};at<void(*)(void*,float*,float*)>(Build::MouseDelta)(pc,&mx,&my);
    if(mx!=0||my!=0||!finite(output->value))return output;
    output->value.x*=assistScale;output->value.y*=assistScale;
    if(settings.debugLogging)++metrics.assist;
    return output;
}
template<class Fn> void hook(uintptr_t rva,Fn detour,Fn& original) {
    auto address=at<void*>(rva);
    if(MH_CreateHook(address,reinterpret_cast<void*>(detour),reinterpret_cast<void**>(&original))!=MH_OK)throw std::runtime_error("Hook creation failed");
    hooked[hookCount++]=address;
    if(MH_QueueEnableHook(address)!=MH_OK)throw std::runtime_error("Hook activation could not be queued");
}
}
extern "C" bool ShouldFreeCamera(void* combat) {
    // No thread-bound gameplay predicate here. The game-thread producer and
    // deletion listener publish/revoke the validated owner's identity.
    bool enabled=active.load(std::memory_order_acquire)&&combat&&cameraOwner.load(std::memory_order_acquire)==combat;
    if(cameraLogging.load(std::memory_order_relaxed)){
        cameraMetrics.checks.fetch_add(1,std::memory_order_relaxed);
        if(enabled)cameraMetrics.accepted.fetch_add(1,std::memory_order_relaxed);
        if(GetCurrentThreadId()!=gameThread)cameraMetrics.otherThread.fetch_add(1,std::memory_order_relaxed);
    }
    return enabled;
}
extern "C" bool ShouldPreventCameraAttach(void* combat) {
    // Attachment events on the gameplay thread validate current ownership and
    // initialize once. Other threads consume only the published opaque identity.
    const bool context=GetCurrentThreadId()!=gameThread||managed(combat,true);
    const bool prevent=context&&active.load(std::memory_order_acquire)&&combat&&cameraOwner.load(std::memory_order_acquire)==combat;
    if(prevent&&cameraLogging.load(std::memory_order_relaxed))cameraMetrics.attachPrevented.fetch_add(1,std::memory_order_relaxed);
    return prevent;
}
extern "C" bool ResolveFreeDirection(void* combat,Vec3* direction) {
    if(!managed(combat)||playerLocked)return false;
    const auto before=settings.debugLogging?clockMicros():0;
    auto pawn=field<void*>(combat,0xa8),pc=field<void*>(pawn,0x2e8);
    auto camera=field<void*>(pc,0x370);
    bool resolved=false;
    if(camera){
        Vec3 rotation{};
        auto getRotation=method<Vec3*(*)(void*,Vec3*)>(camera,0x820);
        auto value=getRotation?getRotation(camera,&rotation):nullptr;
        if(value&&finite(*value)){
            // Grounded attack direction uses yaw even when looking straight
            // up/down. No target selection or persistent actor rotation.
            *direction=forward({0,value->y,0});resolved=true;
        }
    }
    // The gate initializes XY from native character facing. Keep that
    // untargeted fallback when camera data is unavailable or nonfinite.
    if(settings.debugLogging){
        if(resolved)++metrics.cameraDirections;else ++metrics.directionFallbacks;
        metrics.directionMicros+=clockMicros()-before;
    }
    return true;
}
extern "C" double Threshold(void* context) {
    return live()&&selecting&&context==coneContext?coneThreshold:Build::nativeCone;
}
extern "C" bool UseNearestScore(void* context) {
    return live()&&nearestSelecting&&selecting&&context==coneContext;
}
void configure(Settings value) {
    if(gameThread&&GetCurrentThreadId()!=gameThread)throw std::runtime_error("Settings must be applied on the game thread");
    const bool cameraChanged=settings.cameraMode!=value.cameraMode;
    if(attackerHeldTargetId.address)clearAttackerLook();
    ++lockIntent;clearDeath();
    if(settings.lockLastAttacker!=value.lockLastAttacker||settings.autoLockOnHit!=value.autoLockOnHit)clearAttacker();
    settings=value;coneThreshold=value.coneDegrees?std::max(Build::nativeCone,std::cos(value.coneDegrees*3.14159265358979323846/180.0)):Build::nativeCone;
    cameraLogging.store(value.debugLogging,std::memory_order_relaxed);
    cameraMetrics.checks=0;cameraMetrics.accepted=0;cameraMetrics.otherThread=0;
    cameraMetrics.attachPrevented=0;cameraMetrics.initialDetaches=0;
    cameraMetrics.viewChecks=0;cameraMetrics.viewOtherThread=0;
    // The first nine ABI values and legacy freeCamera key remain readable.
    // Camera/targeting Apply preserves the explicit lock-button choice.
    if(cameraChanged){cameraInitialized=false;clearTracking();if(recoveryRemaining)clearAttackerHeld();clearRecovery();}
    if(!settings.enabled)clearSession();
    abilityQueried=false;abilityTargetId={};abilityActorId={};
    dwell.clear();nextFallback=false;assistScale=1;assistAt=0;
    metrics={};active=installed.load()&&settings.enabled&&inputSupported;
    if(cameraChanged&&live()){
        auto combat=reinterpret_cast<void*>(ownerId.address);
        if(sameCurrent(combat,ownerId))managed(combat,true);
    }
}
void deactivate(){active=false;cameraOwner.store(nullptr,std::memory_order_release);}
bool start(std::wstring& error) {
    if(attempted){error=startError;return installed.load();}attempted=true;gameThread=GetCurrentThreadId();moduleBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    try{
        NativeCompatibility::validateContract(moduleBase,Build::code,Build::pointers);
        for(auto& site:Build::guards)if(!NativeCompatibility::accessible(moduleBase,site.rva,site.size,true)
            ||std::memcmp(at<void*>(site.rva),site.bytes.data(),site.size)!=0)
            throw std::runtime_error("Game code differs at hook RVA "+NativeCompatibility::location(site.rva)+"; no patches installed");
        bool focusSupported=true;
        try{NativeCompatibility::validateContract(moduleBase,Build::focusCode,std::array<NativeCompatibility::Pointer,0>{});}
        catch(const std::exception& failure){
            focusSupported=false;const std::string reason=failure.what();
            RC::Output::send(L"[CombatCamera] Unlocked ability targeting unavailable: "+std::wstring(reason.begin(),reason.end())+L". Camera features remain available.\n");
        }
        auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)throw std::runtime_error("MinHook initialization failed");
        // Engine's validated FName constructor. Store only value IDs; resolve
        // input action names once on the game thread at startup.
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::lookName,"IA_Look",1);
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::lookPackageName,"/Game/_Dawnwalker/Player/Input/Actions/Traversal/IA_Look",1);
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::playerGraphName,"ExecuteUbergraph_BP_PlayerCharacter",1);
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::playerClassName,"BP_PlayerCharacter_C",1);
        if(!Build::lookName||!Build::lookPackageName||!Build::playerGraphName||!Build::playerClassName)throw std::runtime_error("Player input action names are unavailable");
        CameraContinue=at<void*>(Build::CameraResume);ConeContinue=at<void*>(Build::ConeResume);
        hook(Build::Camera,reinterpret_cast<void*>(&CameraGate),CameraOriginal);
        hook(Build::Score,reinterpret_cast<void*>(&ScoreGate),ScoreOriginal);
        void* unused{};hook(Build::Cone,reinterpret_cast<void*>(&ConeGate),unused);
        hook(Build::Tick,&tick,originalTick);hook(Build::Switch,&switchTarget,originalSwitch);
        hook(Build::Picker,&pick,originalPick);hook(Build::SetTarget,&setTarget,originalSetTarget);
        hook(Build::Request,&nativeRequest,originalRequest);hook(Build::Lock,&lockTarget,originalLock);
        hook(Build::LoseLock,&loseLock,originalLoseLock);
        hook(Build::ReactToHit,&reactToHit,originalReactToHit);
        if(focusSupported){
            hook(Build::CanAbility,&canAbility,originalCanAbility);
            hook(Build::PlanAbility,&planAbility,originalPlanAbility);
            hook(Build::FocusActor,&focusActor,originalFocusActor);
        }
        hook(Build::DrawHUD,&draw,originalDraw);hook(Build::Modify,&modify,originalModify);
        hook(Build::ViewRotation,&viewRotation,originalViewRotation);
        hook(Build::QuerySetting,&querySetting,originalSettingQuery);
        hook(Build::LockScript,&lockScript,originalLockScript);hook(Build::SwitchScript,&switchScript,originalSwitchScript);
        hook(Build::ActionTarget,&actionTarget,originalActionTarget);hook(Build::AttackTarget,&attackTarget,originalAttackTarget);
        ForwardContinue=at<void*>(Build::FreeDirection);
        hook(Build::Forward,reinterpret_cast<void*>(&ForwardGate),ForwardOriginal);
        AttachDirectContinue=at<void*>(Build::AttachDirect+7);
        hook(Build::AttachDirect,reinterpret_cast<void*>(&AttachDirectGate),AttachDirectOriginal);
        AttachRequestContinue=at<void*>(Build::AttachRequest+7);
        hook(Build::AttachRequest,reinterpret_cast<void*>(&AttachRequestGate),AttachRequestOriginal);
        AttachScriptContinue=at<void*>(Build::AttachScript+7);
        hook(Build::AttachScript,reinterpret_cast<void*>(&AttachScriptGate),AttachScriptOriginal);
        AttachCastContinue=at<void*>(Build::AttachCast+7);
        hook(Build::AttachCast,reinterpret_cast<void*>(&AttachCastGate),AttachCastOriginal);
        AttachAbilityContinue=at<void*>(Build::AttachAbility+7);
        hook(Build::AttachAbility,reinterpret_cast<void*>(&AttachAbilityGate),AttachAbilityOriginal);
        AttachThreatContinue=at<void*>(Build::AttachThreat+7);
        hook(Build::AttachThreat,reinterpret_cast<void*>(&AttachThreatGate),AttachThreatOriginal);
        AttachCombatContinue=at<void*>(Build::AttachCombat+7);
        hook(Build::AttachCombat,reinterpret_cast<void*>(&AttachCombatGate),AttachCombatOriginal);
        AttachLockContinue=at<void*>(Build::AttachLock+7);
        hook(Build::AttachLock,reinterpret_cast<void*>(&AttachLockGate),AttachLockOriginal);
        AttachSelectionContinue=at<void*>(Build::AttachSelection+7);
        hook(Build::AttachSelection,reinterpret_cast<void*>(&AttachSelectionGate),AttachSelectionOriginal);
        FUObjectArray::AddUObjectDeleteListener(&session);listening=true;
        if(MH_ApplyQueued()!=MH_OK)throw std::runtime_error("Hook activation failed");
        installed=true;active=settings.enabled&&inputSupported;return true;
    }catch(const std::exception& failure){
        auto message=std::string(failure.what());startError.assign(message.begin(),message.end());stop();error=startError;return false;
    }
}
void stop() {
    active=false;installed=false;cameraOwner.store(nullptr,std::memory_order_release);
    for(size_t i=0;i<hookCount;++i)MH_QueueDisableHook(hooked[i]);
    if(hookCount)MH_ApplyQueued();
    for(size_t i=0;i<hookCount;++i)MH_RemoveHook(hooked[i]);
    hookCount=0;
    if(listening){FUObjectArray::RemoveUObjectDeleteListener(&session);listening=false;}
}
}
