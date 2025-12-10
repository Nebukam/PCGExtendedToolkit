// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCommon.h"

#include "CoreMinimal.h"
#include "Data/PCGExDataTag.h"

namespace PCGExCommon
{
	uint64 SHash(const FString& S)
	{
		const ANSICHAR* Ansi = TCHAR_TO_ANSI(*S);
		const uint64 H = CityHash64(Ansi, S.Len());
		
		//UE_LOG(LogTemp, Warning, TEXT("PCGEx CTX STATE >> '%s' = %llu"), *S, H)
			
		return H;
	}
}

namespace PCGExData
{
	FTaggedData::FTaggedData(const UPCGData* InData, const TSharedPtr<FTags>& InTags, const TSharedPtr<IPCGAttributeAccessorKeys>& InKeys)
		: Data(InData), Tags(InTags), Keys(InKeys)
	{
	}

	TSharedPtr<FTags> FTaggedData::GetTags() const { return Tags.Pin(); }

	void FTaggedData::Dump(FPCGTaggedData& InOut) const
	{
		InOut.Data = Data;
		if (const TSharedPtr<FTags> PinnedTags = Tags.Pin(); PinnedTags.IsValid()) { PinnedTags->DumpTo(InOut.Tags); }
	};
}
