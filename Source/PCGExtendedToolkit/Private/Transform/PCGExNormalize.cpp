// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExNormalize.h"

#include "Data/PCGExData.h"
#include "Sampling/PCGExSampling.h"


#define LOCTEXT_NAMESPACE "PCGExNormalizeElement"
#define PCGEX_NAMESPACE Normalize

UPCGExNormalizeSettings::UPCGExNormalizeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (Output.GetName() == TEXT("@Last")) { Output.Update("$Position"); }
}

PCGEX_INITIALIZE_ELEMENT(Normalize)

TArray<FPCGPinProperties> UPCGExNormalizeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExTransform::SourceDeformersBoundsLabel, "Point data that will be used as unified bounds for all inputs", Normal, {})
	return PinProperties;
}

bool UPCGExNormalizeSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExTransform::SourceDeformersBoundsLabel) { return InPin->EdgeCount() > 0; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

bool FPCGExNormalizeElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Normalize)

	TArray<FPCGTaggedData> UnifiedBounds = Context->InputData.GetSpatialInputsByPin(PCGExTransform::SourceDeformersBoundsLabel);
	for (int i = 0; i < UnifiedBounds.Num(); ++i)
	{
		if (const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(UnifiedBounds[i].Data))
		{
			Context->bUseUnifiedBounds = true;
			Context->UnifiedBounds += PCGExTransform::GetBounds(PointData, Settings->BoundsSource);
		}
	}

	return true;
}

bool FPCGExNormalizeElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExNormalizeElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Normalize)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExNormalize::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExNormalize::FProcessor>>& NewBatch)
			{
				NewBatch->bSkipCompletion = true;
			}))
		{
			return Context->CancelExecution(TEXT("No data."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExNormalize
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExNormalize::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		Box = Context->bUseUnifiedBounds ? Context->UnifiedBounds : PCGExTransform::GetBounds(PointDataFacade->GetIn(), Settings->BoundsSource);
		Size = Box.GetSize();

		OneMinus[0] = EnumHasAnyFlags(static_cast<EPCGExApplySampledComponentFlags>(Settings->OneMinus), EPCGExApplySampledComponentFlags::X);
		OneMinus[1] = EnumHasAnyFlags(static_cast<EPCGExApplySampledComponentFlags>(Settings->OneMinus), EPCGExApplySampledComponentFlags::Y);
		OneMinus[2] = EnumHasAnyFlags(static_cast<EPCGExApplySampledComponentFlags>(Settings->OneMinus), EPCGExApplySampledComponentFlags::Z);

		PCGExData::FProxyDescriptor Descriptor = PCGExData::FProxyDescriptor();
		Descriptor.DataFacade = PointDataFacade;
		Descriptor.Role = PCGExData::EProxyRole::Write;

		Descriptor.Capture(Context, Settings->Output, PCGExData::EIOSide::Out, false);

		Descriptor.WorkingType = EPCGMetadataTypes::Vector;
		if (Descriptor.RealType == EPCGMetadataTypes::Unknown) { Descriptor.RealType = EPCGMetadataTypes::Vector; }

		OutputBuffer = StaticCastSharedPtr<PCGExData::TBufferProxy<FVector>>(PCGExData::GetProxyBuffer(Context, Descriptor));

		if (!OutputBuffer.IsValid()) { return false; }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::Normalize::ProcessPoints);

		UPCGBasePointData* OutPoints = PointDataFacade->GetOut();
		const TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		if (Settings->bWrap)
		{
			const FVector OnePlus = FVector(1 + UE_SMALL_NUMBER);
			PCGEX_SCOPE_LOOP(Index)
			{
				FVector UVW = PCGExMath::Tile(Settings->Offset + ((InTransforms[Index].GetLocation() - Box.Min) * Settings->Tile) / Size, FVector::ZeroVector, OnePlus);
				for (int i = 0; i < 3; i++) { if (OneMinus[i]) { UVW[i] = 1 - UVW[i]; } }
				OutputBuffer->Set(Index, UVW);
			}
		}
		else
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				FVector UVW = Settings->Offset + ((InTransforms[Index].GetLocation() - Box.Min) * Settings->Tile) / Size;
				for (int i = 0; i < 3; i++) { if (OneMinus[i]) { UVW[i] = 1 - UVW[i]; } }
				OutputBuffer->Set(Index, UVW);
			}
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		PointDataFacade->WriteFastest(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
