#pragma once
#ifdef _WIN32
#include <windows.h>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <cwchar>

namespace a50 {
struct StoredProfile {
    std::array<int,8> sliders{}; // gain, prob, minvoice, pitch, resonance, bass, bright, mix
    int denoise=1, vad=1, voice=0;
};
inline const std::array<const wchar_t*,11>& profile_keys() {
    static const std::array<const wchar_t*,11> k{
        L"gain",L"prob",L"minvoice",L"pitch",L"resonance",
        L"bass",L"brightness",L"mix",L"denoise",L"vad",L"voice"
    };
    return k;
}
inline bool valid_profile_name(const std::wstring& name) {
    return !name.empty() && name.size()<100 &&
           name.find_first_of(L"[]=;\r\n")==std::wstring::npos;
}
inline std::vector<std::wstring> list_profiles(const std::filesystem::path& file) {
    std::array<wchar_t,16384> sections{};
    GetPrivateProfileSectionNamesW(sections.data(),
                                   static_cast<DWORD>(sections.size()),file.c_str());
    std::vector<std::wstring> result;
    for(const wchar_t* p=sections.data();*p;p+=std::wcslen(p)+1)
        result.emplace_back(p);
    return result;
}
inline bool profile_exists(const std::filesystem::path& file,const std::wstring& name) {
    for(const auto& current:list_profiles(file)) if(current==name) return true;
    return false;
}
inline void save_profile_file(const std::filesystem::path& file,
                              const std::wstring& name,const StoredProfile& values) {
    if(!valid_profile_name(name)) throw std::invalid_argument("Invalid profile name");
    std::array<int,11> all{};
    for(std::size_t i=0;i<8;++i) all[i]=values.sliders[i];
    all[8]=values.denoise; all[9]=values.vad; all[10]=values.voice;
    const auto& keys=profile_keys();
    for(std::size_t i=0;i<all.size();++i) {
        const auto val=std::to_wstring(all[i]);
        if(!WritePrivateProfileStringW(name.c_str(),keys[i],val.c_str(),file.c_str()))
            throw std::runtime_error("Unable to save INI profile");
    }
}
inline StoredProfile load_profile_file(const std::filesystem::path& file,
                                      const std::wstring& name) {
    if(!valid_profile_name(name)||!profile_exists(file,name))
        throw std::invalid_argument("Unknown profile");
    StoredProfile result;
    constexpr std::array<int,11> defaults{0,65,-46,4,35,-3,2,100,1,1,0};
    std::array<int,11> v{};
    const auto& keys=profile_keys();
    for(std::size_t i=0;i<v.size();++i)
        v[i]=GetPrivateProfileIntW(name.c_str(),keys[i],defaults[i],file.c_str());
    for(std::size_t i=0;i<8;++i)result.sliders[i]=v[i];
    result.denoise=v[8]; result.vad=v[9]; result.voice=v[10];
    return result;
}
}
#endif
