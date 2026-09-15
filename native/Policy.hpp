// Combat Camera - Configurable. Copyright (c) 2026 my-mods. MIT.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace CombatCamera {
struct Settings {
    bool enabled{true}, freeCamera{true}, targeting{true};
    int crosshair{0}, delayMs{65}, coneDegrees{45};
    bool aimAssist{true};
    int assistStrength{35};
    bool debugLogging{false};
    int cameraMode{0},trackingSpeed{50},trackingResumeMs{750};
    bool lockLastAttacker{false};
};
struct Vec3 { double x{}, y{}, z{}; };
inline double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline bool finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
inline Vec3 forward(Vec3 rotation) {
    constexpr double rad=3.14159265358979323846/180.0;
    const auto pitch=rotation.x*rad, yaw=rotation.y*rad;
    return {std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),std::sin(pitch)};
}
inline double directionDot(Vec3 origin,Vec3 axis,Vec3 target) {
    Vec3 delta{target.x-origin.x,target.y-origin.y,target.z-origin.z};
    auto length=std::sqrt(dot(delta,delta));
    if(!finite(delta)||!finite(axis)||!std::isfinite(length)||length<1e-6) return -1;
    return std::clamp(dot(delta,axis)/length,-1.0,1.0);
}
inline double slowdown(double cosine,int percent) {
    constexpr double edge=0.984807753012208; // 10 degrees from the camera axis.
    if(!std::isfinite(cosine)||cosine<=edge||cosine>1.0||percent<=0) return 1.0;
    const double t=std::clamp((cosine-edge)/(1.0-edge),0.0,1.0);
    return 1.0-std::clamp(percent,0,80)*0.01*t*t*(3.0-2.0*t);
}
inline double angleDelta(double degrees) {
    double value=std::fmod(degrees+180.0,360.0);
    return (value<0?value+360.0:value)-180.0;
}
// Stateful timing only: each correction starts from the current view, never a
// retained engine rotation. Time is gameplay delta, so pauses cannot accumulate.
struct Tracking {
    double quiet{},ramp{};
    bool following{};
    void clear(){*this={};}
    Vec3 step(Vec3 view,Vec3 desired,double delta,bool input,int speed,int resumeMs) {
        if(!finite(view)||!finite(desired)||!std::isfinite(delta)||delta<=0||delta>0.25){clear();return {};}
        if(input){quiet=0;ramp=0;following=false;return {};}
        delta=std::min(delta,0.05);
        const double delay=std::clamp(resumeMs,0,3000)*0.001;
        const double previous=quiet;quiet=std::min(quiet+delta,delay+1.0);
        if(quiet<delay)return {};
        double activeDelta=following?delta:std::min(delta,std::max(0.0,quiet-delay));
        if(previous>=delay)activeDelta=delta;
        following=true;
        const double before=ramp;ramp=std::min(0.2,ramp+activeDelta);
        // Integrate the linear 200ms engagement ramp, avoiding frame-rate drift.
        const double weighted=activeDelta-(ramp-before)+(ramp*ramp-before*before)/0.4;
        const double s=std::clamp(speed,10,100)*0.01;
        const double alpha=-std::expm1(-8.0*s*weighted);
        Vec3 correction{angleDelta(desired.x-view.x)*alpha,angleDelta(desired.y-view.y)*alpha,0};
        const double length=std::hypot(correction.x,correction.y),limit=180.0*s*weighted;
        if(length>limit&&length>0){correction.x*=limit/length;correction.y*=limit/length;}
        return correction;
    }
};
// Opaque identity values only; ownership changes never bypass the hard budget.
struct RequestBudget {
    uint64_t last{}; bool used{};
    bool take(uint64_t now) {
        if(used && now>=last && now-last<50) return false;
        used=true;last=now;return true;
    }
};
struct Dwell {
    uintptr_t owner{}, candidate{}; uint64_t first{}, seen{};
    void clear(){*this={};}
    bool ready(uintptr_t currentOwner,uintptr_t next,uint64_t now,int delay) {
        if(!next||delay<=0){clear();return true;}
        if(owner!=currentOwner||candidate!=next||now<seen||now-seen>200) {
            owner=currentOwner;candidate=next;first=now;
        }
        seen=now;return now-first>=static_cast<uint64_t>(delay);
    }
};
}
