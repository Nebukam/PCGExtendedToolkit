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
	if (SupportsPointFilters())
	{
		if (RequiresPointFilters()) { PCGEX_PIN_PARAMS(GetPointFilterLabel(), "Path points processing filters", Required, {}) }
		else { PCGEX_PIN_PARAMS(GetPointFilterLabel(), "Path points processing filters", Advanced, {}) }
	}
	return PinProperties;
}

PCGExData::EInit UPCGExPathProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FName UPCGExPathProcessorSettings::GetMainInputLabel() const { return PCGExGraph::SourcePathsLabel; }
FName UPCGExPathProcessorSettings::GetMainOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

FName UPCGExPathProcessorSettings::GetPointFilterLabel() const { return NAME_None; }
bool UPCGExPathProcessorSettings::SupportsPointFilters() const { return !GetPointFilterLabel().IsNone(); }
bool UPCGExPathProcessorSettings::RequiresPointFilters() const { return false; }

FPCGExPathProcessorContext::~FPCGExPathProcessorContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(PointFiltersManager)
}

bool FPCGExPathProcessorContext::ProcessorAutomation()
{
	if (!FPCGExPointsProcessorContext::ProcessorAutomation()) { return false; }
	return ProcessFilters();
}

bool FPCGExPathProcessorContext::AdvancePointsIO()
{
	PCGEX_SETTINGS_LOCAL(PathProcessor)

	PCGEX_DELETE(PointFiltersManager)

	if (!FPCGExPointsProcessorContext::AdvancePointsIO()) { return false; }

	const bool DefaultResult = DefaultPointFilterResult();

	if (PrepareFiltersWithAdvance())
	{
		if (Settings->SupportsPointFilters())
		{
			PCGEX_DELETE(PointFiltersManager)
			PointFiltersManager = CreatePointFilterManagerInstance(CurrentIO, false);

			if (!PointFiltersManager->bValid)
			{
				for (bool& Result : PointFiltersManager->Results) { Result = DefaultResult; }
			}
			else
			{
				PointFiltersManager->PrepareForTesting();
				bRequirePointFilterPreparation = PointFiltersManager->RequiresPerPointPreparation();
				bWaitingOnFilterWork = true;
			}
		}
	}

	return true;
}

bool FPCGExPathProcessorContext::ProcessFilters()
{
	if (!bWaitingOnFilterWork) { return true; }

	if (bRequirePointFilterPreparation)
	{
		auto PrepareVtx = [&](const int32 Index) { PointFiltersManager->PrepareSingle(Index); };
		if (!Process(PrepareVtx, CurrentIO->GetNum())) { return false; }
		bRequirePointFilterPreparation = false;
	}

	auto FilterVtx = [&](const int32 Index) { PointFiltersManager->Test(Index); };
	if (!Process(FilterVtx, CurrentIO->GetNum())) { return false; }

	bWaitingOnFilterWork = false;

	return true;
}

bool FPCGExPathProcessorContext::DefaultPointFilterResult() const { return true; }
bool FPCGExPathProcessorContext::PrepareFiltersWithAdvance() const { return true; }

PCGExDataFilter::TEarlyExitFilterManager* FPCGExPathProcessorContext::CreatePointFilterManagerInstance(const PCGExData::FPointIO* PointIO, const bool bForcePrepare) const
{
	PCGExDataFilter::TEarlyExitFilterManager* NewInstance = new PCGExDataFilter::TEarlyExitFilterManager(PointIO);
	NewInstance->Register<UPCGExFilterFactoryBase>(this, FilterFactories, PointIO);
	if (bForcePrepare)
	{
		NewInstance->PrepareForTesting();
		if (!DefaultPointFilterResult()) { for (bool& Result : NewInstance->Results) { Result = false; } }
	}
	return NewInstance;
}

PCGEX_INITIALIZE_CONTEXT(PathProcessor)

bool FPCGExPathProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathProcessor)

	if (Settings->SupportsPointFilters())
	{
		PCGExFactories::GetInputFactories(InContext, Settings->GetPointFilterLabel(), Context->FilterFactories, {PCGExFactories::EType::Filter}, false);
		if (Settings->RequiresPointFilters() && Context->FilterFactories.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(FTEXT("Missing {0}."), FText::FromName(Settings->GetPointFilterLabel())));
			return false;
		}
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
