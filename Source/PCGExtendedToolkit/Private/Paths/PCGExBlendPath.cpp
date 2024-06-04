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

void UPCGExBlendPathSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(Blending, UPCGExSubPointsBlendInterpolate)
}

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
		PCGEX_WAIT_ASYNC
		Context->OutputPoints();
		Context->Done();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExBlendPathTask::ExecuteTask()
{
	const FPCGExBlendPathContext* Context = Manager->GetContext<FPCGExBlendPathContext>();
	TArrayView<FPCGPoint> PathPoints = MakeArrayView(PointIO->GetOut()->GetMutablePoints());

	if (PathPoints.IsEmpty()) { return false; }

	PCGExDataBlending::FMetadataBlender* Blender = Context->Blending->CreateBlender(*PointIO, *PointIO);

	const PCGEx::FPointRef StartPoint = PointIO->GetOutPointRef(0);
	const PCGEx::FPointRef EndPoint = PointIO->GetOutPointRef(PathPoints.Num() - 1);

	const PCGExMath::FPathMetricsSquared* Metrics = new PCGExMath::FPathMetricsSquared(PathPoints);
	Context->Blending->BlendSubPoints(StartPoint, EndPoint, PathPoints, *Metrics, Blender);

	PCGEX_DELETE(Blender);
	PCGEX_DELETE(Metrics);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
