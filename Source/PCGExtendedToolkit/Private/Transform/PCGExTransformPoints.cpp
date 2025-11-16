// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExTransformPoints.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsSettings.h"

#define LOCTEXT_NAMESPACE "PCGExTransformPointsElement"
#define PCGEX_NAMESPACE TransformPoints

PCGEX_INITIALIZE_ELEMENT(TransformPoints)

PCGExData::EIOInit UPCGExTransformPointsSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(TransformPoints)

bool FPCGExTransformPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(TransformPoints)

	return true;
}

bool FPCGExTransformPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTransformPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(TransformPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bSkipCompletion = true;
			}))
		{
			return Context->CancelExecution(TEXT("No data."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExTransformPoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExTransformPoints::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Cherry pick native properties allocations

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;
		AllocateFor |= EPCGPointNativeProperties::Transform;
		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		OffsetMin = Settings->OffsetMin.GetValueSetting();
		if (!OffsetMin->Init(PointDataFacade)) { return false; }

		OffsetMax = Settings->OffsetMax.GetValueSetting();
		if (!OffsetMax->Init(PointDataFacade)) { return false; }

		OffsetSnap = Settings->OffsetSnap.GetValueSetting();
		if (!OffsetSnap->Init(PointDataFacade)) { return false; }

		AbsoluteOffset = Settings->AbsoluteOffset.GetValueSetting();
		if (!AbsoluteOffset->Init(PointDataFacade)) { return false; }

		RotMin = Settings->RotationMin.GetValueSetting();
		if (!RotMin->Init(PointDataFacade)) { return false; }

		RotMax = Settings->RotationMax.GetValueSetting();
		if (!RotMax->Init(PointDataFacade)) { return false; }

		RotSnap = Settings->RotationSnap.GetValueSetting();
		if (!RotSnap->Init(PointDataFacade)) { return false; }

		ScaleMin = Settings->ScaleMin.GetValueSetting();
		if (!ScaleMin->Init(PointDataFacade)) { return false; }

		ScaleMax = Settings->ScaleMax.GetValueSetting();
		if (!ScaleMax->Init(PointDataFacade)) { return false; }

		ScaleSnap = Settings->ScaleSnap.GetValueSetting();
		if (!ScaleSnap->Init(PointDataFacade)) { return false; }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::TransformPoints::ProcessPoints);

		TConstPCGValueRange<int32> Seeds = PointDataFacade->GetIn()->GetConstSeedValueRange();
		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);

		FRandomStream RandomSource;
		
		PCGEX_SCOPE_LOOP(Index)
		{
			RandomSource.Initialize(Seeds[Index]);
			
			FTransform& OutTransform = OutTransforms[Index];
			
			const FVector OffsetMinV = OffsetMin->Read(Index);
			const FVector OffsetMaxV = OffsetMax->Read(Index);
			const FVector OffsetSnapV = OffsetSnap->Read(Index);

			const FRotator RotMinV = RotMin->Read(Index);
			const FRotator RotMaxV = RotMax->Read(Index);
			const FRotator RotSnapV = RotSnap->Read(Index);

			const FVector ScaleMinV = ScaleMin->Read(Index);
			const FVector ScaleMaxV = ScaleMax->Read(Index);
			const FVector ScaleSnapV = ScaleSnap->Read(Index);

			const bool bAbsoluteOffset = AbsoluteOffset->Read(Index);
			
			FPCGExFittingVariations Variations(
				OffsetMinV, OffsetMaxV,
				Settings->SnapPosition, OffsetSnapV,
				bAbsoluteOffset,
				RotMinV, RotMaxV,
				Settings->SnapRotation, RotSnapV,
				Settings->AbsoluteRotation,
				ScaleMinV, ScaleMaxV,
				Settings->SnapScale, ScaleSnapV
			);

			Variations.ApplyOffset(RandomSource, OutTransform);
			Variations.ApplyRotation(RandomSource, OutTransform);
			Variations.ApplyScale(RandomSource, OutTransform);
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
