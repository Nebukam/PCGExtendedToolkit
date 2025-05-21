// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExAttributesToTags.h"

#include "Data/PCGExDataForward.h"
#include "Misc/Pickers/PCGExPicker.h"
#include "Misc/Pickers/PCGExPickerFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExAttributesToTagsElement"
#define PCGEX_NAMESPACE AttributesToTags

bool UPCGExAttributesToTagsSettings::GetIsMainTransactional() const
{
	return true;
}

TArray<FPCGPinProperties> UPCGExAttributesToTagsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Resolution != EPCGExAttributeToTagsResolution::Self)
	{
		PCGEX_PIN_ANY(FName("Tags Source"), "Source collection(s) to read the tags from.", Required, {})
	}

	if (Selection == EPCGExCollectionEntrySelection::Picker ||
		Selection == EPCGExCollectionEntrySelection::PickerFirst ||
		Selection == EPCGExCollectionEntrySelection::PickerLast)
	{
		PCGEX_PIN_PARAMS(PCGExPicker::SourcePickersLabel, "Pickers config", Required, {})
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExAttributesToTagsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (Action == EPCGExAttributeToTagsAction::AddTags)
	{
		PCGEX_PIN_ANY(GetMainOutputPin(), "The processed input.", Normal, {})
	}
	else
	{
		PCGEX_PIN_PARAMS(FName("Tags"), "Tags value in the format `AttributeName = AttributeName:AttributeValue`", Required, {})
	}


	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(AttributesToTags)

bool FPCGExAttributesToTagsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributesToTags)

	Context->Attributes = Settings->Attributes;
	PCGExHelpers::AppendUniqueSelectorsFromCommaSeparatedList(Settings->CommaSeparatedAttributeSelectors, Context->Attributes);

	if (Settings->Resolution == EPCGExAttributeToTagsResolution::Self)
	{
		return true;
	}

	// Converting collection
	PCGEX_MAKE_SHARED(SourceCollection, PCGExData::FPointIOCollection, InContext, FName("Tags Source"), PCGExData::EIOInit::None, true)

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
		if (SourceCollection->Num() != 1 && !Settings->bQuietTooManyCollectionsWarning)
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
		Details.Attributes = Context->Attributes;

		PCGEX_MAKE_SHARED(SourceFacade, PCGExData::FFacade, SourceCollection->Pairs[i].ToSharedRef())
		Context->SourceDataFacades.Add(SourceFacade);

		if (!Details.Init(Context, SourceFacade)) { return false; }
	}

	if (Settings->Selection == EPCGExCollectionEntrySelection::Picker ||
		Settings->Selection == EPCGExCollectionEntrySelection::PickerFirst ||
		Settings->Selection == EPCGExCollectionEntrySelection::PickerLast)
	{
		PCGExFactories::GetInputFactories(Context, PCGExPicker::SourcePickersLabel, Context->PickerFactories, {PCGExFactories::EType::IndexPicker}, false);
		if (Context->PickerFactories.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing pickers."));
			return false;
		}
	}

	return true;
}

bool FPCGExAttributesToTagsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributesToTagsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AttributesToTags)
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

	if (Settings->Action == EPCGExAttributeToTagsAction::AddTags)
	{
		Context->MainPoints->StageAnyOutputs();
	}
	else
	{
		Context->MainBatch->Output();
	}

	return Context->TryComplete();
}

