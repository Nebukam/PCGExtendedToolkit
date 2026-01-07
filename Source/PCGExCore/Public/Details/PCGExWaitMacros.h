// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_ASYNC_WAIT_CHKD(_CONDITION) int _WAIT_COUNTER_ = 0; while (_CONDITION){ if (++_WAIT_COUNTER_ < 100){ FPlatformProcess::YieldThread(); } else { FPlatformProcess::SleepNoStats(0.001f); _WAIT_COUNTER_ = 0; } }
#define PCGEX_ASYNC_WAIT_CHKD_ADV(_CONDITION)\
constexpr int SPIN_PHASE_ITERATIONS = 50; constexpr int YIELD_PHASE_ITERATIONS = 200;\
constexpr float SHORT_SLEEP_MS = 0.001f; constexpr float LONG_SLEEP_MS = 0.005f;\
constexpr int LONG_SLEEP_THRESHOLD = 1000; int _WAIT_COUNTER_ = 0;\
while (_CONDITION){if (_WAIT_COUNTER_ < SPIN_PHASE_ITERATIONS)	{FPlatformProcess::YieldThread();}\
else if (_WAIT_COUNTER_ < YIELD_PHASE_ITERATIONS){if ((_WAIT_COUNTER_ & 0x7) == 0){ FPlatformProcess::SleepNoStats(SHORT_SLEEP_MS);}else{FPlatformProcess::YieldThread();}\
}else if (_WAIT_COUNTER_ < LONG_SLEEP_THRESHOLD){ FPlatformProcess::SleepNoStats(SHORT_SLEEP_MS);}else{FPlatformProcess::SleepNoStats(LONG_SLEEP_MS);} ++_WAIT_COUNTER_;}
