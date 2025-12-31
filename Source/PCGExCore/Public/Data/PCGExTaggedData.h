// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

struct FPCGTaggedData;
class IPCGAttributeAccessorKeys;

namespace PCGExData
{
	class FTags;
	class FPointIO;
}

class UPCGData;

struct PCGEXCORE_API FPCGExTaggedData
{
	const UPCGData* Data = nullptr;
	int32 Index = -1;
	TWeakPtr<PCGExData::FTags> Tags;
	TSharedPtr<IPCGAttributeAccessorKeys> Keys = nullptr;

	FPCGExTaggedData() = default;
	FPCGExTaggedData(const UPCGData* InData, const int32 InIdx, const TSharedPtr<PCGExData::FTags>& InTags, const TSharedPtr<IPCGAttributeAccessorKeys>& InKeys);
	explicit FPCGExTaggedData(const TSharedPtr<PCGExData::FPointIO>& InData, const int32 InIdx = -1);
	
	TSharedPtr<PCGExData::FTags> GetTags() const;
	void Dump(FPCGTaggedData& InOut) const;
};
