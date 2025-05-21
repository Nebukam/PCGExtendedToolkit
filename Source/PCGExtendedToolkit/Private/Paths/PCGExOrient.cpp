// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExOrient.h"


#include "Paths/Orient/PCGExOrientAverage.h"

#define LOCTEXT_NAMESPACE "PCGExOrientElement"
#define PCGEX_NAMESPACE Orient

TArray<FPCGPinProperties> UPCGExOrientSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExOrient::SourceOverridesOrient)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(Orient)

bool FPCGExOrientElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Orient)

	if (!Settings->Orientation)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Please select an orientation module in the detail panel."));
		return false;
	}

	if (Settings->Output == EPCGExOrientUsage::OutputToAttribute) { PCGEX_VALIDATE_NAME(Settings->OutputAttribute); }
	if (Settings->bOutputDot) { PCGEX_VALIDATE_NAME(Settings->DotAttribute); }

	PCGEX_OPERATION_BIND(Orientation, UPCGExOrientOperation, PCGExOrient::SourceOverridesOrient)
	Context->Orientation->OrientAxis = Settings->OrientAxis;
	Context->Orientation->UpAxis = Settings->UpAxis;

	return true;
}

bool FPCGExOrientElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOrientElement::Execute);

	PCGEX_CONTEXT(Orient)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExOrient::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EIOInit::Forward);
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExOrient::FProcessor>>& NewBatch)
			{
			}))
		{
			Context->CancelExecution(TEXT("Could not find any paths to orient."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExOrient
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExOrient::Process);

		DefaultPointFilterValue = Settings->bFlipDirection;

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		Path = PCGExPaths::MakePath(PointDataFacade->GetIn(), 0, Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source));
		//PathBinormal = Path->AddExtra<PCGExPaths::FPathEdgeBinormal>(false);

		LastIndex = PointDataFacade->GetNum() - 1;
		
		Orient = Context->Orientation->CreateOperation();
		if (!Orient->PrepareForData(PointDataFacade, Path.ToSharedRef())) { return false; }

		if (Settings->Output == EPCGExOrientUsage::OutputToAttribute)
		{
			TransformWriter = PointDataFacade->GetWritable<FTransform>(Settings->OutputAttribute, PCGExData::EBufferInit::Inherit);
		}

		if (Settings->bOutputDot)
		{
			DotWriter = PointDataFacade->GetWritable<double>(Settings->DotAttribute, PCGExData::EBufferInit::Inherit);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		TPCGValueRange<FTransform> OutTransform = PointDataFacade->GetOut()->GetTransformValueRange();
		PCGEX_SCOPE_LOOP(Index)
		{
			if (Path->IsValidEdgeIndex(Index)) { Path->ComputeEdgeExtra(Index); }

			const FTransform OutT = Orient->ComputeOrientation(PointDataFacade->GetOutPoint(Index), PointFilterCache[Index] ? -1 : 1);
			if (Settings->bOutputDot) { DotWriter->GetMutable(Index) = FVector::DotProduct(Path->DirToPrevPoint(Index) * -1, Path->DirToNextPoint(Index)); }

			if (TransformWriter) { TransformWriter->GetMutable(Index) = OutT; }
			else { OutTransform[Index] = OutT; }
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
