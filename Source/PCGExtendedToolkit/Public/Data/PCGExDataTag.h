// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Kismet/KismetStringLibrary.h"

namespace PCGExData
{
	const FString TagSeparator = FSTRING("::");

	struct FTags
	{
		TSet<FString> RawTags;       // Contains all data tag
		TMap<FString, FString> Tags; // PCGEx Tags Name::Value

		FTags()
		{
			RawTags.Empty();
			Tags.Empty();
		}

		explicit FTags(const TSet<FString>& InTags)
			: FTags()
		{
			for (const FString& TagString : InTags)
			{
				FString InKey;
				FString InValue;

				if (!GetTagFromString(TagString, InKey, InValue))
				{
					RawTags.Add(TagString);
					continue;
				}

				//check(!Tags.Contains(InKey)) // Should not contain duplicate tag with different value

				Tags.Add(InKey, InValue);
			}
		}

		explicit FTags(const FTags& InTags)
			: FTags()
		{
			Reset(InTags);
		}

		void Append(const FTags* InTags)
		{
			Tags.Append(InTags->Tags);
			RawTags.Append(InTags->RawTags);
		}

		void Reset()
		{
			RawTags.Empty();
			Tags.Empty();
		}

		void Reset(const FTags& InTags)
		{
			Reset();
			RawTags.Append(InTags.RawTags);
			Tags.Append(InTags.Tags);
		}

		void Dump(TSet<FString>& InTags) const
		{
			InTags.Append(RawTags);
			for (const TPair<FString, FString>& Tag : Tags) { InTags.Add((Tag.Key + TagSeparator + Tag.Value)); }
		}

		~FTags()
		{
			RawTags.Empty();
			Tags.Empty();
		}

		void Set(const FString& Key, const FString& Value)
		{
			Tags.Add(Key, Value);
		}

		void Set(const FString& Key, const int64 Value, FString& OutValue)
		{
			OutValue = FString::Printf(TEXT("%llu"), Value);
			Tags.Add(Key, OutValue);
		}

		void Remove(const FString& Key)
		{
			Tags.Remove(Key);
			RawTags.Remove(Key);
		}

		bool GetValue(const FString& Key, FString& OutValue)
		{
			if (FString* Value = Tags.Find(Key))
			{
				OutValue = *Value;
				return true;
			}

			return false;
		}

		void GetOrSet(const FString& Key, FString& Value)
		{
			if (FString* InValue = Tags.Find(Key))
			{
				Value = *InValue;
				return;
			}

			Tags.Add(Key, Value);
		}

		void GetOrSet(const FString& Key, const int64 Value, FString& OutValue)
		{
			OutValue = FString::Printf(TEXT("%llu"), Value);
			GetOrSet(Key, OutValue);
		}

		bool IsTagged(const FString& Key) const { return Tags.Contains(Key); }
		bool IsTagged(const FString& Key, const FString& Value) const { return RawTags.Contains(Key + TagSeparator + Value); }
		bool IsTagged(const FString& Key, const int64 Value) const { return IsTagged(Key, FString::Printf(TEXT("%llu"), Value)); }

	protected:
		// NAME::VALUE
		static bool GetTagFromString(const FString& Input, FString& OutKey, FString& OutValue)
		{
			if (!Input.Contains(TagSeparator)) { return false; }

			TArray<FString> Split = UKismetStringLibrary::ParseIntoArray(Input, TagSeparator, true);

			if (Split.IsEmpty() || Split.Num() < 2) { return false; }

			OutKey = Split[0];
			Split.RemoveAt(0);
			OutValue = FString::Join(Split, TEXT(""));
			return true;
		}
	};
}
