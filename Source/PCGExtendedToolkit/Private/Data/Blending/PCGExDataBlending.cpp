// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlending.h"

#include "Metadata/PCGMetadataAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

namespace PCGExDataBlending
{
	FDataBlendingOperationBase::~FDataBlendingOperationBase()
	{
		PrimaryBaseAttribute = nullptr;
		SecondaryBaseAttribute = nullptr;
	}

	void FDataBlendingOperationBase::PrepareForData(
		const UPCGPointData* InPrimaryData,
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

		bInterpolationAllowed = PrimaryBaseAttribute->AllowsInterpolation() && !SecondaryBaseAttribute->AllowsInterpolation();
	}

	bool FDataBlendingOperationBase::GetRequiresFinalization() const { return false; }

	void FDataBlendingOperationBase::PrepareOperation(const int32 WriteIndex) const
	{
	}

	void FDataBlendingOperationBase::DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const PCGMetadataEntryKey WriteIndex, const double Alpha) const
	{
	}

	void FDataBlendingOperationBase::FinalizeOperation(const int32 WriteIndex, double Alpha) const
	{
	}

	void FDataBlendingOperationBase::ResetToDefault(int32 WriteIndex) const
	{
	}

	bool FDataBlendingOperationBase::GetRequiresPreparation() const { return false; }
}
