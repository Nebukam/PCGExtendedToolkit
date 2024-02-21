// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlending.h"

#include "PCGExSettings.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/PCGExGraph.h"
#include "Misc/PCGExFusePoints.h"

namespace PCGExDataBlending
{
	FDataBlendingOperationBase::~FDataBlendingOperationBase()
	{
	}

	void FDataBlendingOperationBase::PrepareForData(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, bool bSecondaryIn)
	{
	}

	void FDataBlendingOperationBase::PrepareForData(PCGEx::FAAttributeIO* InWriter, const PCGExData::FPointIO& InSecondaryData, bool bSecondaryIn)
	{
	}

	bool FDataBlendingOperationBase::GetIsInterpolation() const { return false; }

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

namespace PCGExDataBlendingTask
{
	bool FBlendCompoundedIO::ExecuteTask()
	{
		PointIO->CreateInKeys();

		PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(BlendingSettings);
		MetadataBlender->PrepareForData(*TargetIO);

		const TArray<FPCGPoint>& SourcePoints = PointIO->GetIn()->GetPoints();

		for (int i = 0; i < CompoundList->Compounds.Num(); i++)
		{
			PCGExData::FIdxCompound* Idx = CompoundList->Compounds[i];
			const PCGEx::FPointRef Target = TargetIO->GetOutPointRef(i);

			Idx->ComputeWeights(SourcePoints, *Target.Point, DistSettings);

			MetadataBlender->PrepareForBlending(Target);

			for (int j = 0; j < Idx->CompoundedPoints.Num(); j++)
			{
				const uint32 SourceIndex = PCGEx::H64B(Idx->CompoundedPoints[j]);
				MetadataBlender->Blend(Target, PointIO->GetInPointRef(SourceIndex), Target, Idx->Weights[j]);
			}

			MetadataBlender->CompleteBlending(Target, Idx->CompoundedPoints.Num());
		}

		MetadataBlender->Write();
		PCGEX_DELETE(MetadataBlender);

		if (MetadataSettings)
		{
			// Write fuse meta after, so we don't blend it
			Manager->Start<FWriteFuseMetadata>(TaskIndex, TargetIO, MetadataSettings, CompoundList);
		}

		return true;
	}

	bool FWriteFuseMetadata::ExecuteTask()
	{
		const bool bWriteCompounded = MetadataSettings->bWriteCompounded;
		const bool bWriteCompoundSize = MetadataSettings->bWriteCompoundSize;

		if (!bWriteCompounded && bWriteCompoundSize) { return false; }

		PCGEx::TFAttributeWriter<bool>* CompoundedWriter = bWriteCompounded ? new PCGEx::TFAttributeWriter<bool>(MetadataSettings->CompoundedAttributeName, false, false) : nullptr;
		PCGEx::TFAttributeWriter<int32>* CompoundSizeWriter = bWriteCompoundSize ? new PCGEx::TFAttributeWriter<int32>(MetadataSettings->CompoundSizeAttributeName, 0, false) : nullptr;

		if (CompoundedWriter) { CompoundedWriter->BindAndGet(*PointIO); }
		if (CompoundSizeWriter) { CompoundSizeWriter->BindAndGet(*PointIO); }

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
