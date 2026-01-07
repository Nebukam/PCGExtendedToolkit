// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExOffsetPath.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExOffsetPathElement"
#define PCGEX_NAMESPACE OffsetPath

PCGEX_SETTING_VALUE_IMPL(UPCGExOffsetPathSettings, Offset, double, OffsetInput, OffsetAttribute, OffsetConstant)

PCGEX_INITIALIZE_ELEMENT(OffsetPath)

PCGExData::EIOInit UPCGExOffsetPathSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(OffsetPath)

bool FPCGExOffsetPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(OffsetPath)

	if (Settings->bFlagFlippedPoints) { PCGEX_VALIDATE_NAME(Settings->FlippedAttributeName) }
	if (Settings->bFlagMutatedPoints) { PCGEX_VALIDATE_NAME(Settings->MutatedAttributeName) }
	if (Settings->CleanupMode != EPCGExOffsetCleanupMode::None && Settings->bFlagMutatedPoints) { PCGEX_VALIDATE_NAME(Settings->MutatedAttributeName) }

	return true;
}

bool FPCGExOffsetPathElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOffsetPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(OffsetPath)

	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be affected."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				return true;
			}, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
			}))
		{
			Context->CancelExecution(TEXT("Could not find any paths to offset."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExOffsetPath
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExOffsetPath::Process);

		if (Settings->OffsetMethod == EPCGExOffsetMethod::Slide) { PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet; }
		else { PointDataFacade->bSupportsScopedGet = false; }

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		PointDataFacade->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform);

		CrossingSettings.Tolerance = Settings->IntersectionTolerance;
		CrossingSettings.bUseMinAngle = false;
		CrossingSettings.bUseMaxAngle = false;
		CrossingSettings.Init();

		if (Settings->bInvertDirection) { DirectionFactor *= -1; }

		InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		Up = Settings->UpVectorConstant.GetSafeNormal();
		OffsetConstant = Settings->OffsetConstant;

		Path = MakeShared<PCGExPaths::FPath>(InTransforms, PCGExPaths::Helpers::GetClosedLoop(PointDataFacade->GetIn()), 0);

		if (Settings->OffsetMethod == EPCGExOffsetMethod::Slide)
		{
			if (Settings->Adjustment != EPCGExOffsetAdjustment::None)
			{
				PathAngles = Path->AddExtra<PCGExPaths::FPathEdgeHalfAngle>(false, Up);
			}
		}

		OffsetGetter = Settings->GetValueSettingOffset();
		if (!OffsetGetter->Init(PointDataFacade)) { return false; }

		if (Settings->DirectionType == EPCGExInputValueType::Attribute)
		{
			DirectionGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->DirectionAttribute, true);
			if (!DirectionGetter)
			{
				PCGEX_LOG_INVALID_SELECTOR_C(ExecutionContext, Direction, Settings->DirectionAttribute)
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
				case EPCGExPathNormalDirection::Normal: OffsetDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeNormal>(false, Up));
					break;
				case EPCGExPathNormalDirection::Binormal: OffsetDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeBinormal>(false, Up));
					break;
				case EPCGExPathNormalDirection::AverageNormal: OffsetDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeAvgNormal>(false, Up));
					break;
				}
			}
		}

		StartParallelLoopForPoints();
		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::OffsetPath::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);

		PCGEX_SCOPE_LOOP(Index)
		{
			const int32 EdgeIndex = (!Path->IsClosedLoop() && Index == Path->LastIndex) ? Path->LastEdge : Index;
			Path->ComputeEdgeExtra(EdgeIndex);

			FVector Dir = (OffsetDirection ? OffsetDirection->Get(EdgeIndex) : DirectionGetter->Read(Index)) * DirectionFactor;
			double Offset = OffsetGetter->Read(Index);

			if (Settings->bApplyPointScaleToOffset) { Dir *= InTransforms[Index].GetScale3D(); }

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

				OutTransforms[Index].SetLocation(Path->GetPos_Unsafe(Index) + (Dir * Offset));
			}
			else
			{
				const int32 PrevIndex = Path->SafePointIndex(Index - 1);
				const FVector PlaneDir = ((OffsetDirection ? OffsetDirection->Get(PrevIndex) : DirectionGetter->Read(PrevIndex)) * DirectionFactor).GetSafeNormal();
				const FVector PlaneOrigin = Path->GetPos_Unsafe(PrevIndex) + (PlaneDir * OffsetGetter->Read(PrevIndex));

				const FVector A = Path->GetPos_Unsafe(Index) + (Dir * Offset);
				const double Dot = FMath::Clamp(FMath::Abs(FVector::DotProduct(Path->DirToPrevPoint(Index), Path->DirToNextPoint(Index))), 0, 1);


				if (FMath::IsNearlyZero(1 - Dot))
				{
					OutTransforms[Index].SetLocation(A);
				}
				else
				{
					const FVector Candidate = FMath::LinePlaneIntersection(A, A + Path->DirToNextPoint(Index) * 10, PlaneOrigin, PlaneDir * -1);
					if (Candidate.ContainsNaN()) { OutTransforms[Index].SetLocation(A); }
					else { OutTransforms[Index].SetLocation(Candidate); }
				}
			}

			if (!PointFilterCache[Index]) { OutTransforms[Index].SetLocation(InTransforms[Index].GetLocation()); }
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		const TConstPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetConstTransformValueRange();

		if (Settings->CleanupMode == EPCGExOffsetCleanupMode::None)
		{
			if (Settings->bFlagFlippedPoints)
			{
				DirtyPath = MakeShared<PCGExPaths::FPath>(OutTransforms, Path->IsClosedLoop(), CrossingSettings.Tolerance * 2);
				TSharedPtr<PCGExData::TBuffer<bool>> FlippedEdgeBuffer = PointDataFacade->GetWritable<bool>(Settings->FlippedAttributeName, false, true, PCGExData::EBufferInit::New);
				for (int i = 0; i < DirtyPath->NumEdges; i++) { FlippedEdgeBuffer->SetValue(i, !(FVector::DotProduct(Path->Edges[i].Dir, DirtyPath->Edges[i].Dir) > 0)); }
				PointDataFacade->WriteFastest(TaskManager);
			}
			return;
		}

		DirtyPath = MakeShared<PCGExPaths::FPath>(OutTransforms, Path->IsClosedLoop(), CrossingSettings.Tolerance * 2);
		DirtyLength = DirtyPath->AddExtra<PCGExPaths::FPathEdgeLength>();

		CleanEdge.Init(false, DirtyPath->NumEdges);
		EdgeCrossings.Init(nullptr, DirtyPath->NumEdges);

		// Mark edges that have been flipped
		for (int i = 0; i < DirtyPath->NumEdges; i++)
		{
			DirtyPath->ComputeEdgeExtra(i);
			CleanEdge[i] = FVector::DotProduct(Path->Edges[i].Dir, DirtyPath->Edges[i].Dir) > 0;
			if (FirstFlippedEdge == -1 && !CleanEdge[i]) { FirstFlippedEdge = i; }
		}

		DirtyPath->BuildEdgeOctree();

		// Find all crossings
		PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, FindCrossings)

		FindCrossings->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS

			const TSharedPtr<PCGExPaths::FPath> P = This->DirtyPath;
			const TSharedPtr<PCGExPaths::FPathEdgeLength> L = This->DirtyLength;

			PCGEX_SCOPE_LOOP(i)
			{
				TSharedPtr<PCGExPaths::FPathEdgeCrossings> NewCrossing = MakeShared<PCGExPaths::FPathEdgeCrossings>(i);
				const PCGExPaths::FPathEdge& E = P->Edges[i];
				P->GetEdgeOctree()->FindElementsWithBoundsTest(E.Bounds.GetBox(), [&](const PCGExPaths::FPathEdge* OtherEdge)
				{
					if (E.ShareIndices(OtherEdge)) { return; }
					NewCrossing->FindSplit(P, E, L, P, *OtherEdge, This->CrossingSettings);
				});

				if (!NewCrossing->IsEmpty())
				{
					NewCrossing->SortByHash();
					This->EdgeCrossings[i] = NewCrossing;
				}
			}
		};

		FindCrossings->StartSubLoops(DirtyPath->NumEdges, PCGEX_CORE_SETTINGS.GetPointsBatchChunkSize());
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->CleanupMode == EPCGExOffsetCleanupMode::None) { return; }

		switch (Settings->CleanupMode)
		{
		case EPCGExOffsetCleanupMode::CollapseFlipped: (void)PointDataFacade->Source->Gather(CleanEdge);
			break;
		case EPCGExOffsetCleanupMode::SectionsFlipped: CollapseSections(true);
			break;
		case EPCGExOffsetCleanupMode::Sections: CollapseSections(false);
			break;
		}
	}

	int32 FProcessor::CollapseFrom(const int32 StartIndex, TArray<int32>& KeptPoints, const bool bFlippedOnly)
	{
		int32 FirstItx = -1;
		for (int i = StartIndex; i < Path->NumEdges; i++)
		{
			if (!CleanEdge[i]) { continue; }

			KeptPoints.Add(i);

			TSharedPtr<PCGExPaths::FPathEdgeCrossings> Crossings = EdgeCrossings[i];
			if (!Crossings || Crossings->IsEmpty()) { continue; }

			// There's a crossing
			// Jump to the highest index segment

			const PCGExPaths::FCrossing Crossing = Crossings->Crossings.Last();
			const int32 OtherEdge = PCGEx::H64A(Crossing.Hash);
			if (FirstItx == -1) { FirstItx = i; }

			// That crossing is from earlier edges
			if (OtherEdge < i) { continue; }

			if (bFlippedOnly)
			{
				// Make sure the section contains flipped elements
				bool bContainsFlippedEdge = false;
				for (int e = i + 1; e < OtherEdge; e++)
				{
					if (!CleanEdge[e])
					{
						bContainsFlippedEdge = true;
						break;
					}
				}

				if (!bContainsFlippedEdge)
				{
					for (int e = i + 1; e < OtherEdge; e++) { KeptPoints.Add(e); }
					i = OtherEdge - 1;
					continue;
				}
			}

			// Move the other edge' point to the crossing location
			Mutated[OtherEdge] = true;
			i = OtherEdge - 1;
		}

		if (!Path->IsClosedLoop())
		{
			// Append last point (end point of last edge)
			KeptPoints.Add(Path->LastIndex);
		}

		return FirstItx;
	}

	void FProcessor::CollapseSections(const bool bFlippedOnly)
	{
		TArray<int32> KeptPoints;
		KeptPoints.Reserve(Path->NumPoints);
		Mutated.Init(false, Path->NumPoints);

		int32 StartItx = CollapseFrom(0, KeptPoints, bFlippedOnly);

		if (KeptPoints.IsEmpty())
		{
			// Collapse led to no point? 
			(void)PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::NoInit);
			return;
		}

		if (Path->IsClosedLoop() && KeptPoints[0] == 0 && KeptPoints.Last() != Path->LastIndex)
		{
			// We started inside the part of a loop that's being pruned
			// We need to start again from the first intersection this time.
			KeptPoints.Reset();
			Mutated.Init(false, Path->NumPoints);
			CollapseFrom(StartItx + 1, KeptPoints, bFlippedOnly);
		}

		if (KeptPoints.Num() == Path->NumPoints)
		{
			MarkMutated();
			return;
		}

		// TODO : Revisit this

		// Commit transforms
		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange();
		for (int i = 0; i < Path->NumEdges; i++) { if (Mutated[i] && EdgeCrossings[i]) { OutTransforms[i].SetLocation(EdgeCrossings[i]->Crossings.Last().Location); } }

		MarkMutated();

		(void)PointDataFacade->Source->Gather(KeptPoints);
	}

	void FProcessor::MarkMutated()
	{
		if (!Settings->bFlagMutatedPoints) { return; }

		TSharedPtr<PCGExData::TBuffer<bool>> MutatedBuffer = PointDataFacade->GetWritable<bool>(Settings->FlippedAttributeName, false, true, PCGExData::EBufferInit::New);
		for (int i = 0; i < DirtyPath->NumEdges; i++) { MutatedBuffer->SetValue(i, Mutated[i]); }
		PointDataFacade->WriteSynchronous();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
