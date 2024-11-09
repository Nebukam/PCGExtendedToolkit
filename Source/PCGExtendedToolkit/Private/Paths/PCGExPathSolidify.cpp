// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSolidify.h"

#include "PCGExDataMath.h"


#define LOCTEXT_NAMESPACE "PCGExPathSolidifyElement"
#define PCGEX_NAMESPACE PathSolidify

PCGExData::EIOInit UPCGExPathSolidifySettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::DuplicateInput; }

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
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
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

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathSolidify::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;
		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointIO);

		Path = PCGExPaths::MakePath(PointDataFacade->GetIn()->GetPoints(), 0, bClosedLoop);
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();
		Path->IOIndex = PointDataFacade->Source->IOIndex;


#define PCGEX_CREATE_LOCAL_AXIS_SET_CONST(_AXIS) if (Settings->bWriteRadius##_AXIS){Rad##_AXIS##Constant = Settings->Radius##_AXIS##Constant;}
		PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_SET_CONST)
#undef PCGEX_CREATE_LOCAL_AXIS_SET_CONST

		// Create edge-scope getters
#define PCGEX_CREATE_LOCAL_AXIS_GETTER(_AXIS)\
if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Input == EPCGExInputValueType::Attribute){\
SolidificationRad##_AXIS = PointDataFacade->GetBroadcaster<double>(Settings->Radius##_AXIS##SourceAttribute);\
if (!SolidificationRad##_AXIS){ PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Some paths don't have the specified Radius Attribute \"{0}\"."), FText::FromName(Settings->Radius##_AXIS##SourceAttribute.GetName()))); return false; }}
		PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_GETTER)
#undef PCGEX_CREATE_LOCAL_AXIS_GETTER

		if (Settings->SolidificationLerpInput == EPCGExInputValueType::Attribute)
		{
			SolidificationLerpGetter = PointDataFacade->GetBroadcaster<double>(Settings->SolidificationLerpAttribute);
			if (!SolidificationLerpGetter)
			{
				PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FText::Format(FTEXT("Some paths don't have the specified SolidificationEdgeLerp Attribute \"{0}\"."), FText::FromName(Settings->SolidificationLerpAttribute.GetName())));
				return false;
			}
		}

		if (!bClosedLoop && Settings->bRemoveLastPoint) { PointIO->GetOut()->GetMutablePoints().RemoveAt(Path->LastIndex); }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (!Path->IsValidEdgeIndex(Index)) { return; }

		const PCGExPaths::FPathEdge& Edge = Path->Edges[Index];
		Path->ComputeEdgeExtra(Index);

		const double Length = PathLength->Get(Index);

		FRotator EdgeRot;
		FVector TargetBoundsMin = Point.BoundsMin;
		FVector TargetBoundsMax = Point.BoundsMax;

		const double EdgeLerp = FMath::Clamp(SolidificationLerpGetter ? SolidificationLerpGetter->Read(Index) : Settings->SolidificationLerpConstant, 0, 1);
		const double EdgeLerpInv = 1 - EdgeLerp;
		bool bProcessAxis;

		const FVector PtScale = Point.Transform.GetScale3D();
		const FVector InvScale = FVector::One() / PtScale;

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
		bProcessAxis = Settings->bWriteRadius##_AXIS || Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS;\
		if (bProcessAxis){\
			if (Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS){\
				TargetBoundsMin._AXIS = (-Length * EdgeLerp)* InvScale._AXIS;\
				TargetBoundsMax._AXIS = (Length * EdgeLerpInv) * InvScale._AXIS;\
			}else{\
				double Rad = Rad##_AXIS##Constant;\
				if(SolidificationRad##_AXIS){Rad = FMath::Lerp(SolidificationRad##_AXIS->Read(Index), SolidificationRad##_AXIS->Read(Index), EdgeLerpInv); }\
				else{Rad=Settings->Radius##_AXIS##Constant;}\
				TargetBoundsMin._AXIS = (-Rad) * InvScale._AXIS;\
				TargetBoundsMax._AXIS = (Rad) * InvScale._AXIS;\
			}\
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

		Point.Transform = FTransform(EdgeRot, Path->GetEdgePositionAtAlpha(Index, EdgeLerp), Point.Transform.GetScale3D());

		Point.BoundsMin = TargetBoundsMin;
		Point.BoundsMax = TargetBoundsMax;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
