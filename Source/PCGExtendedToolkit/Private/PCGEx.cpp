// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGEx.h"

#include "PCGExH.h"

namespace PCGEx
{
	bool IsPCGExAttribute(const FString& InStr) { return InStr.StartsWith(PCGExPrefix); }

	bool IsPCGExAttribute(const FName InName) { return IsPCGExAttribute(InName.ToString()); }

	bool IsPCGExAttribute(const FText& InText) { return IsPCGExAttribute(InText.ToString()); }

	bool IsValidName(const FName Name) { return FPCGMetadataAttributeBase::IsValidName(Name) && !Name.IsNone(); }

	FString StringTagFromName(const FName Name)
	{
		if (Name.IsNone()) { return TEXT(""); }
		return Name.ToString().TrimStartAndEnd();
	}

	bool IsValidStringTag(const FString& Tag)
	{
		if (Tag.TrimStartAndEnd().IsEmpty()) { return false; }
		return true;
	}

	double TruncateDbl(const double Value, const EPCGExTruncateMode Mode)
	{
		switch (Mode)
		{
		case EPCGExTruncateMode::Round: return FMath::RoundToInt(Value);
		case EPCGExTruncateMode::Ceil: return FMath::CeilToDouble(Value);
		case EPCGExTruncateMode::Floor: return FMath::FloorToDouble(Value);
		default:
		case EPCGExTruncateMode::None: return Value;
		}
	}

	void ArrayOfIndices(TArray<int32>& OutArray, const int32 InNum)
	{
		{
			const int32 _num_ = InNum;
			OutArray.Reserve(_num_);
			OutArray.SetNum(_num_);
		}
		for (int i = 0; i < InNum; i++) { OutArray[i] = i; }
	}

	FName GetCompoundName(const FName A, const FName B)
	{
		// PCGEx/A/B
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + A.ToString() + Separator + B.ToString());
	}

	FName GetCompoundName(const FName A, const FName B, const FName C)
	{
		// PCGEx/A/B/C
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + A.ToString() + Separator + B.ToString() + Separator + C.ToString());
	}

	void ScopeIndices(const TArray<int32>& InIndices, TArray<uint64>& OutScopes)
	{
		TArray<int32> InIndicesCopy = InIndices;
		InIndicesCopy.Sort();

		int32 StartIndex = InIndicesCopy[0];
		int32 LastIndex = StartIndex;
		int32 Count = 1;

		for (int i = 1; i < InIndicesCopy.Num(); i++)
		{
			const int32 NextIndex = InIndicesCopy[i];
			if (NextIndex == (LastIndex + 1))
			{
				Count++;
				LastIndex = NextIndex;
				continue;
			}

			OutScopes.Emplace(H64(StartIndex, Count));
			LastIndex = StartIndex = NextIndex;
			Count = 0;
		}

		OutScopes.Emplace(H64(StartIndex, Count));
	}
}
