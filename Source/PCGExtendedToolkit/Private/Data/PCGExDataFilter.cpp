// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataFilter.h"

PCGExFactories::EType UPCGExFilterFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::Filter;
}

PCGExDataFilter::TFilter* UPCGExFilterFactoryBase::CreateFilter() const
{
	return nullptr;
}

namespace PCGExDataFilter
{
	EType TFilter::GetFilterType() const { return EType::Default; }

	void TFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		bValid = true;
	}

	bool TFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		if (bCacheResults)
		{
			const int32 NumPoints = PointIO->GetNum();
			Results.SetNumUninitialized(NumPoints);
			for (bool& Result : Results) { Result = false; }
		}

		return false;
	}

	bool TFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO, const TArrayView<int32>& PointIndices)
	{
		if (bCacheResults)
		{
			if (const int32 NumPoints = PointIO->GetNum(); Results.Num() != NumPoints) { Results.SetNumUninitialized(NumPoints); }
			for (const int32 i : PointIndices) { Results[i] = false; }
		}

		return false;
	}

	void TFilter::PrepareSingle(const int32 PointIndex)
	{
	}

	void TFilter::PreparationComplete()
	{
	}

	TFilterManager::TFilterManager(const PCGExData::FPointIO* InPointIO)
		: PointIO(InPointIO)
	{
	}

	bool TFilterManager::PrepareForTesting()
	{
		HeavyHandlers.Reset();
		for (TFilter* Handler : Handlers) { if (Handler->PrepareForTesting(PointIO)) { HeavyHandlers.Add(Handler); } }
		return !HeavyHandlers.IsEmpty();
	}

	bool TFilterManager::PrepareForTesting(const TArrayView<int32>& PointIndices)
	{
		HeavyHandlers.Reset();
		for (TFilter* Handler : Handlers) { if (Handler->PrepareForTesting(PointIO, PointIndices)) { HeavyHandlers.Add(Handler); } }
		return !HeavyHandlers.IsEmpty();
	}

	void TFilterManager::PrepareSingle(const int32 PointIndex)
	{
		for (TFilter* Handler : HeavyHandlers) { Handler->PrepareSingle(PointIndex); }
	}

	void TFilterManager::PreparationComplete()
	{
		for (TFilter* Handler : HeavyHandlers) { Handler->PreparationComplete(); }
	}

	void TFilterManager::Test(const int32 PointIndex)
	{
		for (TFilter* Handler : Handlers) { Handler->Results[PointIndex] = Handler->Test(PointIndex); }
	}

	bool TFilterManager::RequiresPerPointPreparation() const
	{
		return !HeavyHandlers.IsEmpty();
	}

	void TFilterManager::PostProcessHandler(TFilter* Handler)
	{
	}

	TEarlyExitFilterManager::TEarlyExitFilterManager(const PCGExData::FPointIO* InPointIO)
		: TFilterManager(InPointIO)
	{
		
	}

	void TEarlyExitFilterManager::Test(const int32 PointIndex)
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

	bool TEarlyExitFilterManager::PrepareForTesting()
	{
		HeavyHandlers.Reset();
		for (TFilter* Handler : Handlers) { if (Handler->PrepareForTesting(PointIO)) { HeavyHandlers.Add(Handler); } }

		Results.SetNumUninitialized(PointIO->GetNum());
		for (bool& Result : Results) { Result = true; }

		return !HeavyHandlers.IsEmpty();
	}

	bool TEarlyExitFilterManager::PrepareForTesting(const TArrayView<int32>& PointIndices)
	{
		check(false) //this override Should not be used with early exit
		return TFilterManager::PrepareForTesting(PointIndices);
	}
}
