// Direction-selection contract for LTT_AddWeakSpot; resolved references are checked by name.
#pragma once
#include <array>
#include <cstdint>
namespace CombatCamera::RiposteScript {
inline constexpr int size=977;
inline constexpr uintptr_t queryEnd=0x88;
struct Bytes {uintptr_t offset;const char* value;size_t length;};
inline constexpr std::array<Bytes,30> bytes{{
{0x57,"\x0f",1},
{0x60,"\x00",1},
{0x69,"\x19\x00",2},
{0x73,"\x0a\x00\x00\x00",4},
{0x7f,"\x1c",1},
{0x88,"\x16\x14\x00",3},
{0x93,"\x68",1},
{0x9c,"\x00",1},
{0xa5,"\x1f\x52\x65\x62\x65\x6c\x41\x49\x2e\x44\x69\x72\x65\x63\x74\x69\x6f\x6e\x2e\x54\x6f\x70\x00\x16\x07\x6e\x03\x00\x00\x00",30},
{0xcb,"\x14\x00",2},
{0xd5,"\x68",1},
{0xde,"\x00",1},
{0xe7,"\x1f\x52\x65\x62\x65\x6c\x41\x49\x2e\x44\x69\x72\x65\x63\x74\x69\x6f\x6e\x2e\x42\x6f\x74\x74\x6f\x6d\x00\x16\x07\x86\x03\x00\x00\x00",33},
{0x110,"\x14\x00",2},
{0x11a,"\x68",1},
{0x123,"\x00",1},
{0x12c,"\x1f\x52\x65\x62\x65\x6c\x41\x49\x2e\x44\x69\x72\x65\x63\x74\x69\x6f\x6e\x2e\x4c\x65\x66\x74\x00\x16\x07\x9e\x03\x00\x00\x00",31},
{0x153,"\x14\x00",2},
{0x15d,"\x68",1},
{0x166,"\x00",1},
{0x16f,"\x1f\x52\x65\x62\x65\x6c\x41\x49\x2e\x44\x69\x72\x65\x63\x74\x69\x6f\x6e\x2e\x52\x69\x67\x68\x74\x00\x16\x07\xb6\x03\x00\x00\x00",32},
{0x36e,"\x5f\x01",2},
{0x378,"\x20",1},
{0x381,"\x06\xe5\x01\x00\x00\x5f\x01",7},
{0x390,"\x20",1},
{0x399,"\x06\xe5\x01\x00\x00\x5f\x01",7},
{0x3a8,"\x20",1},
{0x3b1,"\x06\xe5\x01\x00\x00\x5f\x01",7},
{0x3c0,"\x20",1},
{0x3c9,"\x06\xe5\x01\x00\x00",5},
}};
struct Object {uintptr_t offset;const char* name;};
inline constexpr std::array<Object,9> objects{{
{0x80,"GetDirectionRequiredForCombo"},
{0x94,"NotEqual_TagTag"},
{0xd6,"NotEqual_TagTag"},
{0x11b,"NotEqual_TagTag"},
{0x15e,"NotEqual_TagTag"},
{0x379,"GE_WeakSpot_Bottom_C"},
{0x391,"GE_WeakSpot_Top_C"},
{0x3a9,"GE_WeakSpot_Left_C"},
{0x3c1,"GE_WeakSpot_Right_C"},
}};
inline std::array<uint64_t,objects.size()> names{};
struct Property {uintptr_t offset,reference;};
inline constexpr std::array<Property,21> properties{{
{0x58,0x58},
{0x61,0x58},
{0x77,0x58},
{0x9d,0x58},
{0xdf,0x58},
{0x124,0x58},
{0x167,0x58},
{0x8b,0x8b},
{0xc3,0x8b},
{0xcd,0x8b},
{0x108,0x8b},
{0x112,0x8b},
{0x14b,0x8b},
{0x155,0x8b},
{0x18f,0x8b},
{0x1d4,0x1d4},
{0x27b,0x1d4},
{0x370,0x1d4},
{0x388,0x1d4},
{0x3a0,0x1d4},
{0x3b8,0x1d4},
}};
}
