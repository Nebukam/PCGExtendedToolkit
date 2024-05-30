// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataFilter.h"

PCGExDataFilter::TFilter* UPCGExFilterFactoryBase::CreateFilter() const
{
	return nullptr;
}

namespace PCGExDataFilter
{
	void TFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		bValid = true;
	}

	FORCEINLINE bool TFilter::Test(const int32 PointIndex) const { return true; }

	void TFilter::PrepareForTesting(PCGExData::FPointIO* PointIO)
	{
		const int32 NumPoints = PointIO->GetNum();
		Results.SetNumUninitialized(NumPoints);
		for (int i = 0; i < NumPoints; i++) { Results[i] = false; }
	}

	void TFilter::PrepareForTesting(PCGExData::FPointIO* PointIO, const TArrayView<int32>& PointIndices)
	{
		if (const int32 NumPoints = PointIO->GetNum(); Results.Num() != NumPoints) { Results.SetNumUninitialized(NumPoints); }
		for (const int32 i : PointIndices) { Results[i] = false; }
	}

	TFilterManager::TFilterManager(PCGExData::FPointIO* InPointIO)
		: PointIO(InPointIO)
	{
	}

	void TFilterManager::PrepareForTesting()
	{
		for (TFilter* Handler : Handlers) { Handler->PrepareForTesting(PointIO); }
	}

	void TFilterManager::PrepareForTesting(const TArrayView<int32>& PointIndices)
	{
		for (TFilter* Handler : Handlers) { Handler->PrepareForTesting(PointIO, PointIndices); }
	}

	void TFilterManager::Test(const int32 PointIndex)
	{
		for (TFilter* Handler : Handlers)
		{
			const bool bValue = Handler->Test(PointIndex);
			Handler->Results[PointIndex] = bValue;
		}
	}

	void TFilterManager::PostProcessHandler(TFilter* Handler)
	{
	}

	TDirectFilterManager::TDirectFilterManager(PCGExData::FPointIO* InPointIO)
		: TFilterManager(InPointIO)
	{
	}

	void TDirectFilterManager::Test(const int32 PointIndex)
	{
		bool bPass = true;
		for (const TFilter* Handler : Handlers)
		{
			if (!Handler->Test(PointIndex))
			{
				bPass = false;
				break;
			}
		}

		Results[PointIndex] = bPass;
	}

	void TDirectFilterManager::PrepareForTesting()
	{
		for (TFilter* Handler : Handlers) { Handler->PrepareForTesting(PointIO); }

		const int32 NumPoints = PointIO->GetNum();
		Results.SetNumUninitialized(NumPoints);
		for (int i = 0; i < NumPoints; i++) { Results[i] = true; }
	}
}
