// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExAttributesToTags.h"

#include "Data/PCGExDataForward.h"


#define LOCTEXT_NAMESPACE "PCGExAttributesToTagsElement"
#define PCGEX_NAMESPACE AttributesToTags

PCGExData::EIOInit UPCGExAttributesToTagsSettings::GetMainOutputInitMode() const { return bCleanupConsumableAttributes ? PCGExData::EIOInit::Duplicate : PCGExData::EIOInit::Forward; }

TArray<FPCGPinProperties> UPCGExAttributesToTagsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Resolution != EPCGExAttributeToTagsResolution::Self)
	{
		PCGEX_PIN_POINTS(FName("Tags Source"), "Source collection(s) to read the tags from.", Required, {})
	}
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(AttributesToTags)

bool FPCGExAttributesToTagsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributesToTags)

	if (Settings->Resolution == EPCGExAttributeToTagsResolution::Self)
	{
		return true;
	}

	PCGEX_MAKE_SHARED(SourceCollection, PCGExData::FPointIOCollection, InContext, FName("Tags Source"))

	if (SourceCollection->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Source collections are empty."));
		return false;
	}

	int32 NumIterations = 0;

	if (Settings->Resolution == EPCGExAttributeToTagsResolution::CollectionToCollection)
	{
		if (SourceCollection->Num() != Context->MainPoints->Num())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Number of input collections don't match the number of sources."));
			return false;
		}

		NumIterations = SourceCollection->Num();
	}
	else
	{
		if (SourceCollection->Num() != 1)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("More that one collections found in the sources, only the first one will be used."));
		}

		NumIterations = 1;
	}

	Context->SourceDataFacades.Reserve(NumIterations);
	Context->Details.Reserve(NumIterations);
	for (int i = 0; i < NumIterations; i++)
	{
		FPCGExAttributeToTagDetails& Details = Context->Details.Emplace_GetRef();
		Details.bAddIndexTag = false;
		Details.bPrefixWithAttributeName = Settings->bPrefixWithAttributeName;
		Details.Attributes = Settings->Attributes;

		PCGEX_MAKE_SHARED(SourceFacade, PCGExData::FFacade, SourceCollection->Pairs[i].ToSharedRef())
		Context->SourceDataFacades.Add(SourceFacade);

		if (!Details.Init(Context, SourceFacade)) { return false; }
	}

	return true;
}

bool FPCGExAttributesToTagsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributesToTagsElement::Execute);

	PCGEX_CONTEXT(AttributesToTags)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExAttributesToTags::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExAttributesToTags::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExAttributesToTags
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributesToTags::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		FRandomStream RandomSource(BatchIndex);

		for(int i = 0; i < Settings->Attributes.Num(); i++)
		{
			FPCGAttributePropertyInputSelector Selector = Settings->Attributes[i].CopyAndFixLast(PointDataFacade->Source->GetIn());
			Context->AddConsumableAttributeName(Selector.GetName());
		}
		
		TSet<FString> OutTags;

		if (Settings->Resolution == EPCGExAttributeToTagsResolution::Self)
		{
			FPCGExAttributeToTagDetails Details = FPCGExAttributeToTagDetails();
			Details.bAddIndexTag = false;
			Details.bPrefixWithAttributeName = Settings->bPrefixWithAttributeName;
			Details.Attributes = Settings->Attributes;

			if (!Details.Init(Context, PointDataFacade)) { return false; }

			switch (Settings->Selection)
			{
			case EPCGExCollectionEntrySelection::FirstIndex:
				Details.Tag(0, OutTags);
				break;
			case EPCGExCollectionEntrySelection::LastIndex:
				Details.Tag(PointDataFacade->GetNum() - 1, OutTags);
				break;
			case EPCGExCollectionEntrySelection::RandomIndex:
				Details.Tag(RandomSource.RandRange(0, PointDataFacade->GetNum() - 1), OutTags);
				break;
			}
		}
		else
		{
			FPCGExAttributeToTagDetails& Details = Context->Details[Settings->Resolution == EPCGExAttributeToTagsResolution::CollectionToCollection ? BatchIndex : 0];

			switch (Settings->Selection)
			{
			case EPCGExCollectionEntrySelection::FirstIndex:
				Details.Tag(0, OutTags);
				break;
			case EPCGExCollectionEntrySelection::LastIndex:
				Details.Tag(PointDataFacade->GetNum() - 1, OutTags);
				break;
			case EPCGExCollectionEntrySelection::RandomIndex:
				Details.Tag(RandomSource.RandRange(0, PointDataFacade->GetNum() - 1), OutTags);
				break;
			}
		}

		PointDataFacade->Source->Tags->Append(OutTags);

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
