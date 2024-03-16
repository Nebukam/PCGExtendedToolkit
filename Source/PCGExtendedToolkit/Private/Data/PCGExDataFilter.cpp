// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataFilter.h"


void UPCGExFilterDefinitionBase::BeginDestroy()
{
	Super::BeginDestroy();
}

PCGExDataFilter::TFilterHandler* UPCGExFilterDefinitionBase::CreateHandler() const
{
	return nullptr;
}

namespace PCGExDataFilter
{
	void TFilterHandler::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		bValid = true;
	}

	bool TFilterHandler::Test(const int32 PointIndex) const { return true; }

	void TFilterHandler::PrepareForTesting(PCGExData::FPointIO* PointIO)
	{
		const int32 NumPoints = PointIO->GetNum();
		Results.SetNumUninitialized(NumPoints);
		for (int i = 0; i < NumPoints; i++) { Results[i] = false; }
	}

	TFilterManager::TFilterManager(PCGExData::FPointIO* InPointIO)
		: PointIO(InPointIO)
	{
	}

	void TFilterManager::PrepareForTesting()
	{
		for (TFilterHandler* Handler : Handlers) { Handler->PrepareForTesting(PointIO); }
	}

	void TFilterManager::Test(const int32 PointIndex)
	{
		for (TFilterHandler* Handler : Handlers)
		{
			const bool bValue = Handler->Test(PointIndex);
			Handler->Results[PointIndex] = bValue;
		}
	}

	void TFilterManager::PostProcessHandler(TFilterHandler* Handler)
	{
	}

	TDirectFilterManager::TDirectFilterManager(PCGExData::FPointIO* InPointIO)
		: TFilterManager(InPointIO)
	{
	}

	void TDirectFilterManager::Test(const int32 PointIndex)
	{
		bool bPass = true;
		for (const TFilterHandler* Handler : Handlers)
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
		for (TFilterHandler* Handler : Handlers) { Handler->PrepareForTesting(PointIO); }

		const int32 NumPoints = PointIO->GetNum();
		Results.SetNumUninitialized(NumPoints);
		for (int i = 0; i < NumPoints; i++) { Results[i] = true; }
	}
}
