﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBlendPath.h"

#include "opencv2/gapi/fluid/gfluidbuffer.hpp"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExBlendPathElement"
#define PCGEX_NAMESPACE BlendPath

UPCGExBlendPathSettings::UPCGExBlendPathSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_OPERATION_DEFAULT(BlendPathing, UPCGExMovingAverageBlendPathing)
}

#if WITH_EDITOR
void UPCGExBlendPathSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExBlendPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(BlendPath)

FPCGExBlendPathContext::~FPCGExBlendPathContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExBlendPathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BlendPath)

	PCGEX_FWD(SubdivideMethod)

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)
	Context->Blending->bClosedPath = Settings->bClosedPath;

	return true;
}

bool FPCGExBlendPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBlendPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BlendPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		int32 Index = 0;
		Context->MainPoints->ForEach(
			[&](PCGExData::FPointIO& PointIO, const int32)
			{
				if (PointIO.GetNum() > 2)
				{
					PointIO.CreateInKeys();
					PointIO.CreateOutKeys();
					Context->GetAsyncManager()->Start<FPCGExBlendPathTask>(Index++, &PointIO);
				}
			});
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->OutputPoints();
		Context->Done();
	}

	return Context->IsDone();
}

bool FPCGExBlendPathTask::ExecuteTask()
{
	const FPCGExBlendPathContext* Context = Manager->GetContext<FPCGExBlendPathContext>();
	TArrayView<FPCGPoint> PathPoints = MakeArrayView(PointIO->GetOut()->GetMutablePoints());

	if (PathPoints.IsEmpty()) { return false; }

	const PCGExDataBlending::FMetadataBlender* Blender = Context->Blending->CreateBlender(*PointIO, *PointIO, true);

	const PCGEx::FPointRef StartPoint = PointIO->GetOutPointRef(0);
	const PCGEx::FPointRef EndPoint = PointIO->GetOutPointRef(PathPoints.Num() - 1);

	const PCGExMath::FPathMetrics* Metrics = new PCGExMath::FPathMetrics(PathPoints);	
	Context->Blending->BlendSubPoints(StartPoint, EndPoint, PathPoints, *Metrics, Blender);

	PCGEX_DELETE(Blender);
	PCGEX_DELETE(Metrics);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE