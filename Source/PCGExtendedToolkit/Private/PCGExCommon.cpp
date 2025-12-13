// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCommon.h"

#include "CoreMinimal.h"
#include "Data/PCGExDataTag.h"

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
