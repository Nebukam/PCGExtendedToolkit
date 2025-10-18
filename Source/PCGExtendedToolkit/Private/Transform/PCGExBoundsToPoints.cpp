﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExBoundsToPoints.h"

#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsToPointsElement"
#define PCGEX_NAMESPACE BoundsToPoints

PCGEX_INITIALIZE_ELEMENT(BoundsToPoints)
PCGEX_ELEMENT_BATCH_POINT_IMPL(BoundsToPoints)

bool FPCGExBoundsToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsToPoints)

	return true;
}

bool FPCGExBoundsToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsToPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Missing data."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBoundsToPoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBoundsToPoints::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, Settings->bGeneratePerPointData ? PCGExData::EIOInit::NoInit : PCGExData::EIOInit::Duplicate)

		bSetExtents = Settings->bSetExtents;
		Extents = Settings->Extents;

		bSetScale = Settings->bSetScale;
		Scale = Settings->Scale;

		Axis = Settings->SymmetryAxis;
		UVW = Settings->UVW;
		if (!UVW.Init(ExecutionContext, PointDataFacade)) { return false; }

		PointAttributesToOutputTags = Settings->PointAttributesToOutputTags;
		if (!PointAttributesToOutputTags.Init(ExecutionContext, PointDataFacade)) { return false; }

		NumPoints = PointDataFacade->GetNum();
		bGeneratePerPointData = Settings->bGeneratePerPointData;
		bSymmetry = Settings->SymmetryAxis != EPCGExMinimalAxis::None;

		if (bGeneratePerPointData)
		{
			NewOutputs.SetNum(PointDataFacade->GetNum());
			for (int i = 0; i < NewOutputs.Num(); i++)
			{
				NewOutputs[i] = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::New);
			}

			if (bSymmetry)
			{
			}
			else
			{
			}
		}
		else
		{
			if (bSymmetry)
			{
				PCGEx::SetNumPointsAllocated(PointDataFacade->GetOut(), NumPoints * 2);
				PointDataFacade->Source->InheritProperties(0, NumPoints, NumPoints);
			}
			else
			{
				PCGEx::SetNumPointsAllocated(PointDataFacade->GetOut(), NumPoints);
			}
		}

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::BoundsToPoints::ProcessPoints);

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;
		PointDataFacade->Fetch(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExData::FConstPoint Point = PointIO->GetInPoint(Index);
			FVector FinalExtents = Settings->bMultiplyExtents ? Point.GetExtents() * Extents : Extents;

			if (bGeneratePerPointData)
			{
				int32 A;
				int32 B = -1;

				// TODO : Revisit this, it's a terrible way to work with point arrays	
				const TSharedPtr<PCGExData::FPointIO>& NewOutput = NewOutputs[Index];

				NewOutput->CopyToNewPoint(Index, A);
				if (bSymmetry) { NewOutput->CopyToNewPoint(Index, B); }

				TPCGValueRange<FTransform> Transforms = NewOutput->GetOut()->GetTransformValueRange(false);
				TPCGValueRange<FVector> BoundsMin = NewOutput->GetOut()->GetBoundsMinValueRange(false);
				TPCGValueRange<FVector> BoundsMax = NewOutput->GetOut()->GetBoundsMaxValueRange(false);

				if (bSetExtents)
				{
					BoundsMin[A] = -FinalExtents;
					BoundsMax[A] = FinalExtents;
				}

				Transforms[A].SetLocation(UVW.GetPosition(Index));

				if (bSetScale) { Transforms[A].SetScale3D(Scale); }

				if (bSymmetry)
				{
					if (bSetExtents)
					{
						BoundsMin[B] = -FinalExtents;
						BoundsMax[B] = FinalExtents;
					}

					Transforms[B].SetLocation(UVW.GetPosition(Index, Axis, true));

					if (bSetScale) { Transforms[B].SetScale3D(Scale); }
				}

				PointAttributesToOutputTags.Tag(Point, NewOutput);
			}
			else
			{
				TPCGValueRange<FTransform> Transforms = PointIO->GetOut()->GetTransformValueRange(false);
				TPCGValueRange<FVector> BoundsMin = PointIO->GetOut()->GetBoundsMinValueRange(false);
				TPCGValueRange<FVector> BoundsMax = PointIO->GetOut()->GetBoundsMaxValueRange(false);

				int32 A = Index;
				int32 B = NumPoints + A;

				if (bSetExtents)
				{
					BoundsMin[A] = -FinalExtents;
					BoundsMax[A] = FinalExtents;
				}

				Transforms[A].SetLocation(UVW.GetPosition(Index));

				if (bSetScale) { Transforms[A].SetScale3D(Scale); }

				if (bSymmetry)
				{
					if (bSetExtents)
					{
						BoundsMin[B] = -FinalExtents;
						BoundsMax[B] = FinalExtents;
					}

					Transforms[B].SetLocation(UVW.GetPosition(Index, Axis, true));

					if (bSetScale) { Transforms[B].SetScale3D(Scale); }
				}
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (!bGeneratePerPointData && bSymmetry)
		{
			TPCGValueRange<int64> MetadataEntries = PointDataFacade->GetOut()->GetMetadataEntryValueRange();
			UPCGMetadata* Metadata = PointDataFacade->GetOut()->Metadata;
			for (int64& Key : MetadataEntries) { Metadata->InitializeOnSet(Key); }
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
