// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlending.h"

#include "Metadata/Accessors/PCGAttributeAccessor.h"

namespace PCGExDataBlending
{
	FDataBlendingOperationBase::~FDataBlendingOperationBase()
	{
		PrimaryBaseAttribute = nullptr;
		SecondaryBaseAttribute = nullptr;
	}

	void FDataBlendingOperationBase::PrepareForData(
		UPCGPointData* InPrimaryData,
		const UPCGPointData* InSecondaryData,
		FPCGAttributeAccessorKeysPoints* InPrimaryKeys,
		FPCGAttributeAccessorKeysPoints* InSecondaryKeys)
	{
		PrimaryBaseAttribute = InPrimaryData->Metadata->GetMutableAttribute(AttributeName);
		SecondaryBaseAttribute = InSecondaryData->Metadata->GetMutableAttribute(AttributeName);

		if (!PrimaryBaseAttribute && SecondaryBaseAttribute)
		{
			PrimaryBaseAttribute = InPrimaryData->Metadata->CopyAttribute(SecondaryBaseAttribute, AttributeName, false, false, false);
		}

		bInterpolationAllowed = PrimaryBaseAttribute->AllowsInterpolation() && SecondaryBaseAttribute->AllowsInterpolation();
	}

	bool FDataBlendingOperationBase::GetRequiresFinalization() const { return false; }

	void FDataBlendingOperationBase::PrepareOperation(const int32 WriteIndex) const
	{
		PrepareRangeOperation(1, WriteIndex);
	}

	void FDataBlendingOperationBase::DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Alpha) const
	{
		TArray<double> Alphas = {Alpha};
		DoRangeOperation(PrimaryReadIndex, SecondaryReadIndex, WriteIndex, 1, Alphas);
	}

	void FDataBlendingOperationBase::FinalizeOperation(const int32 WriteIndex, double Alpha) const
	{
		TArray<double> Alphas = {Alpha};
		FinalizeRangeOperation(WriteIndex, 1, Alphas);
	}

	void FDataBlendingOperationBase::ResetToDefault(int32 WriteIndex) const
	{
		ResetRangeToDefault(WriteIndex, 1);
	}

	void FDataBlendingOperationBase::ResetRangeToDefault(int32 StartIndex, int32 Count) const
	{
	}

	bool FDataBlendingOperationBase::GetRequiresPreparation() const { return false; }
}
