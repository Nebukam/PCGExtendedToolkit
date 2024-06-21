// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSplitPath.h"

#include "Data/PCGExDataFilter.h"

#define LOCTEXT_NAMESPACE "PCGExSplitPathElement"
#define PCGEX_NAMESPACE SplitPath

TArray<FPCGPinProperties> UPCGExSplitPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "Intersection targets", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSplitPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExSplitPathSettings::GetPointFilterLabel() const { return FName("SplitConditions"); }
bool UPCGExSplitPathSettings::RequiresPointFilters() const { return true; }


PCGEX_INITIALIZE_ELEMENT(SplitPath)

FPCGExSplitPathContext::~FPCGExSplitPathContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(MainPaths)
}

bool FPCGExSplitPathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

	Context->MainPaths = new PCGExData::FPointIOCollection();
	Context->MainPaths->DefaultOutputLabel = Settings->GetMainOutputLabel();

	return true;
}

bool FPCGExSplitPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSplitPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SplitPath)

	PCGE_LOG(Error, GraphAndLog, FTEXT("NOT IMPLEMENTED YET"));
	return true;
	/*
	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSplitPath::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Settings->SplitAction == EPCGExPathSplitAction::Split && Entry->GetNum() < 3)
				{
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 3 points and won't be processed."));
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExSplitPath::FProcessor>* NewBatch)
			{
				NewBatch->SetPointsFilterData(&Context->FilterFactories);
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to split."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Context->IsDone())
	{
		Context->MainPaths->OutputTo(Context);
		Context->ExecuteEnd();
	}

	return Context->IsDone();
	*/
}

namespace PCGExSplitPath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SplitPath)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();

		bool bAnySplit = false;


		for (const bool Result : PointFilterCache)
		{
			if (Result)
			{
				bAnySplit = true;
				break;
			}
		}

		if (!bAnySplit)
		{
			TypedContext->MainPaths->Emplace_GetRef(PointIO->GetIn(), PCGExData::EInit::Forward);
			return false;
		}

		// TODO : Go through each point and split/create new partition at each split

		for (int i = 0; i < InPoints.Num(); i++)
		{
			const FPCGPoint& CurrentPoint = InPoints[i];

			// Note: Start and End can never split, but can be removed.
		}

		// Context->MainPaths->Emplace_GetRef(PointIO->GetIn(), PCGExData::EInit::DuplicateInput);

		return true;
	}

	void FProcessor::CompleteWork()
	{
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
