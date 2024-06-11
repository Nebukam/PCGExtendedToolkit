// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathProcessor.h"

#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExPathProcessorElement"

UPCGExPathProcessorSettings::UPCGExPathProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExPathProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (SupportsPointFilters()) { PCGEX_PIN_PARAMS(GetPointFilterLabel(), "Path points filters", Advanced, {}) }
	return PinProperties;
}

PCGExData::EInit UPCGExPathProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FName UPCGExPathProcessorSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }
FName UPCGExPathProcessorSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

FName UPCGExPathProcessorSettings::GetPointFilterLabel() const { return NAME_None; }
bool UPCGExPathProcessorSettings::SupportsPointFilters() const { return !GetPointFilterLabel().IsNone(); }

FPCGExPathProcessorContext::~FPCGExPathProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_UOBJECT(PointFiltersData)
	PCGEX_DELETE(PointFiltersHandler)
	PointFilterResults.Empty();
}

bool FPCGExPathProcessorContext::ProcessorAutomation()
{
	if (!FPCGExPointsProcessorContext::ProcessorAutomation()) { return false; }
	return ProcessFilters();
}

bool FPCGExPathProcessorContext::AdvancePointsIO()
{
	PCGEX_SETTINGS_LOCAL(PathProcessor)

	PCGEX_DELETE(PointFiltersHandler)

	if (!FPCGExPointsProcessorContext::AdvancePointsIO()) { return false; }

	const bool DefaultResult = DefaultPointFilterResult();

	if (PointFiltersData)
	{
		PointFilterResults.SetNumUninitialized(CurrentIO->GetNum());
		for (int i = 0; i < PointFilterResults.Num(); i++) { PointFilterResults[i] = DefaultResult; }

		PointFiltersHandler = static_cast<PCGExCluster::FNodeStateHandler*>(PointFiltersData->CreateFilter());
		PointFiltersHandler->bCacheResults = false;
		PointFiltersHandler->Capture(this, CurrentIO);

		bRequirePointFilterPreparation = PointFiltersHandler->PrepareForTesting(CurrentIO);
		bWaitingOnFilterWork = true;
	}
	else if (Settings->SupportsPointFilters())
	{
		PointFilterResults.SetNumUninitialized(CurrentIO->GetNum());
		for (int i = 0; i < PointFilterResults.Num(); i++) { PointFilterResults[i] = DefaultResult; }
	}

	return true;
}

bool FPCGExPathProcessorContext::ProcessFilters()
{
	if (!bWaitingOnFilterWork) { return true; }

	if (bRequirePointFilterPreparation)
	{
		auto PrepareVtx = [&](const int32 Index) { PointFiltersHandler->PrepareSingle(Index); };
		if (!Process(PrepareVtx, CurrentIO->GetNum())) { return false; }
		bRequirePointFilterPreparation = false;
	}

	if (PointFiltersHandler)
	{
		auto FilterVtx = [&](const int32 Index) { PointFilterResults[Index] = PointFiltersHandler->Test(Index); };
		if (!Process(FilterVtx, CurrentIO->GetNum())) { return false; }
		PCGEX_DELETE(PointFiltersHandler)
	}

	bWaitingOnFilterWork = false;

	return true;
}

bool FPCGExPathProcessorContext::DefaultPointFilterResult() const { return true; }

PCGEX_INITIALIZE_CONTEXT(PathProcessor)

bool FPCGExPathProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathProcessor)

	if (Settings->SupportsPointFilters())
	{
		TArray<UPCGExFilterFactoryBase*> FilterFactories;
		if (PCGExFactories::GetInputFactories(InContext, Settings->GetPointFilterLabel(), FilterFactories, {PCGExFactories::EType::Filter}, false))
		{
			Context->PointFiltersData = NewObject<UPCGExNodeStateFactory>();
			Context->PointFiltersData->FilterFactories.Append(FilterFactories);
		}
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
