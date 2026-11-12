#pragma once
#ifndef _WIN32
#error LiveControlShared is Windows-only
#endif
#include <windows.h>

namespace a50 {
// ABI shared between the Win32 UI and the WASAPI processing process.
// All fields are 32-bit aligned LONG, protected by an even/odd sequence.
struct LiveShared {
    volatile LONG sequence;
    volatile LONG gain_tenths;
    volatile LONG probability_milli;
    volatile LONG min_voice_db_tenths;
    volatile LONG voice_enabled;
    volatile LONG pitch_tenths;
    volatile LONG resonance_milli;
    volatile LONG bass_tenths;
    volatile LONG brightness_tenths;
    volatile LONG mix_milli;
};
struct LiveValues {
    LONG gain_tenths=0;
    LONG probability_milli=650;
    LONG min_voice_db_tenths=-460;
    LONG voice_enabled=0;
    LONG pitch_tenths=40;
    LONG resonance_milli=350;
    LONG bass_tenths=-30;
    LONG brightness_tenths=20;
    LONG mix_milli=1000;
};
inline LONG read_atomic(volatile LONG* p) {
    return InterlockedCompareExchange(p,0,0);
}
inline void publish(LiveShared* s,const LiveValues& v) {
    InterlockedIncrement(&s->sequence); // odd: update in progress
    InterlockedExchange(&s->gain_tenths,v.gain_tenths);
    InterlockedExchange(&s->probability_milli,v.probability_milli);
    InterlockedExchange(&s->min_voice_db_tenths,v.min_voice_db_tenths);
    InterlockedExchange(&s->voice_enabled,v.voice_enabled);
    InterlockedExchange(&s->pitch_tenths,v.pitch_tenths);
    InterlockedExchange(&s->resonance_milli,v.resonance_milli);
    InterlockedExchange(&s->bass_tenths,v.bass_tenths);
    InterlockedExchange(&s->brightness_tenths,v.brightness_tenths);
    InterlockedExchange(&s->mix_milli,v.mix_milli);
    InterlockedIncrement(&s->sequence); // even: consistent snapshot
}
inline bool snapshot(LiveShared* s, LiveValues& out, LONG& version) {
    for(int retry=0;retry<3;++retry) {
        const LONG before=read_atomic(&s->sequence);
        if(before&1) continue;
        LiveValues current;
        current.gain_tenths=read_atomic(&s->gain_tenths);
        current.probability_milli=read_atomic(&s->probability_milli);
        current.min_voice_db_tenths=read_atomic(&s->min_voice_db_tenths);
        current.voice_enabled=read_atomic(&s->voice_enabled);
        current.pitch_tenths=read_atomic(&s->pitch_tenths);
        current.resonance_milli=read_atomic(&s->resonance_milli);
        current.bass_tenths=read_atomic(&s->bass_tenths);
        current.brightness_tenths=read_atomic(&s->brightness_tenths);
        current.mix_milli=read_atomic(&s->mix_milli);
        const LONG after=read_atomic(&s->sequence);
        if(before==after && !(after&1)) {
            out=current;
            version=after;
            return true;
        }
    }
    return false;
}
}
