﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

// Boilerplate forward declaration, almost always required when using settings.

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

#ifndef PCGEX_SETTINGS_MACROS
#define PCGEX_SETTINGS_MACROS

#define PCGEX_SETTING_VALUE_DECL(_NAME, _TYPE) TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetValueSetting##_NAME() const;

#define PCGEX_SETTING_VALUE_IMPL(_CLASS, _NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT)\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> _CLASS::GetValueSetting##_NAME() const{ \
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> V = PCGExDetails::MakeSettingValue<_TYPE>(_INPUT, _SOURCE, _CONSTANT);\
return V; }

#define PCGEX_SETTING_VALUE_IMPL_BOOL(_CLASS, _NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT) PCGEX_SETTING_VALUE_IMPL(_CLASS, _NAME, _TYPE, ((_INPUT) ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant), _SOURCE, _CONSTANT);

#define PCGEX_SETTING_VALUE_INLINE(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT)\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetValueSetting##_NAME() const{ \
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> V = PCGExDetails::MakeSettingValue<_TYPE>(_INPUT, _SOURCE, _CONSTANT);\
return V; }


#define PCGEX_SETTING_DATA_VALUE_DECL(_NAME, _TYPE) TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetValueSetting##_NAME(FPCGExContext* InContext, const UPCGData* InData) const;

#define PCGEX_SETTING_DATA_VALUE_IMPL(_CLASS, _NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT)\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> _CLASS::GetValueSetting##_NAME(FPCGExContext* InContext, const UPCGData* InData) const{\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> V = PCGExDetails::MakeSettingValue<_TYPE>(InContext, InData, _INPUT, _SOURCE, _CONSTANT);\
return V; }

#define PCGEX_SETTING_DATA_VALUE_IMPL_BOOL(_CLASS, _NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT) PCGEX_SETTING_DATA_VALUE_IMPL(_CLASS, _NAME, _TYPE, ((_INPUT) ? EPCGExInputValueType::Attribute : EPCGExInputValueType::Constant), _SOURCE, _CONSTANT);

#define PCGEX_SETTING_DATA_VALUE_INLINE(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT)\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetValueSetting##_NAME(FPCGExContext* InContext, const UPCGData* InData) const{\
TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> V = PCGExDetails::MakeSettingValue<_TYPE>(InContext, InData, _INPUT, _SOURCE, _CONSTANT);\
return V; }

#endif
