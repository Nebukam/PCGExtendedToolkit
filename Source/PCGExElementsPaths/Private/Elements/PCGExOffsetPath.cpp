// Copyright 2026 Timothé Lapetite and contributors
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

}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
