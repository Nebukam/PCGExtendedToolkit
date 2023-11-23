// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transforms/PCGExSampleSignedDistance.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "PCGPin.h"
#include <algorithm>

#define LOCTEXT_NAMESPACE "PCGExSampleSignedDistanceElement"

UPCGExSampleSignedDistanceSettings::UPCGExSampleSignedDistanceSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExSampleSignedDistanceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertySourceTargets = PinProperties.Emplace_GetRef(PCGEx::SourceTargetPolylinesLabel, EPCGDataType::Point | EPCGDataType::PolyLine, true, true);

#if WITH_EDITOR
	PinPropertySourceTargets.Tooltip = LOCTEXT("PCGExSourceTargetsPolylinesPinTooltip", "Polylines (curve) to sample Signed distance against.");
#endif // WITH_EDITOR

	return PinProperties;
}

PCGEx::EIOInit UPCGExSampleSignedDistanceSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

int32 UPCGExSampleSignedDistanceSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleSignedDistanceSettings::CreateElement() const { return MakeShared<FPCGExSampleSignedDistanceElement>(); }

FPCGContext* FPCGExSampleSignedDistanceElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleSignedDistanceContext* Context = new FPCGExSampleSignedDistanceContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleSignedDistanceSettings* Settings = Context->GetInputSettings<UPCGExSampleSignedDistanceSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Targets = InputData.GetInputsByPin(PCGEx::SourceTargetPointsLabel);
	if (!Targets.IsEmpty())
	{
		const FPCGTaggedData& Target = Targets[0];
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Target.Data))
		{
			if (const UPCGPolyLineData* LineData = PCGEx::Common::GetPolyLineData(SpatialData))
			{
				Context->TargetPolyLines.Add(const_cast<UPCGPolyLineData*>(LineData));
			}
			else if (const UPCGPointData* PointData = SpatialData->ToPointData(Context))
			{
				Context->TargetPoints.Add(const_cast<UPCGPointData*>(PointData));
			}
		}
	}

	return Context;
}

bool FPCGExSampleSignedDistanceElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleSignedDistanceContext* Context = static_cast<FPCGExSampleSignedDistanceContext*>(InContext);
	const UPCGExSampleSignedDistanceSettings* Settings = Context->GetInputSettings<UPCGExSampleSignedDistanceSettings>();
	check(Settings);

	if (!Context->TargetPoints.IsEmpty() && Context->TargetPolyLines.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingTargets", "No targets (either no input or empty dataset)"));
		return false;
	}

	return true;
}

bool FPCGExSampleSignedDistanceElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSignedDistanceElement::Execute);

	FPCGExSampleSignedDistanceContext* Context = static_cast<FPCGExSampleSignedDistanceContext*>(InContext);

	if (Context->IsState(PCGExMT::EState::Setup))
	{
		if (!Validate(Context)) { return true; }
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

	auto InitializeForIO = [&Context, this](UPCGExPointIO* IO)
	{
		IO->BuildMetadataEntries();
		// TODO : Initialize output attribute here
	};

	auto ProcessPoint = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		// TODO: We can bulk-parallelize this, by caching out attribute for each output beforehand (like write index)
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeForIO, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->TargetPoints.Empty();
		Context->TargetPolyLines.Empty();
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
