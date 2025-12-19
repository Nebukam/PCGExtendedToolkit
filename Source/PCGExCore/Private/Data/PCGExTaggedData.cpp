// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExTaggedData.h"

#include "PCGData.h"
#include "Data/PCGExDataTag.h"

FPCGExTaggedData::FPCGExTaggedData(const UPCGData* InData, const TSharedPtr<PCGExData::FTags>& InTags, const TSharedPtr<IPCGAttributeAccessorKeys>& InKeys)
	: Data(InData), Tags(InTags), Keys(InKeys)
{
}

TSharedPtr<PCGExData::FTags> FPCGExTaggedData::GetTags() const { return Tags.Pin(); }

void FPCGExTaggedData::Dump(FPCGTaggedData& InOut) const
{
	InOut.Data = Data;
	if (const TSharedPtr<PCGExData::FTags> PinnedTags = Tags.Pin(); PinnedTags.IsValid()) { PinnedTags->DumpTo(InOut.Tags); }
};
