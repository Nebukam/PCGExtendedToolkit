// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

struct FPCGTaggedData;
class IPCGAttributeAccessorKeys;

namespace PCGExData
{
	class FTags;
}

class UPCGData;

struct FPCGExTaggedData
{
	const UPCGData* Data = nullptr;
	TWeakPtr<PCGExData::FTags> Tags;
	TSharedPtr<IPCGAttributeAccessorKeys> Keys = nullptr;

	FPCGExTaggedData() = default;
	FPCGExTaggedData(const UPCGData* InData, const TSharedPtr<PCGExData::FTags>& InTags, const TSharedPtr<IPCGAttributeAccessorKeys>& InKeys);
	TSharedPtr<PCGExData::FTags> GetTags() const;
	void Dump(FPCGTaggedData& InOut) const;
};
