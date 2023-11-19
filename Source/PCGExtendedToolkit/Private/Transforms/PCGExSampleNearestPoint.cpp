// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "..\..\Public\Transforms\PCGExSampleNearestPoint.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"

PCGEx::EIOInit UPCGExSampleNearestPointSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExSampleNearestPointSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestPointElement>(); }

FPCGContext* FPCGExSampleNearestPointElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestPointContext* Context = new FPCGExSampleNearestPointContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNearestPointSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestPointSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Targets = InputData.GetInputsByPin(PCGEx::SourceTargetPointsLabel);
	if (!Targets.IsEmpty())
	{
		FPCGTaggedData& Target = Targets[0];
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Target.Data);
		if (!SpatialData) { return nullptr; }

		const UPCGPointData* PointData = SpatialData->ToPointData(Context);
		if (!PointData) { return nullptr; }

		Context->Targets = const_cast<UPCGPointData*>(PointData);
		Context->NumTargets = Context->Targets->GetPoints().Num();

		//TODO: Initialize target attribute readers here
	}

	Context->MaxDistance = Settings->MaxDistance;
	Context->bUseOctree = Settings->MaxDistance <= 0;

	PCGEX_FORWARD_OUT_ATTRIBUTE(Location)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Direction)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Normal)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Distance)

	return Context;
}

bool FPCGExSampleNearestPointElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleNearestPointContext* Context = static_cast<FPCGExSampleNearestPointContext*>(InContext);

	if (!Context->Targets || Context->NumTargets < 1)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingTargets", "No targets (either no input or empty dataset)"));
		return false;
	}

	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Location)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Direction)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Normal)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Distance)
	return true;
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	FPCGExSampleNearestPointContext* Context = static_cast<FPCGExSampleNearestPointContext*>(InContext);

	if (Context->IsState(PCGExMT::EState::Setup))
	{
		if (!Validate(Context)) { return true; }

		Context->Octree = Context->bUseOctree ? const_cast<UPCGPointData::PointOctree*>(&(Context->CurrentIO->Out->GetOctree())) : nullptr;
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto InitializeForIO = [&Context](UPCGExPointIO* IO)
	{
		IO->BuildMetadataEntries();
		PCGEX_INIT_ATTRIBUTE_OUT(Location, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Direction, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
	};

	auto ProcessPoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{

		
		auto ProcessTarget = [&ReadIndex](const FPCGPoint& OtherPoint)
		{
			
		};

		// First: Sample all possible targets
		if (Context->Octree)
		{
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(Context->MaxDistance));
			Context->Octree->FindElementsWithBoundsTest(
				Box,
				[&ProcessTarget](const FPCGPointRef& OtherPointRef)
				{
					const FPCGPoint* OtherPoint = OtherPointRef.Point;
					ProcessTarget(*OtherPoint);
				});
		}
		else
		{
			const TArray<FPCGPoint>& Targets = Context->Targets->GetPoints();
			for (const FPCGPoint& OtherPoint : Targets) { ProcessTarget(OtherPoint); }
		}

		// Weight targets
		
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeForIO, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
