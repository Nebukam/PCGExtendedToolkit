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


void UPCGExFilterFactoryBase::Init()
{
}


namespace PCGExDataFilter
{
	EType TFilter::GetFilterType() const { return EType::Default; }

	void TFilter::Capture(const FPCGContext* InContext, PCGExDataCaching::FPool* InPointDataCache)
	{
		PointDataCache = InPointDataCache;
		bValid = true;
	}

	void TFilter::PrepareForTesting()
	{
		if (bCacheResults)
		{
			const int32 NumPoints = PointDataCache->Source->GetNum();
			Results.SetNumUninitialized(NumPoints);
			for (bool& Result : Results) { Result = false; }
		}
	}

	void TFilter::PrepareForTesting(const TArrayView<const int32>& PointIndices)
	{
		if (bCacheResults)
		{
			if (const int32 NumPoints = PointDataCache->Source->GetNum(); Results.Num() != NumPoints) { Results.SetNumUninitialized(NumPoints); } // TODO : This is wrong
			for (const int32 i : PointIndices) { Results[i] = false; }
		}
	}

	TFilterManager::TFilterManager(PCGExDataCaching::FPool* InPointDataCache)
		: PointDataCache(InPointDataCache)
	{
	}

	void TFilterManager::PrepareForTesting()
	{
		for (TFilter* Handler : Handlers) { Handler->PrepareForTesting(); }
	}

	void TFilterManager::PrepareForTesting(const TArrayView<const int32>& PointIndices)
	{
		for (TFilter* Handler : Handlers) { Handler->PrepareForTesting(PointIndices); }
	}

	bool TFilterManager::Test(const int32 PointIndex)
	{
		for (TFilter* Handler : Handlers) { Handler->Results[PointIndex] = Handler->Test(PointIndex); }
		return true;
	}

	void TFilterManager::PostProcessHandler(TFilter* Handler)
	{
	}

	TEarlyExitFilterManager::TEarlyExitFilterManager(PCGExDataCaching::FPool* InPointDataCache)
		: TFilterManager(InPointDataCache)
	{
	}

	bool TEarlyExitFilterManager::Test(const int32 PointIndex)
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

		if (bCacheResults) { Results[PointIndex] = bPass; }
		return bPass;
	}

	void TEarlyExitFilterManager::PrepareForTesting()
	{
		for (TFilter* Handler : Handlers)
		{
			Handler->bCacheResults = false;
			Handler->PrepareForTesting();
		}

		if (bCacheResults)
		{
			Results.SetNumUninitialized(PointDataCache->Source->GetNum());
			for (bool& Result : Results) { Result = true; }
		}
	}

	void TEarlyExitFilterManager::PrepareForTesting(const TArrayView<const int32>& PointIndices)
	{
		for (TFilter* Handler : Handlers)
		{
			Handler->bCacheResults = false;
			Handler->PrepareForTesting(PointIndices);
		}

		if (bCacheResults)
		{
			Results.SetNumUninitialized(PointDataCache->Source->GetNum());
			for (bool& Result : Results) { Result = true; }
		}
	}
}
