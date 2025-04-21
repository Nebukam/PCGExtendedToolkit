// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataTag.h"

namespace PCGExTags
{
	bool FTagValue::SameValue(const TSharedPtr<FTagValue>& Other) const
	{
		if (IsNumeric() && Other->IsNumeric()) { return AsDouble() == Other->AsDouble(); }
		if (IsText() && Other->IsText()) { return AsString() == Other->AsString(); }
		return false;
	}

	TSharedPtr<FTagValue> TryGetValueTag(const FString& InTag, FString& OutLeftSide)
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
				return MakeShared<TTagValue<double>>(FCString::Atod(*RightSide));
			}
			return MakeShared<TTagValue<int32>>(FCString::Atoi(*RightSide));
		}

		if (FVector ParsedVector; ParsedVector.InitFromString(RightSide))
		{
			return MakeShared<TTagValue<FVector>>(ParsedVector);
		}

		if (FVector2D ParsedVector2D; ParsedVector2D.InitFromString(RightSide))
		{
			return MakeShared<TTagValue<FVector2D>>(ParsedVector2D);
		}

		if (FVector4 ParsedVector4; ParsedVector4.InitFromString(RightSide))
		{
			return MakeShared<TTagValue<FVector4>>(ParsedVector4);
		}

		return MakeShared<TTagValue<FString>>(RightSide);
	}
}

namespace PCGExData
{
	int32 FTags::Num() const
	{
		return RawTags.Num() + ValueTags.Num();
	}

	bool FTags::IsEmpty() const
	{
		return RawTags.IsEmpty() && ValueTags.IsEmpty();
	}

	FTags::FTags()
	{
		RawTags.Empty();
		ValueTags.Empty();
	}

	FTags::FTags(const TSet<FString>& InTags)
		: FTags()
	{
		for (const FString& TagString : InTags) { ParseAndAdd(TagString); }
	}

	FTags::FTags(const TSharedPtr<FTags>& InTags)
		: FTags()
	{
		Reset(InTags);
	}

	void FTags::Append(const TSharedRef<FTags>& InTags)
	{
		Append(InTags->FlattenToArray());
	}

	void FTags::Append(const TArray<FString>& InTags)
	{
		FWriteScopeLock WriteScopeLock(TagsLock);
		for (const FString& TagString : InTags) { ParseAndAdd(TagString); }
	}

	void FTags::Append(const TSet<FString>& InTags)
	{
		FWriteScopeLock WriteScopeLock(TagsLock);
		for (const FString& TagString : InTags) { ParseAndAdd(TagString); }
	}

	void FTags::Reset()
	{
		FWriteScopeLock WriteScopeLock(TagsLock);
		RawTags.Empty();
		ValueTags.Empty();
	}

	void FTags::Reset(const TSharedPtr<FTags>& InTags)
	{
		Reset();
		if (InTags) { Append(InTags.ToSharedRef()); }
	}

	void FTags::DumpTo(TSet<FString>& InTags, const bool bFlatten) const
	{
		FReadScopeLock ReadScopeLock(TagsLock);

		InTags.Reserve(InTags.Num() + Num());
		InTags.Append(RawTags);
		if (bFlatten) { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { InTags.Add(Pair.Value->Flatten(Pair.Key)); } }
		else { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { InTags.Add(Pair.Key); } }
	}

	void FTags::DumpTo(TArray<FName>& InTags, const bool bFlatten) const
	{
		FReadScopeLock ReadScopeLock(TagsLock);
		InTags.Reserve(Num());
		InTags.Append(FlattenToArrayOfNames(bFlatten));
	}

	TSet<FString> FTags::Flatten()
	{
		FReadScopeLock ReadScopeLock(TagsLock);

		TSet<FString> Flattened;
		Flattened.Reserve(Num());
		Flattened.Append(RawTags);
		for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(Pair.Value->Flatten(Pair.Key)); }
		return Flattened;
	}

	TArray<FString> FTags::FlattenToArray(const bool bIncludeValue) const
	{
		FReadScopeLock ReadScopeLock(TagsLock);

		TArray<FString> Flattened;
		Flattened.Reserve(Num());
		for (const FString& Key : RawTags) { Flattened.Add(Key); }
		if (bIncludeValue) { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(Pair.Value->Flatten(Pair.Key)); } }
		else { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(Pair.Key); } }
		return Flattened;
	}

	TArray<FName> FTags::FlattenToArrayOfNames(const bool bIncludeValue) const
	{
		FReadScopeLock ReadScopeLock(TagsLock);

		TArray<FName> Flattened;
		Flattened.Reserve(Num());
		for (const FString& Key : RawTags) { Flattened.Add(FName(Key)); }
		if (bIncludeValue) { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(FName(Pair.Value->Flatten(Pair.Key))); } }
		else { for (const TPair<FString, TSharedPtr<FTagValue>>& Pair : ValueTags) { Flattened.Add(FName(Pair.Key)); } }
		return Flattened;
	}

	void FTags::AddRaw(const FString& Key)
	{
		FWriteScopeLock WriteScopeLock(TagsLock);
		ParseAndAdd(Key);
	}

	void FTags::Remove(const FString& Key)
	{
		FWriteScopeLock WriteScopeLock(TagsLock);
		ValueTags.Remove(Key);
		RawTags.Remove(Key);
	}

	void FTags::Remove(const TSet<FString>& InSet)
	{
		FWriteScopeLock WriteScopeLock(TagsLock);
		for (const FString& Tag : InSet)
		{
			ValueTags.Remove(Tag);
			RawTags.Remove(Tag);
		}
	}

	void FTags::Remove(const TSet<FName>& InSet)
	{
		FWriteScopeLock WriteScopeLock(TagsLock);
		for (const FName& Tag : InSet)
		{
			FString Key = Tag.ToString();
			ValueTags.Remove(Key);
			RawTags.Remove(Key);
		}
	}

	TSharedPtr<FTagValue> FTags::GetValue(const FString& Key) const
	{
		FReadScopeLock ReadScopeLock(TagsLock);
		if (const TSharedPtr<FTagValue>* ValueTagPtr = ValueTags.Find(Key)) { return *ValueTagPtr; }
		return nullptr;
	}

	bool FTags::IsTagged(const FString& Key) const
	{
		FReadScopeLock ReadScopeLock(TagsLock);
		return ValueTags.Contains(Key) || RawTags.Contains(Key);
	}

	bool FTags::IsTagged(const FString& Key, const bool bInvert) const
	{
		FReadScopeLock ReadScopeLock(TagsLock);
		return (ValueTags.Contains(Key) || RawTags.Contains(Key)) ? !bInvert : bInvert;
	}

	void FTags::ParseAndAdd(const FString& InTag)
	{
		FString InKey = TEXT("");

		if (const TSharedPtr<FTagValue> TagValue = TryGetValueTag(InTag, InKey))
		{
			ValueTags.Add(InKey, TagValue);
			return;
		}

		RawTags.Add(InTag);
	}

	bool FTags::GetTagFromString(const FString& Input, FString& OutKey, FString& OutValue)
	{
		if (!Input.Contains(TagSeparator)) { return false; }

		TArray<FString> Split = UKismetStringLibrary::ParseIntoArray(Input, TagSeparator, true);

		if (Split.IsEmpty() || Split.Num() < 2) { return false; }

		OutKey = Split[0];
		Split.RemoveAt(0);
		OutValue = FString::Join(Split, TEXT(""));
		return true;
	}
}
