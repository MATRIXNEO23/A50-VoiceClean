#pragma once
#include "dsp.h"
#include "wav.h"
namespace a50 {
// Speed adjustment applies to WAV processing only, not a perpetual live stream.
Wave transform(Wave input, Settings settings, float speed = 1.0f);
}
