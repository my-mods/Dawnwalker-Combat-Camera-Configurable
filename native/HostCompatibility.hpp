// These loaders share the pinned 97b7e501c C++/Lua interface. The host DLL
// fingerprint bounds that ABI; game compatibility is checked independently.
#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <fstream>

namespace HostCompatibility {
enum class Runtime { Unsupported, Framecore2b, VercadiRc6 };
using Digest=std::array<unsigned char,32>;
inline constexpr Digest framecore{0xfb,0x18,0x39,0xee,0x91,0xf7,0x1f,0x83,0xd5,0x08,0xd4,0x4a,0x27,0x63,0xa1,0x5a,0xc1,0xbb,0x0c,0x5f,0xb4,0xe5,0x04,0xac,0x0f,0xcf,0xca,0x64,0x37,0x6a,0x05,0x4a};
inline constexpr Digest vercadi{0x79,0x5c,0x37,0x32,0xdd,0x4d,0xde,0x53,0xd1,0xc9,0x21,0x0d,0x93,0xae,0x81,0x91,0xb9,0x83,0x03,0x1c,0xd2,0x75,0xe7,0xe1,0xcf,0x93,0xfe,0xf6,0x9f,0xad,0x6d,0xeb};
inline Runtime identifyDigest(const Digest& digest) {
    if(digest==framecore)return Runtime::Framecore2b;
    if(digest==vercadi)return Runtime::VercadiRc6;
    return Runtime::Unsupported;
}
inline Runtime identifyFile(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file)return Runtime::Unsupported;
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return Runtime::Unsupported;
    Digest digest{};bool ok=false;
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0) {
        std::array<char,65536> data{};bool good=true;
        while(file.read(data.data(),data.size())||file.gcount()) {
            if(BCryptHashData(hash,reinterpret_cast<PUCHAR>(data.data()),static_cast<ULONG>(file.gcount()),0)<0){good=false;break;}
        }
        ok=good&&file.eof()&&!file.bad()&&BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0)>=0;
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm,0);
    return ok?identifyDigest(digest):Runtime::Unsupported;
}
inline Runtime identifyLoaded() {
    const auto module=GetModuleHandleW(L"UE4SS.dll");
    if(!module)return Runtime::Unsupported;
    wchar_t path[32768]{};const auto length=GetModuleFileNameW(module,path,32768);
    if(!length||length==32768)return Runtime::Unsupported;
    return identifyFile(path);
}
}
