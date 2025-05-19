// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSolidify.h"

#include "PCGExDataMath.h"


#define LOCTEXT_NAMESPACE "PCGExPathSolidifyElement"
#define PCGEX_NAMESPACE PathSolidify

PCGEX_INITIALIZE_ELEMENT(PathSolidify)

bool FPCGExPathSolidifyElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSolidify)

	return true;
}

bool FPCGExPathSolidifyElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSolidifyElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSolidify)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathSolidify::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathSolidify::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathSolidify
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathSolidify::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;
		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointIO);

		Path = PCGExPaths::MakePath(PointDataFacade->GetIn(), 0, bClosedLoop);
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();
		Path->IOIndex = PointDataFacade->Source->IOIndex;

#define PCGEX_CREATE_LOCAL_AXIS_SET_CONST(_AXIS) if (Settings->bWriteRadius##_AXIS){\
		SolidificationRad##_AXIS = PCGExDetails::MakeSettingValue(Settings->Radius##_AXIS##Input, Settings->Radius##_AXIS##SourceAttribute, Settings->Radius##_AXIS##Constant);\
		if (!SolidificationRad##_AXIS->Init(Context, PointDataFacade, false)){ return false; }}
		PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_SET_CONST)
#undef PCGEX_CREATE_LOCAL_AXIS_SET_CONST

		SolidificationLerp = Settings->GetValueSettingSolidificationLerp();
		if (!SolidificationLerp->Init(Context, PointDataFacade, false)) { return false; }

		if (!bClosedLoop && Settings->bRemoveLastPoint) { PointIO->GetOut()->SetNumPoints(Path->LastIndex); }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);

		TPCGValueRange<FTransform> Transforms = PointDataFacade->GetOut()->GetTransformValueRange();
		TPCGValueRange<FVector> BoundsMin = PointDataFacade->GetOut()->GetBoundsMinValueRange();
		TPCGValueRange<FVector> BoundsMax = PointDataFacade->GetOut()->GetBoundsMaxValueRange();

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!Path->IsValidEdgeIndex(Index)) { continue; }

			const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];
			Path->ComputeEdgeExtra(Index);

			const double Length = PathLength->Get(Index);

			FRotator EdgeRot;
			FVector TargetBoundsMin = BoundsMin[Index];
			FVector TargetBoundsMax = BoundsMax[Index];

			const double EdgeLerp = FMath::Clamp(SolidificationLerp->Read(Index), 0, 1);
			const double EdgeLerpInv = 1 - EdgeLerp;

			const FVector PtScale = Transforms[Index].GetScale3D();
			const FVector InvScale = FVector::One() / PtScale;

			//SolidificationRad##_AXIS.IsValid();

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
if (Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS){\
TargetBoundsMin._AXIS = (-Length * EdgeLerp)* InvScale._AXIS;\
TargetBoundsMax._AXIS = (Length * EdgeLerpInv) * InvScale._AXIS;\
}else if(Settings->bWriteRadius##_AXIS){\
const double Rad = FMath::Lerp(SolidificationRad##_AXIS->Read(Index), SolidificationRad##_AXIS->Read(Index), EdgeLerpInv); \
TargetBoundsMin._AXIS = (-Rad) * InvScale._AXIS;\
TargetBoundsMax._AXIS = (Rad) * InvScale._AXIS;\
}

			PCGEX_FOREACH_XYZ(PCGEX_SOLIDIFY_DIMENSION)
#undef PCGEX_SOLIDIFY_DIMENSION

			switch (Settings->SolidificationAxis)
			{
			default:
			case EPCGExMinimalAxis::X:
				EdgeRot = FRotationMatrix::MakeFromX(Edge.Dir).Rotator();
				break;
			case EPCGExMinimalAxis::Y:
				EdgeRot = FRotationMatrix::MakeFromY(Edge.Dir).Rotator();
				break;
			case EPCGExMinimalAxis::Z:
				EdgeRot = FRotationMatrix::MakeFromZ(Edge.Dir).Rotator();
				break;
			}

			Transforms[Index] = FTransform(EdgeRot, Path->GetEdgePositionAtAlpha(Index, EdgeLerp), Transforms[Index].GetScale3D());

			BoundsMin[Index] = TargetBoundsMin;
			BoundsMax[Index] = TargetBoundsMax;
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
