#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include "live_profiles_win.h"

int main() {
    assert(a50::valid_profile_name(L"Voce personale"));
    assert(!a50::valid_profile_name(L""));
    assert(!a50::valid_profile_name(L"bad]name"));
    assert(!a50::valid_profile_name(L"bad\nname"));
    const auto file=std::filesystem::temp_directory_path()/
      (L"A50VoiceClean-test-"+std::to_wstring(GetCurrentProcessId())+L".ini");
    std::filesystem::remove(file);
    a50::StoredProfile original;
    original.sliders={3,72,-43,5,38,-4,3,95};
    original.denoise=1;original.vad=1;original.voice=1;
    original.clarity=1;original.denoise_strength=65;
    a50::save_profile_file(file,L"Mio preset",original);
    assert(a50::profile_exists(file,L"Mio preset"));
    const auto names=a50::list_profiles(file);
    assert(std::find(names.begin(),names.end(),L"Mio preset")!=names.end());
    const auto loaded=a50::load_profile_file(file,L"Mio preset");
    assert(loaded.sliders==original.sliders);
    assert(loaded.voice==1 && loaded.vad==1 && loaded.denoise==1);
    assert(loaded.clarity==1 && loaded.denoise_strength==65);
    std::filesystem::remove(file);
    std::cout<<"Live profile validation and persistence passed\n";
}
