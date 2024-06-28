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

namespace PCGExDataBlendingTask
{
	bool FWriteFuseMetadata::ExecuteTask()
	{
		const bool bWriteCompounded = MetadataSettings->bWriteCompounded;
		const bool bWriteCompoundSize = MetadataSettings->bWriteCompoundSize;

		if (!bWriteCompounded && bWriteCompoundSize) { return false; }

		PCGEx::TFAttributeWriter<bool>* CompoundedWriter = bWriteCompounded ? new PCGEx::TFAttributeWriter<bool>(MetadataSettings->CompoundedAttributeName, false, false) : nullptr;
		PCGEx::TFAttributeWriter<int32>* CompoundSizeWriter = bWriteCompoundSize ? new PCGEx::TFAttributeWriter<int32>(MetadataSettings->CompoundSizeAttributeName, 0, false) : nullptr;

		if (CompoundedWriter) { CompoundedWriter->BindAndGet(PointIO); }
		if (CompoundSizeWriter) { CompoundSizeWriter->BindAndGet(PointIO); }

		for (int i = 0; i < CompoundList->Compounds.Num(); i++)
		{
			const PCGExData::FIdxCompound* Idx = CompoundList->Compounds[i];
			if (CompoundedWriter) { CompoundedWriter->Values[i] = Idx->CompoundedPoints.Num() > 1; }
			if (CompoundSizeWriter) { CompoundSizeWriter->Values[i] = Idx->CompoundedPoints.Num(); }
		}

		if (CompoundedWriter) { CompoundedWriter->Write(); }
		if (CompoundSizeWriter) { CompoundSizeWriter->Write(); }

		PCGEX_DELETE(CompoundedWriter);
		PCGEX_DELETE(CompoundSizeWriter);

		return true;
	}
}
