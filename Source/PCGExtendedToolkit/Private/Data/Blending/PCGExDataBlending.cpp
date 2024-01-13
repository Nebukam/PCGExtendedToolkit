// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlending.h"

namespace PCGExDataBlending
{
	FDataBlendingOperationBase::~FDataBlendingOperationBase()
	{
	}

	void FDataBlendingOperationBase::PrepareForData(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, bool bSecondaryIn)
	{
	}

	bool FDataBlendingOperationBase::GetRequiresFinalization() const { return false; }

	void FDataBlendingOperationBase::PrepareOperation(const int32 WriteIndex) const
	{
		PrepareRangeOperation(WriteIndex, 1);
	}

	void FDataBlendingOperationBase::DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Alpha) const
	{
		TArray<double> Alphas = {Alpha};
		DoRangeOperation(PrimaryReadIndex, SecondaryReadIndex, WriteIndex, 1, Alphas);
	}

	void FDataBlendingOperationBase::FinalizeOperation(const int32 WriteIndex, const double Alpha) const
	{
		TArray<double> Alphas = {Alpha};
		FinalizeRangeOperation(WriteIndex, 1, Alphas);
	}

	void FDataBlendingOperationBase::FullBlendToOne(const TArrayView<double>& Alphas) const
	{
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