namespace PCGExAttributesToTags
{
	void FProcessor::Tag(const FPCGExAttributeToTagDetails& InDetails, const int32 Index) const
	{
		const PCGExData::FConstPoint Point = PointDataFacade->GetInPoint(Index);
		if (OutputSet) { InDetails.Tag(Point, OutputSet->Metadata); }
		else { InDetails.Tag(Point, PointDataFacade->Source); }
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributesToTags::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->Action == EPCGExAttributeToTagsAction::Attribute)
		{
			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::None)
		}
		else
		{
			PCGEX_INIT_IO(PointDataFacade->Source, Settings->bCleanupConsumableAttributes ? PCGExData::EIOInit::Duplicate : PCGExData::EIOInit::Forward)
		}


		FRandomStream RandomSource(BatchIndex);

		for (int i = 0; i < Context->Attributes.Num(); i++)
		{
			FPCGAttributePropertyInputSelector Selector = Context->Attributes[i].CopyAndFixLast(PointDataFacade->Source->GetIn());
			Context->AddConsumableAttributeName(Selector.GetName());
		}

		if (Settings->Action == EPCGExAttributeToTagsAction::Attribute)
		{
			OutputSet = Context->ManagedObjects->New<UPCGParamData>();
			OutputSet->Metadata->AddEntry();
		}

		if (Settings->Resolution == EPCGExAttributeToTagsResolution::Self)
		{
			FPCGExAttributeToTagDetails Details = FPCGExAttributeToTagDetails();
			Details.bAddIndexTag = false;
			Details.bPrefixWithAttributeName = Settings->bPrefixWithAttributeName;
			Details.Attributes = Context->Attributes;

			if (!Details.Init(Context, PointDataFacade)) { return false; }

			switch (Settings->Selection)
			{
			case EPCGExCollectionEntrySelection::FirstIndex:
				Tag(Details, 0);
				break;
			case EPCGExCollectionEntrySelection::LastIndex:
				Tag(Details, PointDataFacade->GetNum() - 1);
				break;
			case EPCGExCollectionEntrySelection::RandomIndex:
				Tag(Details, RandomSource.RandRange(0, PointDataFacade->GetNum() - 1));
				break;
			case EPCGExCollectionEntrySelection::Picker:
			case EPCGExCollectionEntrySelection::PickerFirst:
			case EPCGExCollectionEntrySelection::PickerLast:
				TagWithPickers(Details);
				break;
			}
		}
		else
		{
			FPCGExAttributeToTagDetails& Details = Context->Details[Settings->Resolution == EPCGExAttributeToTagsResolution::CollectionToCollection ? BatchIndex : 0];

			switch (Settings->Selection)
			{
			case EPCGExCollectionEntrySelection::FirstIndex:
				Tag(Details, 0);
				break;
			case EPCGExCollectionEntrySelection::LastIndex:
				Tag(Details, PointDataFacade->GetNum() - 1);
				break;
			case EPCGExCollectionEntrySelection::RandomIndex:
				Tag(Details, RandomSource.RandRange(0, PointDataFacade->GetNum() - 1));
				break;
			case EPCGExCollectionEntrySelection::Picker:
			case EPCGExCollectionEntrySelection::PickerFirst:
			case EPCGExCollectionEntrySelection::PickerLast:
				TagWithPickers(Details);
				break;
			}
		}

		return true;
	}

	void FProcessor::TagWithPickers(const FPCGExAttributeToTagDetails& InDetails)
	{
		TSet<int32> UniqueIndices;

		for (const TObjectPtr<const UPCGExPickerFactoryData>& Op : Context->PickerFactories)
		{
			Op->AddPicks(InDetails.SourceDataFacade->GetNum(), UniqueIndices);
		}

		if (UniqueIndices.IsEmpty()) { return; }

		TArray<int32> UniqueIndicesTemp = UniqueIndices.Array();
		UniqueIndices.Sort([](const int32 A, const int32 B) { return A < B; });

		if (Settings->Selection == EPCGExCollectionEntrySelection::Picker)
		{
			for (const int32 i : UniqueIndicesTemp)
			{
				if (i != -1) { Tag(InDetails, i); }
			}
		}
		if (Settings->Selection == EPCGExCollectionEntrySelection::PickerFirst)
		{
			for (int i = 0; i < UniqueIndicesTemp.Num(); i++)
			{
				if (UniqueIndicesTemp[i] != -1)
				{
					Tag(InDetails, UniqueIndicesTemp[i]);
					return;
				}
			}
		}
		else if (Settings->Selection == EPCGExCollectionEntrySelection::PickerLast)
		{
			for (int i = UniqueIndicesTemp.Num() - 1; i >= 0; i--)
			{
				if (UniqueIndicesTemp[i] != -1)
				{
					Tag(InDetails, UniqueIndicesTemp[i]);
					return;
				}
			}
		}
	}

	void FProcessor::Output()
	{
		TPointsProcessor<FPCGExAttributesToTagsContext, UPCGExAttributesToTagsSettings>::Output();
		if (OutputSet) { Context->StageOutput(FName("Tags"), OutputSet, false); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
