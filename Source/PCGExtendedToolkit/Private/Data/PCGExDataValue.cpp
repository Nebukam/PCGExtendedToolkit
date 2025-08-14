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

	template <typename T>
	T IDataValue::GetValue()
	{
		if (IsNumeric()) { return PCGEx::Convert<double, T>(AsDouble()); }
		return PCGEx::Convert<FString, T>(AsString());
	}

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template PCGEXTENDEDTOOLKIT_API _TYPE IDataValue::GetValue<_TYPE>();
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

	template <typename T>
	TDataValue<T>::TDataValue(const T& InValue)
	: IDataValue(), Value(InValue)
	{
		UnderlyingType = PCGEx::GetMetadataType<T>();
	}

	template <typename T>
	FString TDataValue<T>::Flatten(const FString& LeftSide)
	{
		if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
		{
			return FString::Printf(TEXT("%s:%.2f"), *LeftSide, Value);
		}
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64>)
		{
			return FString::Printf(TEXT("%s:%d"), *LeftSide, Value);
		}
		else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4>)
		{
			return FString::Printf(TEXT("%s:%s"), *LeftSide, *Value.ToString());
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return FString::Printf(TEXT("%s:%s"), *LeftSide, *Value);
		}
		else
		{
			return LeftSide;
		}
	}

	template <typename T>
	bool TDataValue<T>::IsNumeric() const
	{
		if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return true; }
		else { return false; }
	}

	template <typename T>
	bool TDataValue<T>::IsText() const
	{
		if constexpr (std::is_same_v<T, FString> || std::is_same_v<T, FSoftClassPath> || std::is_same_v<T, FSoftObjectPath> || std::is_same_v<T, FName>) { return true; }
		else { return false; }
	}

	template <typename T>
	double TDataValue<T>::AsDouble()
	{
		if (CachedDouble.IsSet()) { return CachedDouble.GetValue(); }

		double V = 0;

		if constexpr (std::is_same_v<T, bool>) { V = Value ? 1 : 0; }
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { V = static_cast<double>(Value); }
		else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4>) { V = Value.X; }

		CachedDouble.Emplace(V);
		return V;
	}

	template <typename T>
	FString TDataValue<T>::AsString()
	{
		if (CachedString.IsSet()) { return CachedString.GetValue(); }

		FString V = TEXT("");

		if constexpr (std::is_same_v<T, bool>) { V = Value ? TEXT("true") : TEXT("false"); }
		else if constexpr (std::is_same_v<T, FName>) { V = Value.ToString(); }
		else if constexpr (std::is_same_v<T, FString>) { V = Value; }
		else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) { V = FString::Printf(TEXT("%.2f"), Value); }
		else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64>) { V = FString::Printf(TEXT("%d"), Value); }
		else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4>) { V = FString::Printf(TEXT("%s"), *Value.ToString()); }
		else if constexpr (std::is_same_v<T, FString>) { V = FString::Printf(TEXT("%s"), *Value); }

		CachedString.Emplace(V);
		return V;
	}

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template class PCGEXTENDEDTOOLKIT_API TDataValue<_TYPE>;

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

		FPCGAttributeIdentifier SanitizedIdentifier = PCGEx::GetAttributeIdentifier(Selector, InData);
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
								TypedDataValue = MakeShared<TDataValue<T_WORKING>>(SubSelection.Get<T, T_WORKING>(Value));
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
