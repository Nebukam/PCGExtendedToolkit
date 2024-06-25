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

	void TFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		FilteredIO = PointIO;
		bValid = true;
	}

	void TFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO)
	{
		if (bCacheResults)
		{
			const int32 NumPoints = PointIO->GetNum();
			Results.SetNumUninitialized(NumPoints);
			for (bool& Result : Results) { Result = false; }
		}
	}

	void TFilter::PrepareForTesting(const PCGExData::FPointIO* PointIO, const TArrayView<const int32>& PointIndices)
	{
		if (bCacheResults)
		{
			if (const int32 NumPoints = PointIO->GetNum(); Results.Num() != NumPoints) { Results.SetNumUninitialized(NumPoints); } // TODO : This is wrong
			for (const int32 i : PointIndices) { Results[i] = false; }
		}
	}

	TFilterManager::TFilterManager(const PCGExData::FPointIO* InPointIO)
		: PointIO(InPointIO)
	{
	}

	void TFilterManager::PrepareForTesting()
	{
		for (TFilter* Handler : Handlers) { Handler->PrepareForTesting(PointIO); }
	}

	void TFilterManager::PrepareForTesting(const TArrayView<const int32>& PointIndices)
	{
		for (TFilter* Handler : Handlers) { Handler->PrepareForTesting(PointIO, PointIndices); }
	}

	void TFilterManager::Test(const int32 PointIndex)
	{
		for (TFilter* Handler : Handlers) { Handler->Results[PointIndex] = Handler->Test(PointIndex); }
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

	void TEarlyExitFilterManager::PrepareForTesting()
	{
		for (TFilter* Handler : Handlers) { Handler->PrepareForTesting(PointIO); }
		Results.SetNumUninitialized(PointIO->GetNum());
		for (bool& Result : Results) { Result = true; }
	}

	void TEarlyExitFilterManager::PrepareForTesting(const TArrayView<const int32>& PointIndices)
	{
		check(false) //this override Should not be used with early exit
	}
}
