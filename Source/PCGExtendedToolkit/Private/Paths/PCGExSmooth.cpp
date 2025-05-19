// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSmooth.h"

#include "Data/Blending/PCGExMetadataBlender.h"


#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#define LOCTEXT_NAMESPACE "PCGExSmoothElement"
#define PCGEX_NAMESPACE Smooth

TArray<FPCGPinProperties> UPCGExSmoothSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExSmooth::SourceOverridesSmoothing)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(Smooth)

bool FPCGExSmoothElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)
	PCGEX_OPERATION_BIND(SmoothingMethod, UPCGExSmoothingOperation, PCGExSmooth::SourceOverridesSmoothing)

	return true;
}

bool FPCGExSmoothElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSmoothElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSmooth::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSmooth::FProcessor>>& NewBatch)
			{
				NewBatch->PrimaryOperation = Context->SmoothingMethod;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to smooth."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSmooth
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSmooth::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);
		NumPoints = PointDataFacade->GetNum();

		MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Settings->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade);

		Influence = Settings->GetValueSettingInfluence();
		if (!Influence->Init(Context, PointDataFacade)) { return false; }

		Smoothing = Settings->GetValueSettingSmoothingAmount();
		if (!Smoothing->Init(Context, PointDataFacade)) { return false; }

		TypedOperation = Cast<UPCGExSmoothingOperation>(PrimaryOperation);

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }

			const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

			PCGExData::FConstPoint PtRef = PointIO->GetOutPoint(Index);
			const double LocalSmoothing = FMath::Clamp(Smoothing->Read(Index), 0, MAX_dbl) * Settings->ScaleSmoothingAmountAttribute;

			if ((Settings->bPreserveEnd && Index == NumPoints - 1) ||
				(Settings->bPreserveStart && Index == 0))
			{
				TypedOperation->SmoothSingle(PointIO, PtRef, LocalSmoothing, 0, MetadataBlender.Get(), bClosedLoop);
				continue;
			}

			TypedOperation->SmoothSingle(PointIO, PtRef, LocalSmoothing, Influence->Read(Index), MetadataBlender.Get(), bClosedLoop);
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
