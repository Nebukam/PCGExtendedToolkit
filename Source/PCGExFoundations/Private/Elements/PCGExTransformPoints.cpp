// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExTransformPoints.h"


#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Fitting/PCGExFittingVariations.h"

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

bool FPCGExTransformPointsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExTransformPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(TransformPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		{
			NewBatch->bSkipCompletion = true;
		}))
		{
			return Context->CancelExecution(TEXT("No data."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExTransformPoints
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExTransformPoints::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;
		
		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Cherry pick native properties allocations

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;
		AllocateFor |= EPCGPointNativeProperties::Transform;

		bApplyScaleToBounds = Settings->bApplyScaleToBounds;
		bResetPointCenter = Settings->bApplyScaleToBounds;
		if (bApplyScaleToBounds || bResetPointCenter)
		{
			bAllocatedBounds = true;
			AllocateFor |= EPCGPointNativeProperties::BoundsMin;
			AllocateFor |= EPCGPointNativeProperties::BoundsMax;
		}

		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		OffsetMin = Settings->OffsetMin.GetValueSetting();
		if (!OffsetMin->Init(PointDataFacade)) { return false; }

		OffsetMax = Settings->OffsetMax.GetValueSetting();
		if (!OffsetMax->Init(PointDataFacade)) { return false; }

		OffsetScale = Settings->OffsetScaling.GetValueSetting();
		if (!OffsetScale->Init(PointDataFacade)) { return false; }

		OffsetSnap = Settings->OffsetSnap.GetValueSetting();
		if (!OffsetSnap->Init(PointDataFacade)) { return false; }

		AbsoluteOffset = Settings->AbsoluteOffset.GetValueSetting();
		if (!AbsoluteOffset->Init(PointDataFacade)) { return false; }


		RotMin = Settings->RotationMin.GetValueSetting();
		if (!RotMin->Init(PointDataFacade)) { return false; }

		RotMax = Settings->RotationMax.GetValueSetting();
		if (!RotMax->Init(PointDataFacade)) { return false; }

		RotScale = Settings->RotationScaling.GetValueSetting();
		if (!RotScale->Init(PointDataFacade)) { return false; }

		RotSnap = Settings->RotationSnap.GetValueSetting();
		if (!RotSnap->Init(PointDataFacade)) { return false; }


		ScaleMin = Settings->ScaleMin.GetValueSetting();
		if (!ScaleMin->Init(PointDataFacade)) { return false; }

		ScaleMax = Settings->ScaleMax.GetValueSetting();
		if (!ScaleMax->Init(PointDataFacade)) { return false; }

		ScaleScale = Settings->ScaleScaling.GetValueSetting();
		if (!ScaleScale->Init(PointDataFacade)) { return false; }

		ScaleSnap = Settings->ScaleSnap.GetValueSetting();
		if (!ScaleSnap->Init(PointDataFacade)) { return false; }

		UniformScale = Settings->UniformScale.GetValueSetting();
		if (!UniformScale->Init(PointDataFacade)) { return false; }


		if (bResetPointCenter)
		{
			PointCenter = Settings->PointCenterLocation.GetValueSetting();
			if (!PointCenter->Init(PointDataFacade)) { return false; }
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::TransformPoints::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		TConstPCGValueRange<int32> Seeds = PointDataFacade->GetIn()->GetConstSeedValueRange();
		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);

		TPCGValueRange<FVector> OutBoundsMin = bAllocatedBounds ? PointDataFacade->GetOut()->GetBoundsMinValueRange(false) : TPCGValueRange<FVector>();
		TPCGValueRange<FVector> OutBoundsMax = bAllocatedBounds ? PointDataFacade->GetOut()->GetBoundsMaxValueRange(false) : TPCGValueRange<FVector>();

		FRandomStream RandomSource;

		const bool bResetScale = Settings->bResetScale;
		const bool bResetRotation = Settings->bResetRotation;

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }

			RandomSource.Initialize(Seeds[Index]);

			FTransform& OutTransform = OutTransforms[Index];
			if (bResetScale) { OutTransform.SetScale3D(FVector::OneVector); }
			if (bResetRotation) { OutTransform.SetRotation(FQuat::Identity); }

			const FVector OffsetScaleV = OffsetScale->Read(Index);
			const FVector OffsetMinV = OffsetMin->Read(Index) * OffsetScaleV;
			const FVector OffsetMaxV = OffsetMax->Read(Index) * OffsetScaleV;
			const FVector OffsetSnapV = OffsetSnap->Read(Index);

			const FVector RotScaleV = RotScale->Read(Index);
			const FRotator RotMinV = FRotator::MakeFromEuler(RotMin->Read(Index).Euler() * RotScaleV);
			const FRotator RotMaxV = FRotator::MakeFromEuler(RotMax->Read(Index).Euler() * RotScaleV);
			const FRotator RotSnapV = RotSnap->Read(Index);

			const FVector ScaleScaleV = ScaleScale->Read(Index);
			const FVector ScaleMinV = ScaleMin->Read(Index) * ScaleScaleV;
			const FVector ScaleMaxV = ScaleMax->Read(Index) * ScaleScaleV;
			const FVector ScaleSnapV = ScaleSnap->Read(Index);

			const bool bAbsoluteOffset = AbsoluteOffset->Read(Index);
			const bool bUniformScale = UniformScale->Read(Index);

			FPCGExFittingVariations Variations(OffsetMinV, OffsetMaxV, Settings->SnapPosition, OffsetSnapV, bAbsoluteOffset, RotMinV, RotMaxV, Settings->SnapRotation, RotSnapV, Settings->AbsoluteRotation, ScaleMinV, ScaleMaxV, Settings->SnapScale, ScaleSnapV, bUniformScale);

			Variations.ApplyOffset(RandomSource, OutTransform);
			Variations.ApplyRotation(RandomSource, OutTransform);
			Variations.ApplyScale(RandomSource, OutTransform);

			if (bApplyScaleToBounds)
			{
				PCGPointHelpers::ApplyScaleToBounds(OutTransform, OutBoundsMin[Index], OutBoundsMax[Index]);
			}

			if (bResetPointCenter)
			{
				PCGPointHelpers::ResetPointCenter(PointCenter->Read(Index), OutTransform, OutBoundsMin[Index], OutBoundsMax[Index]);
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
