// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBroadcast.h"
#include "UObject/Object.h"
#include "PCGExHelpers.h"

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
		T GetValue()
		{
			if (IsNumeric()) { return PCGEx::Convert<double, T>(AsDouble()); }
			return PCGEx::Convert<FString, T>(AsString());
		}

	protected:
		TOptional<double> CachedDouble;
		TOptional<FString> CachedString;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API TDataValue : public IDataValue
	{
	public:
		T Value;

		explicit TDataValue(const T& InValue)
			: IDataValue(), Value(InValue)
		{
			UnderlyingType = PCGEx::GetMetadataType<T>();
		}

		virtual FString Flatten(const FString& LeftSide) override
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

		virtual bool IsNumeric() const override
		{
			if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return true; }
			else { return false; }
		}

		virtual bool IsText() const override
		{
			if constexpr (std::is_same_v<T, FString> || std::is_same_v<T, FSoftClassPath> || std::is_same_v<T, FSoftObjectPath> || std::is_same_v<T, FName>) { return true; }
			else { return false; }
		}

		virtual double AsDouble() override
		{
			if (CachedDouble.IsSet()) { return CachedDouble.GetValue(); }

			double V = 0;

			if constexpr (std::is_same_v<T, bool>) { V = Value ? 1 : 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { V = static_cast<double>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4>) { V = Value.X; }

			CachedDouble.Emplace(V);
			return V;
		}

		virtual FString AsString() override
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
	};

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template class TDataValue<_TYPE>;
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
