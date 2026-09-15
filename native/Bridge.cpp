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
#include <filesystem>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include "Bridge.hpp"
#include "GameBuild.hpp"
#include "GameCode.hpp"

extern "C" {
    void CameraGate(); void ConeGate(); void ForwardGate();
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
    std::array<std::atomic_int,8> watched;
    std::atomic_uint32_t invalidated{};
    Session(){for(auto& x:watched)x.store(-1);}
    void NotifyUObjectDeleted(const UObjectBase*,int32_t index) override {
        uint32_t mask=0;
        for(size_t i=0;i<watched.size();++i)if(watched[i].load(std::memory_order_relaxed)==index)mask|=1u<<i;
        if(mask&1)cameraOwner.store(nullptr,std::memory_order_release);
        if(mask)invalidated.fetch_or(mask,std::memory_order_release);
    }
    void OnUObjectArrayShutdown()override;
} session;
bool listening{};
void Session::OnUObjectArrayShutdown() {
    active=false;cameraOwner.store(nullptr,std::memory_order_release);invalidated.fetch_or(255);
    // Unregister before the engine checks its shutdown listener registry.
    // Leave hook teardown to stop(); no dying game objects are accessed here.
    if(listening){FUObjectArray::RemoveUObjectDeleteListener(this);listening=false;}
}
Identity ownerId,castLockId,assistTargetId,pendingId;
Identity trackingTargetId,trackingActorId;
Tracking tracking;
bool nativeCamera{};
void clearTracking() {
    tracking.clear();trackingTargetId={};trackingActorId={};
    session.watched[4]=-1;session.watched[5]=-1;
}
Dwell dwell;
bool nextFallback{};
bool playerLocked{};
bool clearAttackPending{true};
// A native temporary loss is different from an ordinary target clear. Keep
// identities only until its scheduled regain call; never follow a hidden actor.
Identity recoveryTargetId,recoveryActorId;
double recoveryRemaining{};
uint64_t recoveryLookAt{};
bool recoveryHadLook{};
void clearRecovery() {
    if(!recoveryRemaining)return;
    recoveryTargetId={};recoveryActorId={};recoveryRemaining=0;recoveryLookAt=0;recoveryHadLook=false;
    session.watched[6]=-1;session.watched[7]=-1;
}
void abandonRecovery() {
    if(recoveryRemaining&&!settings.targeting){playerLocked=false;clearAttackPending=true;}
    clearRecovery();
}
void* resolve(const Identity& id) {
    // Resolve the saved slot before dereferencing a possibly deleted address.
    if(!id.address)return nullptr;
    auto item=FUObjectArray::IndexToObject(id.index);
    return item&&reinterpret_cast<uintptr_t>(item->GetUObject())==id.address&&
        FUObjectArray::IsValid(item,false)&&item->GetSerialNumber()==id.serial?item->GetUObject():nullptr;
}
double assistScale{1.0};uint64_t assistAt{};
void clearSession() {
    cameraInitialized=false;
    nativeCamera=false;clearTracking();clearRecovery();
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
        if(ownerId.address==reinterpret_cast<uintptr_t>(combat)){cameraInitialized=false;clearTracking();abandonRecovery();}
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
    return cameraContext||field<uint8_t>(combat,0xb28)!=11;
}
bool eligible(void* combat) {
    if(!managed(combat)||!playerLocked||!(field<uint8_t>(combat,0x8e)&0x10)||
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
using Script=void(*)(void*,void*,void*);Script originalLockScript{},originalSwitchScript{};
using ActionTarget=bool(*)(void*);ActionTarget originalActionTarget{};
SetTarget originalAttackTarget{};
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
struct Metrics {
    uint64_t requests{},pickCalls{},candidates{},dwellChecks{},skipped{},draw{},assist{},ticks{},micros{};
    uint64_t lockChanges{},blockedTargets{};
    uint64_t cameraDirections{},directionFallbacks{},directionMicros{};
    uint64_t trackingAttempts{},trackingSteps{},trackingMoves{},trackingInput{},trackingMicros{};
    uint64_t temporaryLosses{},targetRecoveries{},recoveryMisses{};
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
        L" directionUs="+std::to_wstring(metrics.directionMicros)+
        L" cameraMode="+std::to_wstring(settings.cameraMode)+L" trackingSteps="+std::to_wstring(metrics.trackingSteps)+
        L" viewChecks="+std::to_wstring(cameraMetrics.viewChecks.exchange(0))+L" viewOtherThread="+std::to_wstring(cameraMetrics.viewOtherThread.exchange(0))+
        L" trackingUnavailable="+std::to_wstring(metrics.trackingAttempts-metrics.trackingSteps)+
        L" trackingMoves="+std::to_wstring(metrics.trackingMoves)+L" trackingInput="+std::to_wstring(metrics.trackingInput)+
        L" trackingUs="+std::to_wstring(metrics.trackingMicros)+
        L" temporaryLosses="+std::to_wstring(metrics.temporaryLosses)+L" targetRecoveries="+std::to_wstring(metrics.targetRecoveries)+
        L" recoveryMisses="+std::to_wstring(metrics.recoveryMisses)+
        L" targeting="+(playerLocked?(settings.targeting?std::wstring(L"camera"):std::wstring(L"fixed")):std::wstring(L"off"))+
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
    }
    const auto correction=tracking.step(*view,desired,delta,manual,settings.trackingSpeed,settings.trackingResumeMs);
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
    if(live()&&settings.cameraMode==1&&playerLocked&&
       reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase==Build::ViewRotationReturn){
        const auto before=settings.debugLogging?clockMicros():0;
        if(settings.debugLogging)++metrics.trackingAttempts;
        smoothView(camera,delta,view,input);
        if(settings.debugLogging)metrics.trackingMicros+=clockMicros()-before;
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
    if(inRequest||!settings.targeting||recoveryRemaining||!eligible(combat))return;
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
void clearTarget(void* combat) {
    clearRecovery();
    if(trackingTargetId.address)clearTracking();
    // The native setter removes delegates and publishes the target change.
    if(field<void*>(combat,0x1380)){originalSetTarget(combat,nullptr);clearAttackPending=true;}
    if(clearAttackPending){originalAttackTarget(combat,nullptr);clearAttackPending=false;}
    if((field<uint8_t>(combat,0x14a9)!=0)!=playerLocked)originalLock(combat,playerLocked);
    assistScale=1;assistAt=0;dwell.clear();nextFallback=false;
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
            }else recoveryRemaining-=delta;
        }
        if(!(field<uint8_t>(combat,0x8e)&0x10)&&playerLocked){playerLocked=false;clearAttackPending=true;}
        if(!playerLocked)clearTarget(combat);
    }
    originalTick(pawn,delta);
    if(!live())return;
    syncSession();
    combat=field<void*>(pawn,0xc90);
    if(managed(combat)){
        if(!playerLocked)clearTarget(combat);
    }
    if(settings.targeting && gameplay(pawn))request(combat);
    else if(settings.targeting&&settings.aimAssist&&eligible(combat)){
        auto now=GetTickCount64();if(assistBudget.take(now))updateAssist(combat,now);
    }else{assistScale=1;dwell.clear();}
    if(settings.debugLogging)report(GetTickCount64());
}
bool switchTarget(void* combat,uint8_t mode,float distance,bool a,bool b,bool c,bool d,bool e) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
    if(!managed(combat))return originalSwitch(combat,mode,distance,a,b,c,d,e);
    if(caller==Build::RegainLockReturn&&recoveryRemaining){
        const auto targetId=recoveryTargetId,actorId=recoveryActorId;
        auto target=resolve(targetId),actor=resolve(actorId);
        const auto now=GetTickCount64(),lastLook=recoveryLookAt;const bool hadLook=recoveryHadLook;
        if(settings.cameraMode!=1||!eligible(combat)||
           field<void*>(combat,0x1380)||!target||!actor||field<void*>(target,0xa8)!=actor){
            if(settings.debugLogging)++metrics.recoveryMisses;
            abandonRecovery();return false;
        }
        // Let the native switch build its in-range candidate list and validate
        // this one enemy. Its regain event is the only off-cone exception.
        const auto oldSelecting=selecting,oldRecovering=recovering;const bool wasRequest=inRequest;
        selecting=combat;recovering=actor;inRequest=true;
        originalSwitch(combat,2,distance,false,false,false,false,false);
        selecting=oldSelecting;recovering=oldRecovering;inRequest=wasRequest;
        const bool restored=recoveryTargetId==targetId&&recoveryActorId==actorId&&
            field<void*>(combat,0x1380)==target&&resolve(targetId)&&resolve(actorId);
        if(restored){
            clearRecovery();clearTracking();trackingTargetId=targetId;trackingActorId=actorId;
            session.watched[4]=targetId.index;session.watched[5]=actorId.index;
            tracking.quiet=hadLook?(now>=lastLook?(now-lastLook)*0.001:0):settings.trackingResumeMs*0.001;
            if(settings.debugLogging)++metrics.targetRecoveries;
        }else{if(settings.debugLogging)++metrics.recoveryMisses;abandonRecovery();}
        return restored;
    }
    // Native look/next/previous and threat/ability reacquisition never get to
    // replace a manual selection or create an unlocked soft target.
    if(lockButton!=combat)return false;
    lockButton=nullptr;
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
    if(recovering){
        // No search outside the native distance-filtered list, and no substitute
        // enemy if the remembered one is dead, unavailable or occluded.
        bool present=false;for(int i=0;i<candidates->size;++i)if(candidates->data[i]==recovering){present=true;break;}
        if(!present)return nullptr;
        void* actor=recovering;List singleton{&actor,1,1};field<List*>(context,0x58)=&singleton;
        field<uint8_t>(context,0x61)=0; // Verified native angular-rejection gate only.
        if(settings.debugLogging)++metrics.dwellChecks;
        return originalPick(context)==actor?actor:nullptr;
    }
    List waiting{&previous,1,1};
    if(retainOnly){field<List*>(context,0x58)=&waiting;if(settings.debugLogging)++metrics.dwellChecks;}
    else if(settings.debugLogging){++metrics.pickCalls;metrics.candidates+=candidates->size;}
    void* chosen=originalPick(context);
    if(automaticRequest&&!fallbackRequest&&!retainOnly&&!chosen)nextFallback=true;
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
    const bool local=managed(combat);
    if(local){
        if(target&&(!playerLocked||selecting!=combat)){if(settings.debugLogging)++metrics.blockedTargets;return;}
        const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
        bool temporary=false;
        if(!target&&playerLocked&&settings.cameraMode==1&&temporaryLoss&&temporaryLoss->combat==combat&&caller==Build::LoseLockClearReturn){
            auto previous=field<void*>(combat,0x1380);const auto id=identity(previous);
            auto actor=id.address?field<void*>(previous,0xa8):nullptr;const auto actorId=identity(actor);
            const float duration=temporaryLoss->duration;
            if(id.address&&actorId.address&&std::isfinite(duration)&&duration>0&&duration<=60){
                clearRecovery();recoveryTargetId=id;recoveryActorId=actorId;
                recoveryRemaining=duration+1.0;
                session.watched[6]=id.index;session.watched[7]=actorId.index;temporary=true;
                if(settings.debugLogging)++metrics.temporaryLosses;
            }
        }
        if((target&&!recovering)||(!target&&!temporary&&field<void*>(combat,0x1380)))clearRecovery();
        // Losing a manual target returns to untargeted combat. Camera targeting
        // keeps the player's request and may find another enemy on its budget.
        if(!target&&!temporary&&!settings.targeting&&field<void*>(combat,0x1380)){playerLocked=false;clearAttackPending=true;}
    }
    originalSetTarget(combat,target);
    if(local){if(!target)clearTracking();managed(combat,true);}
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
bool hashFile(const std::filesystem::path& path,const char* expected) {
    std::ifstream input(path,std::ios::binary);if(!input)return false;
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};bool good=false;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0){
        std::array<char,65536> buffer;bool valid=true;
        while(input){input.read(buffer.data(),buffer.size());if(BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer.data()),static_cast<ULONG>(input.gcount()),0)<0){valid=false;break;}}
        std::array<unsigned char,32> digest{};
        if(valid&&input.eof()&&BCryptFinishHash(hash,digest.data(),32,0)>=0){
            constexpr char hex[]="0123456789abcdef";std::string actual;
            for(auto b:digest){actual+=hex[b>>4];actual+=hex[b&15];}good=actual==expected;
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm,0);return good;
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
void configure(Settings value) {
    if(gameThread&&GetCurrentThreadId()!=gameThread)throw std::runtime_error("Settings must be applied on the game thread");
    const bool cameraChanged=settings.cameraMode!=value.cameraMode;
    settings=value;coneThreshold=value.coneDegrees?std::max(Build::nativeCone,std::cos(value.coneDegrees*3.14159265358979323846/180.0)):Build::nativeCone;
    cameraLogging.store(value.debugLogging,std::memory_order_relaxed);
    cameraMetrics.checks=0;cameraMetrics.accepted=0;cameraMetrics.otherThread=0;
    cameraMetrics.attachPrevented=0;cameraMetrics.initialDetaches=0;
    cameraMetrics.viewChecks=0;cameraMetrics.viewOtherThread=0;
    // The first nine ABI values and legacy freeCamera key remain readable.
    // Camera/targeting Apply preserves the explicit lock-button choice.
    if(cameraChanged){cameraInitialized=false;clearTracking();clearRecovery();}
    if(!settings.enabled)clearSession();
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
    wchar_t path[32768]{};
    try{
        NativeCompatibility::validateContract(moduleBase,Build::code,Build::pointers);
        auto host=GetModuleHandleW(L"UE4SS.dll");
        if(!host||!GetModuleFileNameW(host,path,32768)||!hashFile(path,Build::hostHash))throw std::runtime_error("Unsupported UE4SS build; use Framecore 2b");
        for(auto& site:Build::guards)if(!NativeCompatibility::accessible(moduleBase,site.rva,site.size,true)
            ||std::memcmp(at<void*>(site.rva),site.bytes.data(),site.size)!=0)
            throw std::runtime_error("Game code differs at hook RVA "+NativeCompatibility::location(site.rva)+"; no patches installed");
        auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)throw std::runtime_error("MinHook initialization failed");
        // Engine's FName constructor, resolved from the exact build. Stores
        // value IDs only and is called four times on the game thread at startup.
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::lookName,"IA_Look",1);
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::lookPackageName,"/Game/_Dawnwalker/Player/Input/Actions/Traversal/IA_Look",1);
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::playerGraphName,"ExecuteUbergraph_BP_PlayerCharacter",1);
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::playerClassName,"BP_PlayerCharacter_C",1);
        if(!Build::lookName||!Build::lookPackageName||!Build::playerGraphName||!Build::playerClassName)throw std::runtime_error("Player input action names are unavailable");
        CameraContinue=at<void*>(Build::CameraResume);ConeContinue=at<void*>(Build::ConeResume);
        hook(Build::Camera,reinterpret_cast<void*>(&CameraGate),CameraOriginal);
        void* unused{};hook(Build::Cone,reinterpret_cast<void*>(&ConeGate),unused);
        hook(Build::Tick,&tick,originalTick);hook(Build::Switch,&switchTarget,originalSwitch);
        hook(Build::Picker,&pick,originalPick);hook(Build::SetTarget,&setTarget,originalSetTarget);
        hook(Build::Request,&nativeRequest,originalRequest);hook(Build::Lock,&lockTarget,originalLock);
        hook(Build::LoseLock,&loseLock,originalLoseLock);
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
