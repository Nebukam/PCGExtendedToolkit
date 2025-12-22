// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SETTING_CACHE_BODY(_MODULE)\
static FPCGEx##_MODULE##SettingsCache& Get()\
{\
	static FPCGEx##_MODULE##SettingsCache Instance;\
	return Instance;\
}\
private:\
	FPCGEx##_MODULE##SettingsCache() = default;\
public:

#define PCGEX_SETTINGS_INST(_MODULE) FPCGEx##_MODULE##SettingsCache::Get()
