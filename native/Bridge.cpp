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

extern "C" {
    void CameraGate(); void ConeGate();
    void* CameraOriginal{}; void* CameraContinue{};
    void* ConeContinue{};
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
Settings settings;
DWORD gameThread{};
bool attempted{};
std::wstring startError;
std::array<void*,12> hooked{};size_t hookCount{};
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
    std::array<std::atomic_int,4> watched;
    std::atomic_uint32_t invalidated{};
    Session(){for(auto& x:watched)x.store(-1);}
    void NotifyUObjectDeleted(const UObjectBase*,int32_t index) override {
        uint32_t mask=0;
        for(size_t i=0;i<watched.size();++i)if(watched[i].load(std::memory_order_relaxed)==index)mask|=1u<<i;
        if(mask)invalidated.fetch_or(mask,std::memory_order_release);
    }
    void OnUObjectArrayShutdown()override {active=false;invalidated.fetch_or(15);}
} session;
bool listening{};
Identity ownerId,castLockId,assistTargetId,pendingId;
Dwell dwell;
bool nextFallback{};
double assistScale{1.0};uint64_t assistAt{};
void clearSession() {
    ownerId={};castLockId={};assistTargetId={};pendingId={};dwell.clear();nextFallback=false;assistScale=1;assistAt=0;
    for(auto& x:session.watched)x.store(-1,std::memory_order_relaxed);
}
void syncSession() {
    auto mask=session.invalidated.exchange(0,std::memory_order_acq_rel);
    if(mask&1){clearSession();return;}
    if(mask&2){castLockId={};session.watched[1]=-1;}
    if(mask&4){assistTargetId={};assistScale=1;assistAt=0;session.watched[2]=-1;}
    if(mask&8){pendingId={};dwell.clear();session.watched[3]=-1;}
}
bool live(){return active.load(std::memory_order_relaxed)&&GetCurrentThreadId()==gameThread;}
bool sameCurrent(void* object,const Identity& id){return id.address && identity(object)==id;}
// These relations are checked together, rather than treating a non-null
// controller/pawn/component pointer as a sufficient gameplay context.
int gameplay(void* pawn) {
    if(!pawn||field<uint8_t>(pawn,0x660))return 0;
    auto pc=field<void*>(pawn,0x2e8);auto combat=field<void*>(pawn,0xc90);
    if(!pc||!combat||field<uint8_t>(pc,0x6ec)!=1||field<void*>(pc,0x8b8)!=pawn||
       field<void*>(pc,0x2f8)!=pawn||field<void*>(combat,0xa8)!=pawn||field<int>(pc,0x8c0)!=0)return 0;
    if(field<uint8_t>(pc,0x4c8)&2)return 0; // Native cursor/menu gate.
    const auto action=field<uint8_t>(combat,0xb28);
    if(action==10||action==11)return 0;
    return (field<uint8_t>(combat,0x8e)&0x10)&&field<uint8_t>(combat,0xdb8)<=2&&field<uint8_t>(combat,0xdbc)==0?2:1;
}
bool manualLocked(void* combat) {
    return field<uint8_t>(combat,0x14a9)!=0&&!sameCurrent(combat,castLockId);
}
bool eligible(void* combat) {
    if(!combat||!gameplay(field<void*>(combat,0xa8))||!(field<uint8_t>(combat,0x8e)&0x10)||
       field<uint8_t>(combat,0x1612)||manualLocked(combat))return false;
    auto action=field<uint8_t>(combat,0xb28);
    return action!=1&&action!=2&&action!=10&&action!=11&&action!=12;
}
using Tick=void(*)(void*,float);Tick originalTick{};
using Switch=void(*)(void*,uint8_t,float,bool,bool,bool,bool,bool);Switch originalSwitch{};
using Pick=void*(*)(void*);Pick originalPick{};
using SetTarget=void(*)(void*,void*);SetTarget originalSetTarget{};
using Request=void(*)(void*);Request originalRequest{};
using Lock=void(*)(void*,bool);Lock originalLock{};
using Draw=void(*)(void*);Draw originalDraw{};
struct InputValue{Vec3 value;int type;int padding;};static_assert(sizeof(InputValue)==32);
using Modify=InputValue*(*)(void*,InputValue*,void*,const InputValue*,float);Modify originalModify{};
using SettingQuery=bool(*)(void*,uint8_t,void*);SettingQuery originalSettingQuery{};
struct InputRoute{bool gamepad{},mouse{};};
thread_local InputRoute* inputRoute{};
thread_local void* selecting{};
thread_local void* coneContext{};
thread_local bool automaticRequest{};
thread_local bool fallbackRequest{};
thread_local bool inRequest{};
struct Metrics {
    uint64_t requests{},pickCalls{},candidates{},dwellChecks{},skipped{},camera{},draw{},assist{},ticks{},micros{};
    uint64_t since{};
} metrics;
uint64_t clockMicros(){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return static_cast<uint64_t>(t.QuadPart/f.QuadPart)*1000000+static_cast<uint64_t>(t.QuadPart%f.QuadPart)*1000000/f.QuadPart;}
void report(uint64_t now) {
    if(!settings.debugLogging)return;
    if(!metrics.since){metrics.since=now;return;}
    if(now-metrics.since<10000)return;
    auto message=L"[CombatCamera] 10s: requests="+std::to_wstring(metrics.requests)+L" picker="+std::to_wstring(metrics.pickCalls)+
        L" candidates="+std::to_wstring(metrics.candidates)+L" dwellChecks="+std::to_wstring(metrics.dwellChecks)+
        L" budgetSkips="+std::to_wstring(metrics.skipped)+L" camera="+std::to_wstring(metrics.camera)+
        L" crosshair="+std::to_wstring(metrics.draw)+L" assist="+std::to_wstring(metrics.assist)+
        L" selectionUs="+std::to_wstring(metrics.micros)+L"\n";
    RC::Output::send(message);metrics={};metrics.since=now;
}
Vec3 cameraVector(void* camera,size_t slot) {
    Vec3 out{};auto value=method<Vec3*(*)(void*,Vec3*)>(camera,slot)(camera,&out);
    return value?*value:out;
}
void updateAssist(void* combat,uint64_t now) {
    assistScale=1;assistTargetId={};assistAt=0;session.watched[2]=-1;
    if(!settings.aimAssist||!settings.assistStrength)return;
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
    if(inRequest||!eligible(combat))return;
    auto config=field<void*>(combat,0x9b8);if(!config)return;
    float parameter=field<float>(config,0x26c);
    if(!std::isfinite(parameter)||parameter<=0||parameter>=1000000)return;
    const auto now=GetTickCount64();
    if(!requestBudget.take(now)){if(settings.debugLogging)++metrics.skipped;return;}
    auto id=identity(combat);if(!id.address)return;
    if(id!=ownerId){auto cast=castLockId;clearSession();ownerId=id;session.watched[0]=id.index;if(cast==id){castLockId=cast;session.watched[1]=id.index;}}
    inRequest=true;automaticRequest=true;fallbackRequest=nextFallback;nextFallback=false;auto previous=selecting;selecting=combat;
    uint64_t before=settings.debugLogging?clockMicros():0;
    if(settings.debugLogging)++metrics.requests;
    originalSwitch(combat,2,parameter,false,false,false,false,false);
    selecting=previous;automaticRequest=false;fallbackRequest=false;inRequest=false;
    updateAssist(combat,now);
    if(settings.debugLogging)metrics.micros+=clockMicros()-before;
}
void tick(void* pawn,float delta) {
    originalTick(pawn,delta);
    if(!live())return;
    syncSession();
    auto combat=field<void*>(pawn,0xc90);
    if(settings.targeting && gameplay(pawn))request(combat);
    else if(settings.aimAssist&&eligible(combat)){
        auto now=GetTickCount64();if(assistBudget.take(now))updateAssist(combat,now);
    }else{assistScale=1;dwell.clear();}
    if(settings.debugLogging)report(GetTickCount64());
}
void switchTarget(void* combat,uint8_t mode,float distance,bool a,bool b,bool c,bool d,bool e) {
    if(live())syncSession();
    auto previous=selecting;
    if(live()&&settings.targeting&&eligible(combat))selecting=combat;
    originalSwitch(combat,mode,distance,a,b,c,d,e);
    selecting=previous;
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
    auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
    if(live()&&settings.targeting&&caller==Build::AutoSetReturn&&target&&eligible(combat)){request(combat);return;}
    originalSetTarget(combat,target);
}
void nativeRequest(void* combat) {
    auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
    if(live()&&settings.targeting&&caller==Build::AutoRequestReturn&&eligible(combat)){request(combat);return;}
    originalRequest(combat);
}
void lockTarget(void* combat,bool locked) {
    if(live()){
        syncSession();
        auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-moduleBase;
        if(locked&&caller==Build::CastingLockReturn&&!field<uint8_t>(combat,0x14a9)){
            castLockId=identity(combat);session.watched[1]=castLockId.index;
        }else if(sameCurrent(combat,castLockId)){castLockId={};session.watched[1]=-1;}
    }
    originalLock(combat,locked);
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
    if(!live()||!settings.aimAssist||!settings.assistStrength)return originalModify(modifier,result,input,value,delta);
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
    if(!live()||!settings.freeCamera)return false;
    syncSession();
    bool enabled=combat&&gameplay(field<void*>(combat,0xa8))&&!manualLocked(combat);
    if(enabled&&settings.debugLogging)++metrics.camera;
    return enabled;
}
extern "C" double Threshold(void* context) {
    return live()&&settings.targeting&&selecting&&context==coneContext?coneThreshold:Build::nativeCone;
}
void configure(Settings value) {
    if(gameThread&&GetCurrentThreadId()!=gameThread)throw std::runtime_error("Settings must be applied on the game thread");
    settings=value;coneThreshold=value.coneDegrees?std::max(Build::nativeCone,std::cos(value.coneDegrees*3.14159265358979323846/180.0)):Build::nativeCone;
    clearSession();metrics={};active=installed.load()&&settings.enabled;
}
void deactivate(){active=false;}
bool start(std::wstring& error) {
    if(attempted){error=startError;return installed.load();}attempted=true;gameThread=GetCurrentThreadId();moduleBase=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    wchar_t path[32768]{};
    try{
        if(!GetModuleFileNameW(nullptr,path,32768)||!hashFile(path,Build::gameHash))throw std::runtime_error("Unsupported Dawnwalker executable; no patches installed");
        auto host=GetModuleHandleW(L"UE4SS.dll");
        if(!host||!GetModuleFileNameW(host,path,32768)||!hashFile(path,Build::hostHash))throw std::runtime_error("Unsupported UE4SS build; use Framecore 2b");
        for(auto& site:Build::guards)if(std::memcmp(at<void*>(site.rva),site.bytes.data(),site.size)!=0)throw std::runtime_error("Game code differs at a required hook; disable conflicting camera mods");
        auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)throw std::runtime_error("MinHook initialization failed");
        // Engine's FName constructor, resolved from the exact build. Stores
        // value IDs only and is called twice, on the game thread, at startup.
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::lookName,"IA_Look",1);
        at<void(*)(uint64_t*,const char*,int)>(Build::MakeName)(&Build::lookPackageName,"/Game/_Dawnwalker/Player/Input/Actions/Traversal/IA_Look",1);
        if(!Build::lookName||!Build::lookPackageName)throw std::runtime_error("Look input action names are unavailable");
        CameraContinue=at<void*>(Build::CameraResume);ConeContinue=at<void*>(Build::ConeResume);
        hook(Build::Camera,reinterpret_cast<void*>(&CameraGate),CameraOriginal);
        void* unused{};hook(Build::Cone,reinterpret_cast<void*>(&ConeGate),unused);
        hook(Build::Tick,&tick,originalTick);hook(Build::Switch,&switchTarget,originalSwitch);
        hook(Build::Picker,&pick,originalPick);hook(Build::SetTarget,&setTarget,originalSetTarget);
        hook(Build::Request,&nativeRequest,originalRequest);hook(Build::Lock,&lockTarget,originalLock);
        hook(Build::DrawHUD,&draw,originalDraw);hook(Build::Modify,&modify,originalModify);
        hook(Build::QuerySetting,&querySetting,originalSettingQuery);
        FUObjectArray::AddUObjectDeleteListener(&session);listening=true;
        if(MH_ApplyQueued()!=MH_OK)throw std::runtime_error("Hook activation failed");
        installed=true;active=settings.enabled;return true;
    }catch(const std::exception& failure){
        auto message=std::string(failure.what());startError.assign(message.begin(),message.end());stop();error=startError;return false;
    }
}
void stop() {
    active=false;installed=false;
    for(size_t i=0;i<hookCount;++i)MH_QueueDisableHook(hooked[i]);
    if(hookCount)MH_ApplyQueued();
    for(size_t i=0;i<hookCount;++i)MH_RemoveHook(hooked[i]);
    hookCount=0;
    if(listening){FUObjectArray::RemoveUObjectDeleteListener(&session);listening=false;}
}
}
