// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExHelpers.h"
#include "PCGExPointIO.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExDetails.h"

namespace PCGExDataHelpers
{
	template <typename T>
	static bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue)
	{
		bool bSuccess = false;
		const UPCGMetadata* InMetadata = InData->Metadata;

		if (!InMetadata) { return false; }

		PCGEx::FSubSelection SubSelection(InSelector);
		FPCGAttributeIdentifier SanitizedIdentifier = PCGEx::GetAttributeIdentifier<true>(InSelector, InData);

		if (const FPCGMetadataAttributeBase* SourceAttribute = InMetadata->GetConstAttribute(SanitizedIdentifier))
		{
			PCGEx::ExecuteWithRightType(
				SourceAttribute->GetTypeId(), [&](auto DummyValue)
				{
					using T_VALUE = decltype(DummyValue);

					const FPCGMetadataAttribute<T_VALUE>* TypedSource = static_cast<const FPCGMetadataAttribute<T_VALUE>*>(SourceAttribute);
					const T_VALUE Value = PCGEX_READ_DATA_ENTRY(TypedSource);

					if (SubSelection.bIsValid) { OutValue = SubSelection.Get<T_VALUE, T>(Value); }
					else { OutValue = PCGEx::Convert<T_VALUE, T>(Value); }

					bSuccess = true;
				});
		}
		else
		{
			PCGEX_LOG_INVALID_SELECTOR_C(InContext, , InSelector)
			return false;
		}

		return bSuccess;
	}

	template <typename T>
	static bool TryReadDataValue(FPCGExContext* InContext, const UPCGData* InData, const FName& InName, T& OutValue)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());
		return TryReadDataValue<T>(InContext, InData, Selector.CopyAndFixLast(InData), OutValue);
	}

	template <typename T>
	static bool TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& InIO, const FName& InName, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryReadDataValue(SharedContext.Get(), InIO->GetIn(), InName, OutValue);
	}

	template <typename T>
	static bool TryReadDataValue(const TSharedPtr<PCGExData::FPointIO>& InIO, const FPCGAttributePropertyInputSelector& InSelector, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryReadDataValue(SharedContext.Get(), InIO->GetIn(), InSelector, OutValue);
	}

	template <typename T>
	static bool TryGetSettingDataValue(
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
	static bool TryGetSettingDataValue(
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
	static bool TryGetSettingDataValue(
		const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input,
		const FName& InName, const T& InConstant, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryGetSettingDataValue(SharedContext.Get(), InIO->GetIn(), Input, InName, InConstant, OutValue);
	}

	template <typename T>
	static bool TryGetSettingDataValue(
		const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExInputValueType Input,
		const FPCGAttributePropertyInputSelector& InSelector, const T& InConstant, T& OutValue)
	{
		PCGEX_SHARED_CONTEXT(InIO->GetContextHandle())
		return TryGetSettingDataValue(SharedContext.Get(), InIO->GetIn(), Input, InSelector, InConstant, OutValue);
	}
}
