// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataHelpers.h"

#include "PCGExLog.h"
#include "Data/PCGExSubSelection.h"
#include "Data/PCGExPointIO.h"
#include "Types/PCGExTypes.h"

namespace PCGExData::Helpers
{
	template <typename T>
	T ReadDataValue(const FPCGMetadataAttribute<T>* Attribute)
	{
		const FPCGMetadataAttribute<T>* Attr = Attribute;
		if (!Attr->GetNumberOfEntries())
		{
			const FPCGMetadataAttribute<T>* Parent = Attr->GetParent();
			while (Parent)
			{
				if (!Parent->GetNumberOfEntries()) { Parent = Parent->GetParent(); }
				else
				{
					Attr = Parent;
					Parent = nullptr;
				}
			}
		}
		return !Attr->GetNumberOfEntries() ? Attr->GetValue(PCGDefaultValueKey) : Attr->GetValueFromItemKey(PCGFirstEntryKey);
	}

	template <typename T>
	T ReadDataValue(const FPCGMetadataAttributeBase* Attribute, T Fallback)
	{
		T Value = Fallback;
		PCGExMetaHelpers::ExecuteWithRightType(Attribute->GetTypeId(), [&](auto DummyValue)
		{
			using T_VALUE = decltype(DummyValue);
			Value = PCGExTypeOps::Convert<T_VALUE, T>(ReadDataValue<T_VALUE>(static_cast<const FPCGMetadataAttribute<T_VALUE>*>(Attribute)));
		});
		return Value;
	}

	template <typename T>
	void SetDataValue(FPCGMetadataAttribute<T>* Attribute, const T Value)
	{
		Attribute->SetValue(PCGFirstEntryKey, Value);
		Attribute->SetDefaultValue(Value);
	}

	template <typename T>
	void SetDataValue(UPCGData* InData, FName Name, const T Value)
	{
		FPCGAttributePropertyInputSelector SafetySelector;
		SafetySelector.Update(Name.ToString());

		if (SafetySelector.GetSelection() != EPCGAttributePropertySelection::Attribute)
		{
			UE_LOG(LogPCGEx, Error, TEXT("Attempting to write @Data value to a non-attribute domain."))
			return;
		}

		FPCGAttributeIdentifier Identifier = FPCGAttributeIdentifier(SafetySelector.GetAttributeName(), EPCGMetadataDomainFlag::Data);
		SetDataValue<T>(InData->Metadata->FindOrCreateAttribute<T>(Identifier, Value, true, true), Value);
	}

