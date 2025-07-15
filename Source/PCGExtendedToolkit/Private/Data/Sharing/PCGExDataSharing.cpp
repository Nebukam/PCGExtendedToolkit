// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Sharing/PCGExDataSharing.h"

#include "PCGExSubSystem.h"

namespace PCGExDataSharing
{
	uint32 GetPartitionIdx(const FVector& InPosition, const double PartitionSize)
	{
		return 0;
	}

	uint32 GetPartitionIdx(uint32 InBaseID, const FVector& InPosition, const double PartitionSize)
	{
		return 0;
	}


	bool FDataBucket::Set(uint32 Key, const FPCGDataCollection& InValue)
	{
		PCGEX_SUBSYSTEM
		// Data used here needs to be flattened
		// TODO
		// Root data
		return false;
	}

	bool FDataBucket::Add(uint32 Key, const FPCGDataCollection& InValue)
	{
		// Data used here needs to be flattened
		// TODO
		// Root data
		return false;
	}

	bool FDataBucket::Remove(uint32 Key)
	{
		// TODO
		// Unroot data
		return false;
	}

	FPCGDataCollection* FDataBucket::Find(uint32 Key)
	{
		// TODO
		return Data.Find(Key);
	}

	FPCGDataCollection* FDataBucket::Find(uint32 Key, FBox WithinBounds)
	{
		// TODO
		return Data.Find(Key);
	}

	int32 FDataBucket::Append(uint32 Key, FPCGDataCollection& OutCollection)
	{
		// TODO
		return 0;
	}

	void FDataBucket::Empty()
	{
		Data.Empty();
		Octree.Reset();
	}

	TSharedPtr<PCGEx::FIndexedItemOctree> FDataBucket::GetOctree()
	{
		// TODO
		return Octree;
	}

	void FDataBucket::RebuildOctree()
	{
		// TODO
	}
}
