﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Details/PCGExMacros.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "UObject/Object.h"

class UPCGData;
struct FPCGAttributePropertyInputSelector;
enum class EPCGMetadataTypes : uint8;

namespace PCGExData
{
	class PCGEXTENDEDTOOLKIT_API IDataValue : public TSharedFromThis<IDataValue>
	{
	public:
		virtual ~IDataValue() = default;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;

		IDataValue() = default;

		virtual FString Flatten(const FString& LeftSide) = 0;

		virtual bool IsNumeric() const = 0;
		virtual bool IsText() const = 0;

		virtual double AsDouble() = 0;
		virtual FString AsString() = 0;

		bool SameValue(const TSharedPtr<IDataValue>& Other);

		template <typename T>
		T GetValue();

	protected:
		TOptional<double> CachedDouble;
		TOptional<FString> CachedString;
	};

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
extern template _TYPE IDataValue::GetValue<_TYPE>();
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

	template <typename T>
	class TDataValue : public IDataValue
	{
	public:
		T Value;

		explicit TDataValue(const T& InValue);


		virtual FString Flatten(const FString& LeftSide) override;

		virtual bool IsNumeric() const override;
		virtual bool IsText() const override;

		virtual double AsDouble() override;
		virtual FString AsString() override;
	};

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
extern template class TDataValue<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<IDataValue> TryGetValueFromTag(const FString& InTag, FString& OutLeftSide);

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<IDataValue> TryGetValueFromData(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector);

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<IDataValue> TryGetValueFromData(const UPCGData* InData, const FName& InName);
}
