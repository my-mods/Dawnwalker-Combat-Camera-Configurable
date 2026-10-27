// Validate the input expressions relative to the current VM cursor.
// Whole graph length and absolute bytecode positions are not dependencies.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace CombatCamera::LockInput {
enum class Route { Invalid, Toggle, Next, Previous };
struct Bytes {ptrdiff_t offset;const char* data;size_t size;};
inline constexpr std::array<const char*,7> names{
    "Not_PreBool","SetHardLock","GetConfig","SwitchLockTarget",
    "KismetMathLibrary","PlayerCombatComponent","CombatComponentBase"
};
inline std::array<uint64_t,names.size()> ids{};
inline uint64_t packageName{};
template<class T> T read(const unsigned char* cursor,ptrdiff_t offset) {
    T value{};std::memcpy(&value,cursor+offset,sizeof(value));return value;
}
inline bool same(const unsigned char* cursor,ptrdiff_t a,ptrdiff_t b) {
    auto value=read<uintptr_t>(cursor,a);return value&&value==read<uintptr_t>(cursor,b);
}
template<size_t N> bool bytes(const unsigned char* cursor,const std::array<Bytes,N>& parts) {
    for(auto& part:parts)if(std::memcmp(cursor+part.offset,part.data,part.size))return false;
    return true;
}
inline constexpr std::array<Bytes,10> toggle{{
{-82,"\x14\x00",2},{-72,"\x68",1},{-63,"\x19\x01",2},{-53,"\x09\x00\x00\x00",4},
{-41,"\x01",1},{-32,"\x16\x19\x01",3},{-21,"\x13\x00\x00\x00",4},{-9,"\x1c",1},
{0,"\x00",1},{9,"\x16",1}
}};
inline constexpr std::array<Bytes,13> selection{{
{-83,"\x5f\x00",2},{-73,"\x19\x01",2},{-63,"\x0a\x00\x00\x00",4},
{-51,"\x1c",1},{-42,"\x16\x14\x00",3},{-31,"\x19\x01",2},
{-21,"\x30\x00\x00\x00",4},{-9,"\x1c",1},{0,"\x24",1},{2,"\x19\x00",2},
{12,"\x09\x00\x00\x00",4},{24,"\x01",1},{33,"\x27\x28\x28\x28\x28\x16",6}
}};
// object() verifies the called UFunction name and its owning native class.
template<class Object> Route classify(const unsigned char* script,size_t length,size_t pc,bool switching,Object object) {
    const size_t before=switching?83:82,after=switching?39:10;
    if(!script||pc<before||pc>length||after>length-pc)return Route::Invalid;
    auto cursor=script+pc;
    if(!switching) {
        if(!bytes(cursor,toggle)||read<uintptr_t>(cursor,-17)!=0||
           !same(cursor,-80,1)||!same(cursor,-61,-29)||!same(cursor,-49,-40)||
           !object(read<void*>(cursor,-71),ids[0],ids[4])||
           !object(read<void*>(cursor,-8),ids[1],ids[5]))return Route::Invalid;
        return Route::Toggle;
    }
    if(!bytes(cursor,selection)||!same(cursor,-81,-59)||!same(cursor,-81,4)||
       !same(cursor,-71,-29)||!same(cursor,-39,-17)||!same(cursor,16,25)||
       !object(read<void*>(cursor,-50),ids[2],ids[6])||
       !object(read<void*>(cursor,-8),ids[3],ids[5]))return Route::Invalid;
    switch(cursor[1]) {case 2:return Route::Toggle;case 1:return Route::Next;case 0:return Route::Previous;default:return Route::Invalid;}
}
}
