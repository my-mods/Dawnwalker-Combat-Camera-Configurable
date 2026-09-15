// Host calls use the pinned UE4SS C++ interface and its registered Lua state.
#include <Mod/CppUserModBase.hpp>
#include <LuaMadeSimple/LuaMadeSimple.hpp>
#include <DynamicOutput/Output.hpp>
#include "Bridge.hpp"
using namespace RC;
using Lua=LuaMadeSimple::Lua;
static_assert(sizeof(CppUserModBase)==192,"Unsupported UE4SS C++ host layout");
class CombatCameraMod final:public CppUserModBase {
public:
    CombatCameraMod(){ ModName=STR("Combat Camera - Configurable");ModVersion=STR("3.1.0");ModAuthors=STR("my-mods"); }
    void on_lua_start(StringViewType name,Lua& lua,Lua&,Lua&,Lua*) override {
        if(name!=STR("CombatCamera"))return;
        lua.register_function("_CCSet",[](const Lua& l){
            // get_integer removes the argument from the host Lua stack.
            // Each next setting is therefore at index 1, in Settings.order.
            auto number=[&](int lo,int hi){return static_cast<int>(std::clamp<int64_t>(l.get_integer(1),lo,hi));};
            CombatCamera::Settings s;
            s.enabled=number(0,1)!=0;s.freeCamera=number(0,1)!=0;s.targeting=number(0,1)!=0;
            s.crosshair=number(0,2);s.delayMs=number(0,1000);s.coneDegrees=number(0,90);
            s.aimAssist=number(0,1)!=0;s.assistStrength=number(0,80);s.debugLogging=number(0,1)!=0;
            s.cameraMode=number(0,2);s.trackingSpeed=number(10,100);s.trackingResumeMs=number(0,3000);
            s.lockLastAttacker=number(0,1)!=0;
            CombatCamera::configure(s);return 0;
        });
        lua.register_function("_CCStart",[](const Lua& l){
            std::wstring error;bool ok=CombatCamera::start(error);
            if(!ok)Output::send(STR("[CombatCamera] Disabled: ")+error+STR("\n"));
            l.set_bool(ok);return 1;
        });
    }
    void on_lua_stop(StringViewType name,Lua&,Lua&,Lua&,Lua*) override {
        if(name==STR("CombatCamera")) CombatCamera::deactivate();
    }
    ~CombatCameraMod()override {CombatCamera::stop();}
};
extern "C" __declspec(dllexport) CppUserModBase* start_mod(){return new CombatCameraMod;}
extern "C" __declspec(dllexport) void uninstall_mod(CppUserModBase* mod){delete mod;}
