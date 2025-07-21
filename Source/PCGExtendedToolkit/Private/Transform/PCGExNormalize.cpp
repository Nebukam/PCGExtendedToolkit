// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExNormalize.h"

#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExNormalizeElement"
#define PCGEX_NAMESPACE Normalize

UPCGExNormalizeSettings::UPCGExNormalizeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (Output.GetName() == TEXT("@Last")) { Output.Update("$Position"); }
}

PCGEX_INITIALIZE_ELEMENT(Normalize)

bool FPCGExNormalizeElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Normalize)

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

		Box = PCGExTransform::GetBounds(PointDataFacade->GetIn(), Settings->BoundsSource);
		Size = Box.GetSize() * Settings->Tile;

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
				const FVector UVW = PCGExMath::Tile(Settings->Offset + (InTransforms[Index].GetLocation() - Box.Min) / Size, FVector::ZeroVector, OnePlus);
				OutputBuffer->Set(Index, UVW);
			}
		}
		else
		{
			PCGEX_SCOPE_LOOP(Index)
			{
				const FVector UVW = Settings->Offset + (InTransforms[Index].GetLocation() - Box.Min) / Size;
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
