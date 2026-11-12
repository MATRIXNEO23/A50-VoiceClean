#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include "live_shared.h"
#include <iostream>

int main() {
    a50::LiveShared shared{};
    a50::LiveValues values{}, snapshot{};
    LONG version=0;
    a50::publish(&shared,values);
    assert(a50::snapshot(&shared,snapshot,version));
    assert(version==2 && snapshot.probability_milli==650);
    values.pitch_tenths=75;
    values.gain_tenths=-30;
    values.voice_enabled=1;
    values.mix_milli=250;
    a50::publish(&shared,values);
    assert(a50::snapshot(&shared,snapshot,version));
    assert(version==4 && snapshot.pitch_tenths==75 &&
           snapshot.gain_tenths==-30 &&
           snapshot.voice_enabled==1 &&
           snapshot.mix_milli==250);
    InterlockedExchange(&shared.meter_input_db,318);
    InterlockedExchange(&shared.meter_output_db,185);
    InterlockedExchange(&shared.voice_probability_milli,817);
    assert(a50::read_atomic(&shared.meter_input_db)==318);
    assert(a50::read_atomic(&shared.meter_output_db)==185);
    assert(a50::read_atomic(&shared.voice_probability_milli)==817);
    std::cout<<"Live shared settings protocol passed\n";
}
