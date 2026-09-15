// MIT. Validate only the native code/data used by this mod, in the loaded image.
#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace NativeCompatibility {
struct Code { uintptr_t rva; ULONG size; const char* digest; };
struct Pointer { uintptr_t rva; uintptr_t target; };
inline std::string location(uintptr_t rva) {
    constexpr char digits[]="0123456789abcdef";
    std::string value; do { value.insert(value.begin(),digits[rva&15]);rva>>=4; } while(rva);
    return "0x"+value;
}
inline bool accessible(uintptr_t image,uintptr_t rva,size_t size,bool executable=false) {
    if(!image||!size||rva>std::numeric_limits<uintptr_t>::max()-image)return false;
    uintptr_t cursor=image+rva;
    if(size>std::numeric_limits<uintptr_t>::max()-cursor)return false;
    const auto end=cursor+size;
    while(cursor<end) {
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region))
            ||region.AllocationBase!=reinterpret_cast<void*>(image)||region.State!=MEM_COMMIT
            ||(region.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
        const auto protection=region.Protect&0xff;
        const bool readable=protection==PAGE_READONLY||protection==PAGE_READWRITE||protection==PAGE_WRITECOPY
            ||protection==PAGE_EXECUTE_READ||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
        const bool code=protection==PAGE_EXECUTE_READ||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
        if(!readable||(executable&&!code))return false;
        const auto begin=reinterpret_cast<uintptr_t>(region.BaseAddress);
        if(region.RegionSize>std::numeric_limits<uintptr_t>::max()-begin)return false;
        const auto next=begin+region.RegionSize;
        if(next<=cursor)return false;
        cursor=next;
    }
    return true;
}
template<size_t N,size_t P> void validateContract(uintptr_t image,const std::array<Code,N>& functions,const std::array<Pointer,P>& pointers) {
    // Validate ranges before any native pointer read, hash or game call.
    for(const auto& site:functions)if(!accessible(image,site.rva,site.size,true))
        throw std::runtime_error("Required game code is unavailable at RVA "+location(site.rva)+"; no native changes installed");
    for(const auto& slot:pointers) {
        if(!accessible(image,slot.rva,sizeof(uintptr_t)))
            throw std::runtime_error("Required game table is unavailable at RVA "+location(slot.rva)+"; no native changes installed");
        uintptr_t actual{};std::memcpy(&actual,reinterpret_cast<void*>(image+slot.rva),sizeof(actual));
        if(slot.target>std::numeric_limits<uintptr_t>::max()-image||actual!=image+slot.target)
            throw std::runtime_error("Game table differs at RVA "+location(slot.rva)+"; no native changes installed");
    }
    BCRYPT_ALG_HANDLE algorithm{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)
        throw std::runtime_error("Native code validation could not initialize SHA-256; no native changes installed");
    struct Close { BCRYPT_ALG_HANDLE value;~Close(){BCryptCloseAlgorithmProvider(value,0);} } close{algorithm};
    for(const auto& site:functions) {
        std::array<unsigned char,32> digest{};
        if(BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(image+site.rva),site.size,digest.data(),32)<0)
            throw std::runtime_error("Native code validation failed at RVA "+location(site.rva)+"; no native changes installed");
        constexpr char hex[]="0123456789abcdef";std::string actual;
        for(auto byte:digest){actual+=hex[byte>>4];actual+=hex[byte&15];}
        if(actual!=site.digest)
            throw std::runtime_error("Required game function differs at RVA "+location(site.rva)+" (different game code or another native mod); no native changes installed");
    }
}
}
