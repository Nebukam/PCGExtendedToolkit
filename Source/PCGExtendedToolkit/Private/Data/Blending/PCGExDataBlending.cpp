// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlending.h"

#include "PCGExGlobalSettings.h"
#include "PCGExSettings.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/PCGExGraph.h"

namespace PCGExDataBlending
{
	FDataBlendingOperationBase::~FDataBlendingOperationBase()
	{
	}

	void FDataBlendingOperationBase::PrepareForData(PCGExData::FFacade* InPrimaryData, PCGExData::FFacade* InSecondaryData, const PCGExData::ESource SecondarySource)
	{
	}

	void FDataBlendingOperationBase::PrepareForData(PCGEx::FAAttributeIO* InWriter, PCGExData::FFacade* InSecondaryData, const PCGExData::ESource SecondarySource)
	{
	}

	bool FDataBlendingOperationBase::GetIsInterpolation() const { return false; }

	bool FDataBlendingOperationBase::GetRequiresFinalization() const { return false; }

	void FDataBlendingOperationBase::PrepareOperation(const int32 WriteIndex) const
	{
		PrepareRangeOperation(WriteIndex, 1);
	}

	void FDataBlendingOperationBase::DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Weight, const bool bFirstOperation) const
	{
		TArray<double> Weights = {Weight};
		DoRangeOperation(PrimaryReadIndex, SecondaryReadIndex, WriteIndex, Weights, bFirstOperation);
	}

	void FDataBlendingOperationBase::FinalizeOperation(const int32 WriteIndex, const int32 Count, const double TotalWeight) const
	{
		TArray<double> Weights = {TotalWeight};
		TArray<int32> Counts = {Count};
		FinalizeRangeOperation(WriteIndex, Counts, Weights);
	}

	void FDataBlendingOperationBase::ResetToDefault(const int32 WriteIndex) const
	{
		ResetRangeToDefault(WriteIndex, 1);
	}

	void FDataBlendingOperationBase::ResetRangeToDefault(int32 StartIndex, int32 Count) const
	{
	}

	bool FDataBlendingOperationBase::GetRequiresPreparation() const { return false; }
}
