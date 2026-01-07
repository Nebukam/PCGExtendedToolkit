// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataCommon.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Helpers/PCGExMetaHelpers.h"

enum class EPCGExInputValueType : uint8;
struct FPCGExContext;

namespace PCGExData
{
	class FPointIO;
}

namespace PCGExData::Helpers
{
	template <typename T>
	T ReadDataValue(const FPCGMetadataAttribute<T>* Attribute);

	template <typename T>
	T ReadDataValue(const FPCGMetadataAttributeBase* Attribute, T Fallback);

	template <typename T>
	void SetDataValue(FPCGMetadataAttribute<T>* Attribute, const T Value);

	template <typename T>
	void SetDataValue(UPCGData* InData, FName Name, const T Value);

	template <typename T>
	void SetDataValue(UPCGData* InData, FPCGAttributeIdentifier Identifier, const T Value);

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template _TYPE ReadDataValue<_TYPE>(const FPCGMetadataAttribute<_TYPE>* Attribute); \
extern template _TYPE ReadDataValue<_TYPE>(const FPCGMetadataAttributeBase* Attribute, _TYPE Fallback); \
extern template void SetDataValue<_TYPE>(FPCGMetadataAttribute<_TYPE>* Attribute, const _TYPE Value); \
extern template void SetDataValue<_TYPE>(UPCGData* InData, FName Name, const _TYPE Value); \
extern template void SetDataValue<_TYPE>(UPCGData* InData, FPCGAttributeIdentifier Identifier, const _TYPE Value);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	constexpr static EPCGMetadataTypes GetNumericType(const EPCGExNumericOutput InType)
	{
		switch (InType)
		{
		case EPCGExNumericOutput::Double: return EPCGMetadataTypes::Double;
		case EPCGExNumericOutput::Float: return EPCGMetadataTypes::Float;
		case EPCGExNumericOutput::Int32: return EPCGMetadataTypes::Integer32;
		case EPCGExNumericOutput::Int64: return EPCGMetadataTypes::Integer64;
		}

		return EPCGMetadataTypes::Unknown;
	}

	template <typename T>
	bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue, const bool bQuiet = false);

	template <typename T>
	bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FName& InName, T& OutValue, const bool bQuiet = false);

	template <typename T>
	bool TryReadDataValue(const TSharedPtr<FPointIO>& InIO, const FName& InName, T& OutValue, const bool bQuiet = false);

	template <typename T>
	bool TryReadDataValue(const TSharedPtr<FPointIO>& InIO, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue, const bool bQuiet = false);

	template <typename T>
	bool TryGetSettingDataValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue, const bool bQuiet = false);

	template <typename T>
	bool TryGetSettingDataValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FName& InName, const T& InConstant, T& OutValue, const bool bQuiet = false);

	template <typename T>
	bool TryGetSettingDataValue(const TSharedPtr<FPointIO>& InIO, const EPCGExInputValueType Input, const FName& InName, const T& InConstant, T& OutValue, const bool bQuiet = false);

	template <typename T>
	bool TryGetSettingDataValue(const TSharedPtr<FPointIO>& InIO, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue, const bool bQuiet = false);

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template bool TryReadDataValue<_TYPE>(FPCGExContext* InContext, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, _TYPE& OutValue, const bool bQuiet); \
extern template bool TryReadDataValue<_TYPE>(FPCGExContext* InContext, const UPCGData* InData, const FName& InName, _TYPE& OutValue, const bool bQuiet); \
extern template bool TryReadDataValue<_TYPE>(const TSharedPtr<PCGExData::FPointIO>& InIO, const FName& InName, _TYPE& OutValue, const bool bQuiet); \
extern template bool TryReadDataValue<_TYPE>(const TSharedPtr<PCGExData::FPointIO>& InIO, const FPCGAttributePropertyInputSelector& InSelector, _TYPE& OutValue, const bool bQuiet); \
extern template bool TryGetSettingDataValue<_TYPE>( FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE& InConstant, _TYPE& OutValue, const bool bQuiet); \
extern template bool TryGetSettingDataValue<_TYPE>( FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FName& InName, const _TYPE& InConstant, _TYPE& OutValue, const bool bQuiet); \
extern template bool TryGetSettingDataValue<_TYPE>( const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input, const FName& InName, const _TYPE& InConstant, _TYPE& OutValue, const bool bQuiet); \
extern template bool TryGetSettingDataValue<_TYPE>( const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE& InConstant, _TYPE& OutValue, const bool bQuiet);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
