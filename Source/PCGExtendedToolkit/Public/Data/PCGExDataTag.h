// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExMacros.h"
#include "Kismet/KismetStringLibrary.h"

namespace PCGExData
{
	const FString TagSeparator = FSTRING("::");

	struct FTags
	{
		mutable FRWLock TagsLock;
		TSet<FString> RawTags;       // Contains all data tag
		TMap<FString, FString> Tags; // PCGEx Tags Name::Value

		bool IsEmpty() const { return RawTags.IsEmpty() && Tags.IsEmpty(); }

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

		explicit FTags(const TSharedPtr<FTags>& InTags)
			: FTags()
		{
			Reset(InTags);
		}

		void Append(const TSharedRef<FTags>& InTags)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			Tags.Append(InTags->Tags);
			RawTags.Append(InTags->RawTags);
		}

		void Append(const TArray<FString>& InTags)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			RawTags.Append(InTags);
		}

		void Reset()
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			RawTags.Empty();
			Tags.Empty();
		}

		void Reset(const TSharedPtr<FTags>& InTags)
		{
			Reset();
			if (InTags) { Append(InTags.ToSharedRef()); }
		}

		void Dump(TSet<FString>& InTags) const
		{
			InTags.Append(RawTags);
			for (const TPair<FString, FString>& Tag : Tags) { InTags.Add((Tag.Key + TagSeparator + Tag.Value)); }
		}

		TSet<FString> ToSet()
		{
			FReadScopeLock ReadScopeLock(TagsLock);
			TSet<FString> Flattened;
			Flattened.Append(RawTags);
			for (const TPair<FString, FString>& Tag : Tags) { Flattened.Add((Tag.Key + TagSeparator + Tag.Value)); }
			return Flattened;
		}

		~FTags() = default;

		void Add(const FString& Key)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			RawTags.Add(Key);
		}

		void Add(const FString& Key, const FString& Value)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			Tags.Add(Key, Value);
		}

		void Add(const FString& Key, const int64 Value, FString& OutValue)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			OutValue = FString::Printf(TEXT("%llu"), Value);
			Tags.Add(Key, OutValue);
		}

		void Remove(const FString& Key)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			Tags.Remove(Key);
			RawTags.Remove(Key);
		}

		void Remove(const TSet<FString>& InSet)
		{
			FWriteScopeLock WriteScopeLock(TagsLock);
			for (const FString& Tag : InSet)
			{
				Tags.Remove(Tag);
				RawTags.Remove(Tag);
			}
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
			FWriteScopeLock WriteScopeLock(TagsLock);
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

		bool IsTagged(const FString& Key) const { return Tags.Contains(Key) || RawTags.Contains(Key); }
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
