// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExTaggedData.h"

#include "PCGData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"

FPCGExTaggedData::FPCGExTaggedData(const UPCGData* InData, const int32 InIdx, const TSharedPtr<PCGExData::FTags>& InTags, const TSharedPtr<IPCGAttributeAccessorKeys>& InKeys)
	: Data(InData), Index(InIdx), Tags(InTags), Keys(InKeys)
{
}

FPCGExTaggedData::FPCGExTaggedData(const TSharedPtr<PCGExData::FPointIO>& InData, const int32 InIdx)
	: Data(InData->GetIn()), Index(InIdx == INDEX_NONE ? InData->IOIndex : InIdx), Tags(InData->Tags), Keys(InData->GetInKeys())
{
}

TSharedPtr<PCGExData::FTags> FPCGExTaggedData::GetTags() const { return Tags.Pin(); }

void FPCGExTaggedData::Dump(FPCGTaggedData& InOut) const
{
	InOut.Data = Data;
	if (const TSharedPtr<PCGExData::FTags> PinnedTags = Tags.Pin(); PinnedTags.IsValid()) { PinnedTags->DumpTo(InOut.Tags); }
};
