// Resolve only the two parameterless player combat notifications.
#pragma once
#include "CodeCompatibility.hpp"
namespace CombatCamera::CombatEvents {
inline uintptr_t resolve(uintptr_t image,uintptr_t table,const char* name) {
    using namespace NativeCompatibility;
    auto fail=[&](){throw std::runtime_error("Combat-start lock cannot verify parameterless notification "+std::string(name)+" at table "+location(table));};
    if(!accessible(image,table,16))fail();
    uintptr_t text{},thunk{};
    std::memcpy(&text,reinterpret_cast<void*>(image+table),8);
    std::memcpy(&thunk,reinterpret_cast<void*>(image+table+8),8);
    if(text<image||!accessible(image,text-image,std::strlen(name)+1)||std::strcmp(reinterpret_cast<const char*>(text),name))fail();
    if(thunk<image||!accessible(image,thunk-image,18,true))fail();
    auto bytes=reinterpret_cast<const unsigned char*>(thunk);
    // Parameterless reflected wrapper: consumes only the end-of-parameters
    // marker, leaves RCX (the component) untouched, tail-calls the native body.
    // Both equivalent XOR-zero encodings are valid; the body may move/change.
    const unsigned char frame[]{0xc0,0x48,0x39,0x42,0x20,0x0f,0x95,0xc0,0x48,0x01,0x42,0x20,0xe9};
    if((bytes[0]!=0x33&&bytes[0]!=0x31)||std::memcmp(bytes+1,frame,sizeof(frame)))fail();
    int32_t displacement{};std::memcpy(&displacement,bytes+14,4);
    auto target=thunk+18+static_cast<int64_t>(displacement);
    if(target<image||!accessible(image,target-image,16,true))fail();
    return target-image;
}
}
