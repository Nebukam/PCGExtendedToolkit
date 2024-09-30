// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSolidify.h"

#include "PCGExDataMath.h"


#define LOCTEXT_NAMESPACE "PCGExPathSolidifyElement"
#define PCGEX_NAMESPACE PathSolidify

PCGExData::EInit UPCGExPathSolidifySettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathSolidify::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return Entry->GetNum() >= 2; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathSolidify::FProcessor>>& NewBatch)
			{
			}))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any valid path."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExPathSolidify
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathSolidify::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;
		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointIO);
		LastIndex = PointIO->GetNum() - 1;


#define PCGEX_CREATE_LOCAL_AXIS_SET_CONST(_AXIS) if (Settings->bWriteRadius##_AXIS){Rad##_AXIS##Constant = Settings->Radius##_AXIS##Constant;}
		PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_SET_CONST)
#undef PCGEX_CREATE_LOCAL_AXIS_SET_CONST

		// Create edge-scope getters
#define PCGEX_CREATE_LOCAL_AXIS_GETTER(_AXIS)\
if (Settings->bWriteRadius##_AXIS && Settings->Radius##_AXIS##Type == EPCGExFetchType::Attribute){\
SolidificationRad##_AXIS = PointDataFacade->GetBroadcaster<double>(Settings->Radius##_AXIS##SourceAttribute);\
if (!SolidificationRad##_AXIS){ PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Some paths don't have the specified Radius Attribute \"{0}\"."), FText::FromName(Settings->Radius##_AXIS##SourceAttribute.GetName()))); return false; }}
		PCGEX_FOREACH_XYZ(PCGEX_CREATE_LOCAL_AXIS_GETTER)
#undef PCGEX_CREATE_LOCAL_AXIS_GETTER

		if (Settings->SolidificationLerpOperand == EPCGExFetchType::Attribute)
		{
			SolidificationLerpGetter = PointDataFacade->GetBroadcaster<double>(Settings->SolidificationLerpAttribute);
			if (!SolidificationLerpGetter)
			{
				PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FText::Format(FTEXT("Some paths don't have the specified SolidificationEdgeLerp Attribute \"{0}\"."), FText::FromName(Settings->SolidificationLerpAttribute.GetName())));
				return false;
			}
		}

		if (!bClosedLoop && Settings->bRemoveLastPoint) { PointIO->GetOut()->GetMutablePoints().RemoveAt(LastIndex); }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (!bClosedLoop && Index == LastIndex) { return; } // Skip last point

		const FVector Position = Point.Transform.GetLocation();
		const FVector NextPosition = PointDataFacade->Source->GetInPoint(Index == LastIndex ? 0 : Index + 1).Transform.GetLocation();
		const double Length = FVector::Dist(Position, NextPosition);
		const FVector EdgeDirection = (Position - NextPosition).GetSafeNormal();
		const FVector Scale = Settings->bScaleBounds ? FVector::OneVector / Point.Transform.GetScale3D() : FVector::OneVector;


		FRotator EdgeRot;
		FVector TargetBoundsMin = Point.BoundsMin;
		FVector TargetBoundsMax = Point.BoundsMax;

		const double EdgeLerp = FMath::Clamp(SolidificationLerpGetter ? SolidificationLerpGetter->Read(Index) : Settings->SolidificationLerpConstant, 0, 1);
		const double EdgeLerpInv = 1 - EdgeLerp;
		bool bProcessAxis;

#define PCGEX_SOLIDIFY_DIMENSION(_AXIS)\
		bProcessAxis = Settings->bWriteRadius##_AXIS || Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS;\
		if (bProcessAxis){\
			if (Settings->SolidificationAxis == EPCGExMinimalAxis::_AXIS){\
				TargetBoundsMin._AXIS = (-Length * EdgeLerpInv)* Scale._AXIS;\
				TargetBoundsMax._AXIS = (Length * EdgeLerp)* Scale._AXIS;\
			}else{\
				double Rad = Rad##_AXIS##Constant;\
				if(SolidificationRad##_AXIS){Rad = FMath::Lerp(SolidificationRad##_AXIS->Read(Index), SolidificationRad##_AXIS->Read(Index), EdgeLerp); }\
				else{Rad=Settings->Radius##_AXIS##Constant;}\
				TargetBoundsMin._AXIS = -Rad;\
				TargetBoundsMax._AXIS = Rad;\
			}\
		}

		PCGEX_FOREACH_XYZ(PCGEX_SOLIDIFY_DIMENSION)
#undef PCGEX_SOLIDIFY_DIMENSION

		switch (Settings->SolidificationAxis)
		{
		default:
		case EPCGExMinimalAxis::X:
			EdgeRot = FRotationMatrix::MakeFromX(EdgeDirection).Rotator();
			break;
		case EPCGExMinimalAxis::Y:
			EdgeRot = FRotationMatrix::MakeFromY(EdgeDirection).Rotator();
			break;
		case EPCGExMinimalAxis::Z:
			EdgeRot = FRotationMatrix::MakeFromZ(EdgeDirection).Rotator();
			break;
		}

		Point.Transform = FTransform(EdgeRot, FMath::Lerp(Position, NextPosition, EdgeLerp), Point.Transform.GetScale3D());

		Point.BoundsMin = TargetBoundsMin;
		Point.BoundsMax = TargetBoundsMax;
	}

	void FProcessor::CompleteWork()
	{
		//PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
