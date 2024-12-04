// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExOffsetPath.h"

#include "PCGExDataMath.h"


#define LOCTEXT_NAMESPACE "PCGExOffsetPathElement"
#define PCGEX_NAMESPACE OffsetPath

PCGExData::EIOInit UPCGExOffsetPathSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(OffsetPath)

bool FPCGExOffsetPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(OffsetPath)

	if (Settings->bCleanupPath && Settings->bFlagMutatedPoints) { PCGEX_VALIDATE_NAME(Settings->MutatedAttributeName) }

	return true;
}

bool FPCGExOffsetPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOffsetPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(OffsetPath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be affected."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExOffsetPath::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					if (!Settings->bOmitInvalidPathsOutputs) { Entry->InitializeOutput(PCGExData::EIOInit::Forward); }
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExOffsetPath::FProcessor>>& NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
			}))
		{
			Context->CancelExecution(TEXT("Could not find any paths to offset."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExOffsetPath
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExOffsetPath::Process);

		if (Settings->OffsetMethod == EPCGExOffsetMethod::Slide)
		{
			PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;
		}
		else
		{
			PointDataFacade->bSupportsScopedGet = false;
		}

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PointDataFacade->Source->InitializeOutput(Settings->bCleanupPath ? PCGExData::EIOInit::New : PCGExData::EIOInit::Duplicate);

		if (Settings->bInvertDirection) { DirectionFactor *= -1; }

		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();

		Up = Settings->UpVectorConstant.GetSafeNormal();
		OffsetConstant = Settings->OffsetConstant;

		Positions.SetNumUninitialized(InPoints.Num());
		for (int i = 0; i < InPoints.Num(); i++) { Positions[i] = InPoints[i].Transform.GetLocation(); }

		ToleranceSquared = Settings->IntersectionTolerance * Settings->IntersectionTolerance;
		Path = PCGExPaths::MakePath(Positions, 0, Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source));

		if (Settings->OffsetMethod == EPCGExOffsetMethod::Slide)
		{
			if (Settings->Adjustment != EPCGExOffsetAdjustment::None)
			{
				PathAngles = Path->AddExtra<PCGExPaths::FPathEdgeHalfAngle>(false, Up);
			}
		}

		if (Settings->OffsetInput == EPCGExInputValueType::Attribute)
		{
			OffsetGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->OffsetAttribute);
			if (!OffsetGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FText::Format(FTEXT("Input missing offset size attribute: \"{0}\"."), FText::FromName(Settings->OffsetAttribute.GetName())));
				return false;
			}
		}

		if (Settings->DirectionType == EPCGExInputValueType::Attribute)
		{
			DirectionGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->DirectionAttribute);
			if (!DirectionGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FText::Format(FTEXT("Input missing UpVector attribute: \"{0}\"."), FText::FromName(Settings->DirectionAttribute.GetName())));
				return false;
			}
		}
		else
		{
			if (Settings->OffsetMethod == EPCGExOffsetMethod::LinePlane)
			{
				OffsetDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeNormal>(true, Up));
			}
			else
			{
				switch (Settings->DirectionConstant)
				{
				case EPCGExPathNormalDirection::Normal:
					OffsetDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeNormal>(false, Up));
					break;
				case EPCGExPathNormalDirection::Binormal:
					OffsetDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeBinormal>(false, Up));
					break;
				case EPCGExPathNormalDirection::AverageNormal:
					OffsetDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeAvgNormal>(false, Up));
					break;
				}
			}
		}

		StartParallelLoopForPoints(Settings->bCleanupPath ? PCGExData::ESource::In : PCGExData::ESource::Out);
		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		const int32 EdgeIndex = (!Path->IsClosedLoop() && Index == Path->LastIndex) ? Path->LastEdge : Index;
		Path->ComputeEdgeExtra(EdgeIndex);

		FVector Dir = (OffsetDirection ? OffsetDirection->Get(EdgeIndex) : DirectionGetter->Read(Index)) * DirectionFactor;
		double Offset = (OffsetGetter ? OffsetGetter->Read(Index) : OffsetConstant);

		if (Settings->bApplyPointScaleToOffset) { Dir *= Point.Transform.GetScale3D(); }

		if (Settings->OffsetMethod == EPCGExOffsetMethod::Slide)
		{
			if (PathAngles)
			{
				if (Settings->Adjustment == EPCGExOffsetAdjustment::SmoothCustom)
				{
					Offset *= (1 + Settings->AdjustmentScale * FMath::Cos(PathAngles->Get(EdgeIndex)));
				}
				else if (Settings->Adjustment == EPCGExOffsetAdjustment::SmoothAuto)
				{
					const double Dot = FMath::Clamp(FVector::DotProduct(Path->DirToPrevPoint(Index) * -1, Path->DirToNextPoint(Index)), -1, 0);
					Offset *= 1 + (FMath::Abs(Dot) * FMath::Acos(Dot)) * FMath::Abs(Dot);
				}
				else if (Settings->Adjustment == EPCGExOffsetAdjustment::Mitre)
				{
					double MitreLength = Offset / FMath::Sin(PathAngles->Get(EdgeIndex) / 2);
					if (MitreLength > Settings->MitreLimit * Offset) { Offset *= Settings->MitreLimit; } // Should bevel :(
				}
			}

			Positions[Index] = Path->GetPosUnsafe(Index) + (Dir * Offset);
		}
		else
		{
			const int32 PrevIndex = Path->SafePointIndex(Index - 1);
			const FVector PlaneDir = ((OffsetDirection ? OffsetDirection->Get(PrevIndex) : DirectionGetter->Read(PrevIndex)) * DirectionFactor).GetSafeNormal();
			const FVector PlaneOrigin = Path->GetPosUnsafe(PrevIndex) + (PlaneDir * (OffsetGetter ? OffsetGetter->Read(PrevIndex) : OffsetConstant));

			const FVector A = Path->GetPosUnsafe(Index) + (Dir * Offset);
			const double Dot = FMath::Clamp(FMath::Abs(FVector::DotProduct(Path->DirToPrevPoint(Index), Path->DirToNextPoint(Index))), 0, 1);


			if (FMath::IsNearlyZero(1 - Dot))
			{
				Positions[Index] = A;
			}
			else
			{
				const FVector Candidate = FMath::LinePlaneIntersection(A, A + Path->DirToNextPoint(Index) * 10, PlaneOrigin, PlaneDir * -1);
				if (Candidate.ContainsNaN()) { Positions[Index] = A; }
				else { Positions[Index] = Candidate; }
			}
		}


		if (!Settings->bCleanupPath) { Point.Transform.SetLocation(Positions[Index]); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (!Settings->bCleanupPath) { return; }

		DirtyPath = PCGExPaths::MakePath(Positions, ToleranceSquared, Path->IsClosedLoop());
		CleanEdge.Init(false, DirtyPath->NumEdges);

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, FlipTestTask)

		FlipTestTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PCGEX_ASYNC_THIS
				PCGEX_ASYNC_SUB_LOOP
				{
					This->DirtyPath->ComputeEdgeExtra(i);
					This->CleanEdge[i] = FVector::DotProduct(This->Path->Edges[i].Dir, This->DirtyPath->Edges[i].Dir) > 0;
				}
			};

		FlipTestTask->StartSubLoops(DirtyPath->NumEdges, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::CompleteWork()
	{
		if (!Settings->bCleanupPath) { return; }

		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();
		TArray<FPCGPoint>& OutPoints = PointDataFacade->GetMutablePoints();
		OutPoints.Reserve(InPoints.Num());

		TArray<int8> Mutated;
		Mutated.Reserve(InPoints.Num());

		int32 Last = 0;

		if (DirtyPath->IsClosedLoop() && !CleanEdge[0])
		{
			//Starting with a flipped edge, should only ever happen with closed loops.
			// Go back in the path until we find a valid end
			for (int i = 0; i < CleanEdge.Num(); i++)
			{
				if (CleanEdge[i])
				{
					Last = i;
					break;
				}
			}
		}

		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		FVector MutatedPosition = FVector::ZeroVector;

		DirtyPath->BuildPartialEdgeOctree(CleanEdge);

		if (Settings->CleanupMode == EPCGExOffsetCleanupMode::Balanced)
		{
			bool bWaitingForCleanEdge = false;

			for (int i = Last; i < CleanEdge.Num(); i++)
			{
				if (bWaitingForCleanEdge)
				{
					if (!CleanEdge[i]) { continue; }

					bWaitingForCleanEdge = false;

					// Try to find if there is any upcoming intersection
					// if not, resolve with current

					if (!FindNextIntersection<false>(DirtyPath->Edges[i], i, MutatedPosition))
					{
						// Fallback to next clean edge, as there is no upcoming intersections.
						const PCGExPaths::FPathEdge& E1 = DirtyPath->Edges[Last];

						const FVector E11 = Positions[E1.Start];
						const FVector E12 = Positions[E1.End];

						const PCGExPaths::FPathEdge& E2 = DirtyPath->Edges[i];
						FMath::SegmentDistToSegment(E11, E12, Positions[E2.Start], Positions[E2.End], A, MutatedPosition);
					}

					FPCGPoint& Pt = OutPoints.Add_GetRef(InPoints[i]);
					Positions[i] = MutatedPosition; // FMath::Lerp(A, B, 0.5);
					Pt.Transform.SetLocation(Positions[i]);
					Mutated.Add(1);

					Last = i;
					continue;
				}

				if (CleanEdge[i])
				{
					Mutated.Add(0);
					FPCGPoint& Pt = OutPoints.Add_GetRef(InPoints[i]);
					Pt.Transform.SetLocation(Positions[i]);
					Last = i;


					if (Settings->bAdditionalIntersectionCheck)
					{
						// Additional intersection check on clean edge; will update the next starting position.
						if (FindNextIntersection<true>(DirtyPath->Edges[i], i, MutatedPosition))
						{
							// Update next position and keep moving
							Positions[i] = MutatedPosition;
							i--;
						}
					}
					continue;
				}

				bWaitingForCleanEdge = true;
			}
		}
		else
		{
			for (int i = Last; i < CleanEdge.Num(); i++)
			{
				if (!CleanEdge[i]) { continue; }

				FPCGPoint& Pt = OutPoints.Add_GetRef(InPoints[i]);
				Pt.Transform.SetLocation(Positions[i]);
				Mutated.Add(0);

				if (FindNextIntersection<true>(DirtyPath->Edges[i], i, MutatedPosition))
				{
					// Update next position and keep moving
					Positions[i] = MutatedPosition;
					i--;
				}
			}
		}

		if (!Path->IsClosedLoop())
		{
			FPCGPoint& Pt = OutPoints.Add_GetRef(InPoints.Last());
			Pt.Transform.SetLocation(Positions.Last());
		}

		if (OutPoints.Num() < 2) { PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::None); }
		else if (Settings->bFlagMutatedPoints)
		{
			TSharedPtr<PCGExData::TBuffer<bool>> MutatedFlag = PointDataFacade->GetWritable<bool>(Settings->MutatedAttributeName, false, true, PCGExData::EBufferInit::Inherit);
			for (int i = 0; i < Mutated.Num(); i++) { MutatedFlag->GetMutable(i) = Mutated[i] ? true : false; }
			PointDataFacade->Write(AsyncManager);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
