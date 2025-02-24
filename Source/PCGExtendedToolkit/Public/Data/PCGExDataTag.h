// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExMacros.h"
#include "PCGExHelpers.h"
#include "Kismet/KismetStringLibrary.h"

//#include "Data/PCGExDataTag.generated.h"

#define PCGEX_FOREACH_SUPPORTEDTAGTYPE(MACRO) \
MACRO(Integer, int64) \
MACRO(FloatingPoint, double) \
MACRO(String, FString) \
MACRO(Vector2, FVector2D) \
MACRO(Vector, FVector) \
MACRO(Vector4, FVector4)

UENUM()
enum class EPCGExSupportedTagValue : uint8
{
	Integer       = 0 UMETA(DisplayName = "Integer", ToolTip="Int32 / Int64"),
	FloatingPoint = 1 UMETA(DisplayName = "Floating Point", ToolTip="Double / float"),
	String        = 2 UMETA(DisplayName = "String", ToolTip="String"),
	Vector2       = 3 UMETA(DisplayName = "Vector 2D", ToolTip="Vector 2D"),
	Vector        = 4 UMETA(DisplayName = "Vector", ToolTip="Vector"),
	Vector4       = 5 UMETA(DisplayName = "Vector 4", ToolTip="Vector 4"),
};

namespace PCGExTags
{
	class PCGEXTENDEDTOOLKIT_API FTagValue : public TSharedFromThis<FTagValue>
	{
	public:
		virtual ~FTagValue() = default;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;

		explicit FTagValue()
		{
		}

		virtual FString Flatten(const FString& LeftSide) = 0;

		virtual bool IsNumeric() const = 0;
		virtual bool IsText() const = 0;

		virtual double AsDouble() const = 0;
		virtual FString AsString() const = 0;

		bool SameValue(const TSharedPtr<FTagValue>& Other) const;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API TTagValue : public FTagValue
	{
	public:
		T Value;

		explicit TTagValue(const T& InValue)
			: FTagValue(), Value(InValue)
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

		virtual double AsDouble() const override
		{
			if constexpr (std::is_same_v<T, bool>) { return Value ? 1 : 0; }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64> || std::is_same_v<T, float> || std::is_same_v<T, double>) { return static_cast<double>(Value); }
			else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4>) { return Value.X; }
			else { return 0; }
		}

		virtual FString AsString() const override
		{
			if constexpr (std::is_same_v<T, bool>) { return Value ? TEXT("true") : TEXT("false"); }
			else if constexpr (std::is_same_v<T, FName>) { return Value.ToString(); }
			else if constexpr (std::is_same_v<T, FString>) { return Value; }
			else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) { return FString::Printf(TEXT("%.2f"), Value); }
			else if constexpr (std::is_same_v<T, int32> || std::is_same_v<T, int64>) { return FString::Printf(TEXT("%d"), Value); }
			else if constexpr (std::is_same_v<T, FVector2D> || std::is_same_v<T, FVector> || std::is_same_v<T, FVector4>) { return FString::Printf(TEXT("%s"), *Value.ToString()); }
			else if constexpr (std::is_same_v<T, FString>) { return FString::Printf(TEXT("%s"), *Value); }
			else { return TEXT(""); }
		}
	};

	TSharedPtr<FTagValue> TryGetValueTag(const FString& InTag, FString& OutLeftSide);

	using IDType = TSharedPtr<TTagValue<int32>>;
}

namespace PCGExData
{
	using namespace PCGExTags;
	const FString TagSeparator = FSTRING(":");

	class PCGEXTENDEDTOOLKIT_API FTags : public TSharedFromThis<FTags>
	{
		mutable FRWLock TagsLock;

	public:
		TSet<FString> RawTags;                          // Contains all data tag
		TMap<FString, TSharedPtr<FTagValue>> ValueTags; // Prefix:ValueTag

		int32 Num() const;
		bool IsEmpty() const;

		FTags();
		explicit FTags(const TSet<FString>& InTags);
		explicit FTags(const TSharedPtr<FTags>& InTags);

		void Append(const TSharedRef<FTags>& InTags);
		void Append(const TArray<FString>& InTags);
		void Append(const TSet<FString>& InTags);

		void Reset();
		void Reset(const TSharedPtr<FTags>& InTags);

		void DumpTo(TSet<FString>& InTags, const bool bFlatten = true) const;
		void DumpTo(TArray<FName>& InTags, const bool bFlatten = true) const;


		TSet<FString> Flatten();
		TArray<FString> FlattenToArray(const bool bIncludeValue = true) const;
		TArray<FName> FlattenToArrayOfNames(const bool bIncludeValue = true) const;

		~FTags() = default;

		/* Simple add to raw tags */
		void AddRaw(const FString& Key);

		template <typename T>
		TSharedPtr<TTagValue<T>> GetOrSet(const FString& Key, const T& Value)
		{
			{
				FReadScopeLock ReadScopeLock(TagsLock);

				TArray<T> ValuesForKey;
				if (const TSharedPtr<FTagValue>* ValueTagPtr = ValueTags.Find(Key))
				{
					if ((*ValueTagPtr)->UnderlyingType == PCGEx::GetMetadataType<T>())
					{
						return StaticCastSharedPtr<TTagValue<T>>(*ValueTagPtr);
					}
				}
			}
			{
				FWriteScopeLock WriteScopeLock(TagsLock);

				const TSharedPtr<TTagValue<T>> ValueTag = MakeShared<TTagValue<T>>(Value);
				ValueTags.Add(Key, ValueTag);
				return ValueTag;
			}
		}

		template <typename T>
		TSharedPtr<TTagValue<T>> Set(const FString& Key, const T& Value)
		{
			const TSharedPtr<TTagValue<T>> ValueTag = GetOrSet(Key, Value);
			ValueTag->Value = Value;
			return ValueTag;
		}

		template <typename T>
		TSharedPtr<TTagValue<T>> Set(const FString& Key, const TSharedPtr<TTagValue<T>>& Value)
		{
			return Set<T>(Key, Value->Value);
		}

		void Remove(const FString& Key);
		void Remove(const TSet<FString>& InSet);
		void Remove(const TSet<FName>& InSet);

		template <typename T>
		TSharedPtr<TTagValue<T>> GetTypedValue(const FString& Key)
		{
			FReadScopeLock ReadScopeLock(TagsLock);

			if (const TSharedPtr<FTagValue>* ValueTagPtr = ValueTags.Find(Key))
			{
				if ((*ValueTagPtr)->UnderlyingType == PCGEx::GetMetadataType<T>())
				{
					return StaticCastSharedPtr<TTagValue<T>>(*ValueTagPtr);
				}
			}

			return nullptr;
		}

		TSharedPtr<FTagValue> GetValue(const FString& Key);
		bool IsTagged(const FString& Key) const;

	protected:
		void ParseAndAdd(const FString& InTag);

		// NAME:VALUE
		static bool GetTagFromString(const FString& Input, FString& OutKey, FString& OutValue);
	};
}
