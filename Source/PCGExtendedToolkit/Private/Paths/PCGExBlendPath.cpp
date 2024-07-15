// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBlendPath.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExBlendPathElement"
#define PCGEX_NAMESPACE BlendPath

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
		while (Context->AdvancePointsIO())
		{
			if (Context->CurrentIO->GetNum() > 2)
			{
				Context->CurrentIO->CreateInKeys();
				Context->CurrentIO->CreateOutKeys();
				Context->GetAsyncManager()->Start<FPCGExBlendPathTask>(Index++, Context->CurrentIO);
			}
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_ASYNC_WAIT
		Context->OutputMainPoints();
		Context->Done();
	}

	return Context->TryComplete();
}

bool FPCGExBlendPathTask::ExecuteTask()
{
	const FPCGExBlendPathContext* Context = Manager->GetContext<FPCGExBlendPathContext>();
	TArrayView<FPCGPoint> PathPoints = MakeArrayView(PointIO->GetOut()->GetMutablePoints());

	if (PathPoints.IsEmpty()) { return false; }

	PCGExData::FFacade* PathFacade = new PCGExData::FFacade(PointIO);

	PCGExDataBlending::FMetadataBlender* Blender = Context->Blending->CreateBlender(PathFacade, PathFacade);

	const PCGExData::FPointRef StartPoint = PointIO->GetOutPointRef(0);
	const PCGExData::FPointRef EndPoint = PointIO->GetOutPointRef(PathPoints.Num() - 1);

	const PCGExMath::FPathMetricsSquared* Metrics = new PCGExMath::FPathMetricsSquared(PathPoints);
	Context->Blending->BlendSubPoints(StartPoint, EndPoint, PathPoints, *Metrics, Blender);

	PCGEX_DELETE(Blender);
	PCGEX_DELETE(PathFacade);
	PCGEX_DELETE(Metrics);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
