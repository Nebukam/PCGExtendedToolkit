// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExBoundsAxisToPoints.h"


#define LOCTEXT_NAMESPACE "PCGExBoundsAxisToPointsElement"
#define PCGEX_NAMESPACE BoundsAxisToPoints

PCGExData::EIOInit UPCGExBoundsAxisToPointsSettings::GetMainOutputInitMode() const { return bGeneratePerPointData ? PCGExData::EIOInit::None : PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(BoundsAxisToPoints)

bool FPCGExBoundsAxisToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsAxisToPoints)

	return true;
}

bool FPCGExBoundsAxisToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsAxisToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsAxisToPoints)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBoundsAxisToPoints::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBoundsAxisToPoints::FProcessor>>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Missing data."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBoundsAxisToPoints
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBoundsAxisToPoints::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bSetExtents = Settings->bSetExtents;
		Extents = Settings->Extents;

		bSetScale = Settings->bSetScale;
		Scale = Settings->Scale;

		PointAttributesToOutputTags = Settings->PointAttributesToOutputTags;
		if (!PointAttributesToOutputTags.Init(ExecutionContext, PointDataFacade)) { return false; }

		NumPoints = PointDataFacade->GetNum();
		bGeneratePerPointData = Settings->bGeneratePerPointData;

		if (bGeneratePerPointData)
		{
			NewOutputs.SetNum(PointDataFacade->GetNum());
			for (int i = 0; i < NewOutputs.Num(); i++)
			{
				NewOutputs[i] = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::New);
			}
		}
		else
		{
			PointDataFacade->GetOut()->GetMutablePoints().SetNumUninitialized(NumPoints * 2);
		}

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		const FVector E = PCGExMath::GetLocalBounds(Point, Settings->BoundsReference).GetExtent();
		FPCGExUVW UVW;
		UVW.BoundsReference = Settings->BoundsReference;

		EPCGExMinimalAxis Axis = EPCGExMinimalAxis::None;

		FQuat R = Point.Transform.GetRotation();
		int32 Idx = -1;
		int32 Indices[3] = {0, 1, 2};
		int32 DotsIndices[3] = {0, 1, 2};

		const FVector Direction[3] = {R.GetAxisX(), R.GetAxisY(), R.GetAxisZ()};
		const double Size[3] = {E.X, E.Y, E.Z};
		const EPCGExMinimalAxis AxisEnum[3] = {EPCGExMinimalAxis::X, EPCGExMinimalAxis::Y, EPCGExMinimalAxis::Z};
		double Dots[3] = {0, 0, 0};

		if (Settings->DirectionConstraint != EPCGExAxisDirectionConstraint::None)
		{
			for (int i = 0; i < 3; i++) { Dots[i] = FVector::DotProduct(Direction[i], Settings->Direction); }
			Algo::Sort(DotsIndices, [&](const int32 A, const int32 B) { return Dots[A] < Dots[B]; });
		}

		Algo::Sort(Indices, [&](const int32 A, const int32 B) { return Size[A] < Size[B]; });

		switch (Settings->Priority)
		{
		case EPCGExBoundAxisPriority::Shortest:
			Idx = 0;
			break;
		case EPCGExBoundAxisPriority::Median:
			Idx = 1;
			break;
		case EPCGExBoundAxisPriority::Longest:
			Idx = 2;
			break;
		}

		auto ApplySizeConstraint = [&]()
		{
			if (Settings->SizeConstraint == EPCGExAxisSizeConstraint::Greater)
			{
				for (int i = FMath::Clamp(Idx, 0, 2); i < 3; i++) { if (Size[Indices[i]] < Settings->SizeThreshold) { Idx++; } }
			}
			else
			{
				for (int i = FMath::Clamp(Idx, 0, 2); i >= 0; i--) { if (Size[Indices[i]] > Settings->SizeThreshold) { Idx--; } }
			}
		};

		auto ApplyDirectionConstraint = [&]()
		{
			if (Settings->DirectionConstraint == EPCGExAxisDirectionConstraint::Avoid)
			{
				// If selected direction is the worst
				// Back off
				if (Indices[FMath::Clamp(Idx, 0, 2)] == DotsIndices[2]) { Idx = 1; }
			}
			else
			{
				// If selected direction is not the best
				// Nudge toward it
				if (Indices[FMath::Clamp(Idx, 0, 2)] != DotsIndices[2]) { Idx++; }
			}
		};

		if (Settings->ConstraintsOrder == EPCGExAxisConstraintSorting::SizeMatters)
		{
			if (Settings->DirectionConstraint != EPCGExAxisDirectionConstraint::None) { ApplyDirectionConstraint(); }
			if (Settings->SizeConstraint != EPCGExAxisSizeConstraint::None) { ApplySizeConstraint(); }
		}
		else
		{
			if (Settings->SizeConstraint != EPCGExAxisSizeConstraint::None) { ApplySizeConstraint(); }
			if (Settings->DirectionConstraint != EPCGExAxisDirectionConstraint::None) { ApplyDirectionConstraint(); }
		}

		Axis = AxisEnum[Indices[FMath::Clamp(Idx, 0, 2)]];

		switch (Axis)
		{
		case EPCGExMinimalAxis::None:
		case EPCGExMinimalAxis::X:
			UVW.UConstant = Settings->U;
			break;
		case EPCGExMinimalAxis::Y:
			UVW.VConstant = Settings->U;
			break;
		case EPCGExMinimalAxis::Z:
			UVW.WConstant = Settings->U;
			break;
		}

		if (bGeneratePerPointData)
		{
			int32 OutIndex;
			const TSharedPtr<PCGExData::FPointIO>& NewOutput = NewOutputs[Index];

			FPCGPoint& A = NewOutput->CopyPoint(Point, OutIndex);
			if (bSetExtents)
			{
				A.BoundsMin = -Extents;
				A.BoundsMax = Extents;
			}

			A.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index)));
			if (bSetScale) { A.Transform.SetScale3D(Scale); }

			FPCGPoint& B = NewOutput->CopyPoint(Point, OutIndex);
			if (bSetExtents)
			{
				B.BoundsMin = -Extents;
				B.BoundsMax = Extents;
			}

			B.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index), Axis, true));
			if (bSetScale) { B.Transform.SetScale3D(Scale); }


			PointAttributesToOutputTags.Tag(Index, NewOutput);
		}
		else
		{
			TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

			FPCGPoint& A = MutablePoints[Index];
			if (bSetExtents)
			{
				A.BoundsMin = -Extents;
				A.BoundsMax = Extents;
			}

			A.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index)));
			if (bSetScale) { A.Transform.SetScale3D(Scale); }

			MutablePoints[NumPoints + Index] = Point;
			FPCGPoint& B = MutablePoints[NumPoints + Index];
			if (bSetExtents)
			{
				B.BoundsMin = -Extents;
				B.BoundsMax = Extents;
			}

			B.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index), Axis, true));
			if (bSetScale) { B.Transform.SetScale3D(Scale); }
		}
	}

	void FProcessor::CompleteWork()
	{
		if (!bGeneratePerPointData)
		{
			TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();
			UPCGMetadata* Metadata = PointDataFacade->GetOut()->Metadata;
			for (int i = NumPoints; i < MutablePoints.Num(); i++) { Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry); }
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
