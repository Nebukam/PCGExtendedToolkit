// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataHelpers.h"

#include "PCGExBroadcast.h"
#include "PCGExHelpers.h"
#include "Data/PCGExPointIO.h"

namespace PCGExDataHelpers
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
		PCGEx::ExecuteWithRightType(
			Attribute->GetTypeId(), [&](auto DummyValue)
			{
				using T_VALUE = decltype(DummyValue);
				Value = PCGEx::Convert<T_VALUE, T>(ReadDataValue<T_VALUE>(static_cast<const FPCGMetadataAttribute<T_VALUE>*>(Attribute)));
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
		FPCGAttributeIdentifier Identifier = FPCGAttributeIdentifier(Name, EPCGMetadataDomainFlag::Data);
		SetDataValue<T>(InData->Metadata->FindOrCreateAttribute<T>(Identifier, Value, true, true), Value);
	}

	template <typename T>
	void SetDataValue(UPCGData* InData, FPCGAttributeIdentifier Identifier, const T Value)
	{
		SetDataValue<T>(InData, Identifier.Name, Value);
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API _TYPE ReadDataValue<_TYPE>(const FPCGMetadataAttribute<_TYPE>* Attribute); \
template PCGEXTENDEDTOOLKIT_API _TYPE ReadDataValue<_TYPE>(const FPCGMetadataAttributeBase* Attribute, _TYPE Fallback); \
template PCGEXTENDEDTOOLKIT_API void SetDataValue<_TYPE>(FPCGMetadataAttribute<_TYPE>* Attribute, const _TYPE Value); \
template PCGEXTENDEDTOOLKIT_API void SetDataValue<_TYPE>(UPCGData* InData, FName Name, const _TYPE Value); \
template PCGEXTENDEDTOOLKIT_API void SetDataValue<_TYPE>(UPCGData* InData, FPCGAttributeIdentifier Identifier, const _TYPE Value);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	template <typename T>
	bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue)
	{
		bool bSuccess = false;
		const UPCGMetadata* InMetadata = InData->Metadata;

		if (!InMetadata) { return false; }

		PCGEx::FSubSelection SubSelection(InSelector);
		FPCGAttributeIdentifier SanitizedIdentifier = PCGEx::GetAttributeIdentifier(InSelector, InData);
		SanitizedIdentifier.MetadataDomain = EPCGMetadataDomainFlag::Data; // Force data domain

		if (const FPCGMetadataAttributeBase* SourceAttribute = InMetadata->GetConstAttribute(SanitizedIdentifier))
		{
			PCGEx::ExecuteWithRightType(
				SourceAttribute->GetTypeId(), [&](auto DummyValue)
				{
					using T_VALUE = decltype(DummyValue);

					const FPCGMetadataAttribute<T_VALUE>* TypedSource = static_cast<const FPCGMetadataAttribute<T_VALUE>*>(SourceAttribute);
					const T_VALUE Value = ReadDataValue(TypedSource);

					if (SubSelection.bIsValid) { OutValue = SubSelection.Get<T_VALUE, T>(Value); }
					else { OutValue = PCGEx::Convert<T_VALUE, T>(Value); }

					bSuccess = true;
				});
		}
		else
		{
			if (InContext) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, Attribute, InSelector) }
			return false;
		}

		return bSuccess;
	}

	template <typename T>
	bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FName& InName, T& OutValue)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return TryReadDataValue<T>(InContext, InData, Selector.CopyAndFixLast(InData), OutValue);
	}

	template <typename T>
	bool TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& InIO, const FName& InName, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryReadDataValue(SharedContext.Get(), InIO->GetIn(), InName, OutValue);
	}

	template <typename T>
	bool TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& InIO, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryReadDataValue(SharedContext.Get(), InIO->GetIn(), InSelector, OutValue);
	}

	template <typename T>
	bool TryGetSettingDataValue(
		FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input,
		const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue)
	{
		if (Input == EPCGExInputValueType::Constant)
		{
			OutValue = InConstant;
			return true;
		}

		return TryReadDataValue<T>(InContext, InData, InSelector, OutValue);
	}

	template <typename T>
	bool TryGetSettingDataValue(
		FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input,
		const FName& InName, const T& InConstant, T& OutValue)
	{
		if (Input == EPCGExInputValueType::Constant)
		{
			OutValue = InConstant;
			return true;
		}

		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return TryReadDataValue<T>(InContext, InData, Selector.CopyAndFixLast(InData), OutValue);
	}

	template <typename T>
	bool TryGetSettingDataValue(
		const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input,
		const FName& InName, const T& InConstant, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryGetSettingDataValue(SharedContext.Get(), InIO->GetIn(), Input, InName, InConstant, OutValue);
	}

	template <typename T>
	bool TryGetSettingDataValue(
		const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input,
		const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryGetSettingDataValue(SharedContext.Get(), InIO->GetIn(), Input, InSelector, InConstant, OutValue);
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API bool TryReadDataValue<_TYPE>(FPCGExContext* InContext, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, _TYPE& OutValue); \
template PCGEXTENDEDTOOLKIT_API bool TryReadDataValue<_TYPE>(FPCGExContext* InContext, const UPCGData* InData, const FName& InName, _TYPE& OutValue); \
template PCGEXTENDEDTOOLKIT_API bool TryReadDataValue<_TYPE>(const TSharedPtr<PCGExData::FPointIO>& InIO, const FName& InName, _TYPE& OutValue); \
template PCGEXTENDEDTOOLKIT_API bool TryReadDataValue<_TYPE>(const TSharedPtr<PCGExData::FPointIO>& InIO, const FPCGAttributePropertyInputSelector& InSelector, _TYPE& OutValue); \
template PCGEXTENDEDTOOLKIT_API bool TryGetSettingDataValue<_TYPE>( FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE& InConstant, _TYPE& OutValue); \
template PCGEXTENDEDTOOLKIT_API bool TryGetSettingDataValue<_TYPE>( FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType Input, const FName& InName, const _TYPE& InConstant, _TYPE& OutValue); \
template PCGEXTENDEDTOOLKIT_API bool TryGetSettingDataValue<_TYPE>( const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input, const FName& InName, const _TYPE& InConstant, _TYPE& OutValue); \
template PCGEXTENDEDTOOLKIT_API bool TryGetSettingDataValue<_TYPE>( const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE& InConstant, _TYPE& OutValue);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