	template <typename T>
	void SetDataValue(UPCGData* InData, FPCGAttributeIdentifier Identifier, const T Value)
	{
		SetDataValue<T>(InData, Identifier.Name, Value);
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXCORE_API _TYPE ReadDataValue<_TYPE>(const FPCGMetadataAttribute<_TYPE>* Attribute); \
template PCGEXCORE_API _TYPE ReadDataValue<_TYPE>(const FPCGMetadataAttributeBase* Attribute, _TYPE Fallback); \
template PCGEXCORE_API void SetDataValue<_TYPE>(FPCGMetadataAttribute<_TYPE>* Attribute, const _TYPE Value); \
template PCGEXCORE_API void SetDataValue<_TYPE>(UPCGData* InData, FName Name, const _TYPE Value); \
template PCGEXCORE_API void SetDataValue<_TYPE>(UPCGData* InData, FPCGAttributeIdentifier Identifier, const _TYPE Value);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	template <typename T>
	bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue, const bool bQuiet)
	{
		bool bSuccess = false;
		const UPCGMetadata* InMetadata = InData->Metadata;

		if (!InMetadata) { return false; }

		FSubSelection SubSelection(InSelector);
		FPCGAttributeIdentifier SanitizedIdentifier = PCGExMetaHelpers::GetAttributeIdentifier(InSelector, InData);
		SanitizedIdentifier.MetadataDomain = EPCGMetadataDomainFlag::Data; // Force data domain

		if (const FPCGMetadataAttributeBase* SourceAttribute = InMetadata->GetConstAttribute(SanitizedIdentifier))
		{
			PCGExMetaHelpers::ExecuteWithRightType(SourceAttribute->GetTypeId(), [&](auto DummyValue)
			{
				using T_VALUE = decltype(DummyValue);

				const FPCGMetadataAttribute<T_VALUE>* TypedSource = static_cast<const FPCGMetadataAttribute<T_VALUE>*>(SourceAttribute);
				const T_VALUE Value = ReadDataValue(TypedSource);

				if (SubSelection.bIsValid) { OutValue = SubSelection.Get<T_VALUE, T>(Value); }
				else { OutValue = PCGExTypeOps::Convert<T_VALUE, T>(Value); }

				bSuccess = true;
			});
		}
		else
		{
			if (!bQuiet && InContext) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, Attribute, InSelector) }
			return false;
		}

		return bSuccess;
	}

	template <typename T>
	bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FName& InName, T& OutValue, const bool bQuiet)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return TryReadDataValue<T>(InContext, InData, Selector.CopyAndFixLast(InData), OutValue, bQuiet);
	}

	template <typename T>
	bool TryReadDataValue(const TSharedPtr<FPointIO>& InIO, const FName& InName, T& OutValue, const bool bQuiet)
	{
		return TryReadDataValue(InIO->GetContext(), InIO->GetIn(), InName, OutValue, bQuiet);
	}

	template <typename T>
	bool TryReadDataValue(const TSharedPtr<FPointIO>& InIO, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue, const bool bQuiet)
	{
		return TryReadDataValue(InIO->GetContext(), InIO->GetIn(), InSelector, OutValue, bQuiet);
	}

	template <typename T>
	bool TryGetSettingDataValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue, const bool bQuiet)
	{
		if (Input == EPCGExInputValueType::Constant)
		{
			OutValue = InConstant;
			return true;
		}

		return TryReadDataValue<T>(InContext, InData, InSelector, OutValue, bQuiet);
	}

	template <typename T>
	bool TryGetSettingDataValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FName& InName, const T& InConstant, T& OutValue, const bool bQuiet)
	{
		if (Input == EPCGExInputValueType::Constant)
		{
			OutValue = InConstant;
			return true;
		}

		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return TryReadDataValue<T>(InContext, InData, Selector.CopyAndFixLast(InData), OutValue, bQuiet);
	}

	template <typename T>
	bool TryGetSettingDataValue(const TSharedPtr<FPointIO>& InIO, const EPCGExInputValueType Input, const FName& InName, const T& InConstant, T& OutValue, const bool bQuiet)
	{
		return TryGetSettingDataValue(InIO->GetContext(), InIO->GetIn(), Input, InName, InConstant, OutValue, bQuiet);
	}

	template <typename T>
	bool TryGetSettingDataValue(const TSharedPtr<FPointIO>& InIO, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue, const bool bQuiet)
	{
		return TryGetSettingDataValue(InIO->GetContext(), InIO->GetIn(), Input, InSelector, InConstant, OutValue, bQuiet);
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXCORE_API bool TryReadDataValue<_TYPE>(FPCGExContext* InContext, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, _TYPE& OutValue, const bool bQuiet); \
template PCGEXCORE_API bool TryReadDataValue<_TYPE>(FPCGExContext* InContext, const UPCGData* InData, const FName& InName, _TYPE& OutValue, const bool bQuiet); \
template PCGEXCORE_API bool TryReadDataValue<_TYPE>(const TSharedPtr<PCGExData::FPointIO>& InIO, const FName& InName, _TYPE& OutValue, const bool bQuiet); \
template PCGEXCORE_API bool TryReadDataValue<_TYPE>(const TSharedPtr<PCGExData::FPointIO>& InIO, const FPCGAttributePropertyInputSelector& InSelector, _TYPE& OutValue, const bool bQuiet); \
template PCGEXCORE_API bool TryGetSettingDataValue<_TYPE>( FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE& InConstant, _TYPE& OutValue, const bool bQuiet); \
template PCGEXCORE_API bool TryGetSettingDataValue<_TYPE>( FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FName& InName, const _TYPE& InConstant, _TYPE& OutValue, const bool bQuiet); \
template PCGEXCORE_API bool TryGetSettingDataValue<_TYPE>( const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input, const FName& InName, const _TYPE& InConstant, _TYPE& OutValue, const bool bQuiet); \
template PCGEXCORE_API bool TryGetSettingDataValue<_TYPE>( const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE& InConstant, _TYPE& OutValue, const bool bQuiet);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
