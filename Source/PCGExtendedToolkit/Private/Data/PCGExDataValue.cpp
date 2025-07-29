// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataValue.h"

#include "PCGExBroadcast.h"
#include "Data/PCGExDataHelpers.h"

namespace PCGExData
{
	bool IDataValue::SameValue(const TSharedPtr<IDataValue>& Other)
	{
		if (IsNumeric() && Other->IsNumeric()) { return AsDouble() == Other->AsDouble(); }
		if (IsText() && Other->IsText()) { return AsString() == Other->AsString(); }
		return false;
	}

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template class TDataValue<_TYPE>;

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL
	
	TSharedPtr<IDataValue> TryGetValueFromTag(const FString& InTag, FString& OutLeftSide)
	{
		int32 DividerPosition = INDEX_NONE;
		if (!InTag.FindChar(':', DividerPosition))
		{
			return nullptr;
		}

		OutLeftSide = InTag.Left(DividerPosition);
		FString RightSide = InTag.RightChop(DividerPosition + 1);

		if (OutLeftSide.IsEmpty())
		{
			return nullptr;
		}
		if (RightSide.IsEmpty())
		{
			return nullptr;
		}
		if (RightSide.IsNumeric())
		{
			int32 FloatingPointPosition = INDEX_NONE;
			if (InTag.FindChar('.', FloatingPointPosition))
			{
				return MakeShared<TDataValue<double>>(FCString::Atod(*RightSide));
			}
			return MakeShared<TDataValue<int32>>(FCString::Atoi(*RightSide));
		}

		if (FVector ParsedVector; ParsedVector.InitFromString(RightSide))
		{
			return MakeShared<TDataValue<FVector>>(ParsedVector);
		}

		if (FVector2D ParsedVector2D; ParsedVector2D.InitFromString(RightSide))
		{
			return MakeShared<TDataValue<FVector2D>>(ParsedVector2D);
		}

		if (FVector4 ParsedVector4; ParsedVector4.InitFromString(RightSide))
		{
			return MakeShared<TDataValue<FVector4>>(ParsedVector4);
		}

		return MakeShared<TDataValue<FString>>(RightSide);
	}

	TSharedPtr<IDataValue> TryGetValueFromData(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector)
	{
		TSharedPtr<IDataValue> DataValue = nullptr;
		const UPCGMetadata* InMetadata = InData->Metadata;

		if (!InMetadata) { return nullptr; }

		FPCGAttributePropertyInputSelector Selector = InSelector.CopyAndFixLast(InData);

		// Attribute unsupported
		if (Selector.GetSelection() != EPCGAttributePropertySelection::Attribute) { return nullptr; }

		FPCGAttributeIdentifier SanitizedIdentifier = PCGEx::GetAttributeIdentifier<true>(Selector, InData);
		SanitizedIdentifier.MetadataDomain = PCGMetadataDomainID::Data; // Force data domain

		// Non-data domain unsupported
		if (SanitizedIdentifier.MetadataDomain.Flag != EPCGMetadataDomainFlag::Data) { return nullptr; }

		if (const FPCGMetadataAttributeBase* SourceAttribute = InMetadata->GetConstAttribute(SanitizedIdentifier))
		{
			PCGEx::ExecuteWithRightType(
				SourceAttribute->GetTypeId(), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const T Value = PCGExDataHelpers::ReadDataValue<T>(static_cast<const FPCGMetadataAttribute<T>*>(SourceAttribute));

					PCGEx::FSubSelection SubSelection(Selector);
					TSharedPtr<IDataValue> TypedDataValue = nullptr;

					if (SubSelection.bIsValid)
					{
						PCGEx::ExecuteWithRightType(
							SourceAttribute->GetTypeId(), [&](auto WorkingValue)
							{
								using T_WORKING = decltype(DummyValue);
								TypedDataValue = MakeShared<TDataValue<T_WORKING>>(SubSelection.Get<T_WORKING>(Value));
							});
					}
					else { TypedDataValue = MakeShared<TDataValue<T>>(Value); }

					DataValue = TypedDataValue;
				});
		}

		return DataValue;
	}

	TSharedPtr<IDataValue> TryGetValueFromData(const UPCGData* InData, const FName& InName)
	{
		FPCGAttributePropertyInputSelector Selector;
		Selector.Update(InName.ToString());

		return TryGetValueFromData(InData, Selector);
	}
}
