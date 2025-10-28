// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExRecursionTracker.h"


#include "PCGExHelpers.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointFilter.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE RecursionTracker

#pragma region UPCGSettings interface

#if WITH_EDITOR
bool UPCGExRecursionTrackerSettings::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const
{
	return GetDefault<UPCGExGlobalSettings>()->GetPinExtraIcon(InPin, OutExtraIcon, OutTooltip, InPin->IsOutputPin());
}
#endif


TArray<FPCGPinProperties> UPCGExRecursionTrackerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	PCGEX_PIN_PARAMS(PCGPinConstants::DefaultInputLabel, "Tracker(s)", Required)
	PCGEX_PIN_FILTERS(PCGExRecursionTracker::SourceTrackerFilters, "Filters incoming data, if any.", Normal)

	if (Mode != EPCGExRecursionTrackerMode::Create)
	{
		if (bDoAdditionalDataTesting)
		{
			PCGEX_PIN_ANY(PCGExRecursionTracker::SourceTestData, "Collections on that will be tested using the filters below. If no filter is provided, only fail on empty data.", Normal)
			PCGEX_PIN_FILTERS(PCGExPointFilter::SourceFiltersLabel, "Collection filters used on the collections above.", Normal)
		}
	}
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRecursionTrackerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (Mode != EPCGExRecursionTrackerMode::Create)
	{
		PCGEX_PIN_PARAMS(PCGPinConstants::DefaultOutputLabel, "Updated tracker(s). Each input has been decremented once", Normal)
		if (bOutputProgress) { PCGEX_PIN_PARAMS(PCGExRecursionTracker::OutputProgressLabel, "Progress float", Normal) }
	}
	else
	{
		PCGEX_PIN_PARAMS(PCGPinConstants::DefaultOutputLabel, "Tracker(s)", Required)
	}

	return PinProperties;
}

FPCGElementPtr UPCGExRecursionTrackerSettings::CreateElement() const { return MakeShared<FPCGExRecursionTrackerElement>(); }

#pragma endregion

bool FPCGExRecursionTrackerElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(RecursionTracker)

	if (!PCGEx::IsWritableAttributeName(Settings->ContinueAttributeName))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid user-defined attribute name for ContinueAttributeName"));
		return true;
	}

	FPCGAttributeIdentifier ContinueAttribute = FPCGAttributeIdentifier(Settings->ContinueAttributeName, PCGMetadataDomainID::Default);

	const FString TAG_MAX_COUNT_STR = TEXT("PCGEx/MaxCount");
	const FString TAG_REMAINDER_STR = TEXT("PCGEx/Remainder");

	TSet<FString> RemoveTags;
	RemoveTags.Append(PCGExHelpers::GetStringArrayFromCommaSeparatedList(Settings->RemoveTags));
	TArray<FString> AddTags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(Settings->AddTags);

	int32 SafeMax = Settings->Count;
	int32 RemainderOffset = 0;
	EPCGExRecursionTrackerMode SafeMode = Settings->Mode;

	TArray<FPCGTaggedData> TaggedData = Context->InputData.GetParamsByPin(PCGPinConstants::DefaultInputLabel);
	TSharedPtr<PCGExData::FPointIOCollection> TrackersCollection = MakeShared<PCGExData::FPointIOCollection>(Context, TaggedData, PCGExData::EIOInit::NoInit, true);
	TSharedPtr<PCGExPointFilter::FManager> CollectionFilters = nullptr;

	TArray<TSharedPtr<PCGExData::FPointIO>> ValidInputs;
	ValidInputs.Reserve(TrackersCollection->Num());

	if (!TrackersCollection->IsEmpty())
	{
		// Initialize collection filters if we have some inputs
		TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> FilterFactories;

		if (PCGExFactories::GetInputFactories(
			Context, PCGExRecursionTracker::SourceTrackerFilters, FilterFactories,
			PCGExFactories::PointFilters, false))
		{
			PCGEX_MAKE_SHARED(DummyFacade, PCGExData::FFacade, TrackersCollection->Pairs[0].ToSharedRef())
			CollectionFilters = MakeShared<PCGExPointFilter::FManager>(DummyFacade.ToSharedRef());
			CollectionFilters->bWillBeUsedWithCollections = true;
			if (!CollectionFilters->Init(Context, FilterFactories)) { CollectionFilters = nullptr; }
		}

		for (const TSharedPtr<PCGExData::FPointIO>& IO : TrackersCollection->Pairs)
		{
			if (CollectionFilters && !CollectionFilters->Test(IO, TrackersCollection)) { continue; }
			ValidInputs.Add(IO);
		}

		if (SafeMode == EPCGExRecursionTrackerMode::CreateOrUpdate)
		{
			SafeMode = EPCGExRecursionTrackerMode::Update;
		}
	}
	else if (SafeMode == EPCGExRecursionTrackerMode::CreateOrUpdate)
	{
		// Create or Update is provided no input
		// Offset safe max (assume we create one from scratch)
		RemainderOffset = Settings->RemainderOffsetWhenCreateInsteadOfUpdate;
		SafeMode = EPCGExRecursionTrackerMode::Create;
	}

	TrackersCollection.Reset();

	SafeMax = FMath::Max(0, SafeMax);

	auto StageResult = [&](bool bResult)
	{
		UPCGParamData* NewParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);

		TSharedPtr<PCGExData::FTags> Tags = MakeShared<PCGExData::FTags>();
		Tags->Append(AddTags);

		Tags->Set<int32>(TAG_MAX_COUNT_STR, SafeMax);
		Tags->Set<int32>(TAG_REMAINDER_STR, SafeMax + RemainderOffset);

		UPCGMetadata* Metadata = NewParamData->MutableMetadata();
		Metadata->CreateAttribute<bool>(ContinueAttribute, bResult, true, true);
		Metadata->AddEntry();

		Context->StageOutput(NewParamData, PCGPinConstants::DefaultOutputLabel, Tags->Flatten(), false, true, false);
	};

	if (SafeMode == EPCGExRecursionTrackerMode::Create)
	{
		if (ValidInputs.IsEmpty())
		{
			StageResult(true);
		}
		else
		{
			for (const TSharedPtr<PCGExData::FPointIO>& IO : ValidInputs)
			{
				const UPCGParamData* OriginalParamData = Cast<UPCGParamData>(IO->InitializationData);
				if (!OriginalParamData) { continue; }

				UPCGParamData* NewParamData = OriginalParamData->DuplicateData(Context);

				IO->Tags->Append(AddTags);
				IO->Tags->Remove(RemoveTags);
				IO->Tags->Set<int32>(TAG_MAX_COUNT_STR, SafeMax);
				IO->Tags->Set<int32>(TAG_REMAINDER_STR, SafeMax);

				UPCGMetadata* Metadata = NewParamData->MutableMetadata();
				Metadata->DeleteAttribute(ContinueAttribute);
				Metadata->CreateAttribute<bool>(ContinueAttribute, true, true, true);

				if (Settings->bAddEntryWhenCreatingFromExistingData) { Metadata->AddEntry(); }

				Context->StageOutput(NewParamData, PCGPinConstants::DefaultOutputLabel, IO->Tags->Flatten(), false, true, false);
			}
		}
	}
	else
	{
		FPCGAttributeIdentifier ProgressAttribute = FPCGAttributeIdentifier(FName("Progress"), PCGMetadataDomainID::Default);

		if (ValidInputs.IsEmpty())
		{
			// No trackers, empty list.
			if (!Settings->bOutputNothingOnStop) { StageResult(false); }
		}
		else
		{
			int32 NumTrackers = 0;
			bool bShouldStop = false;

			if (Settings->bDoAdditionalDataTesting)
			{
				TSharedPtr<PCGExData::FPointIOCollection> TestDataCollection = MakeShared<PCGExData::FPointIOCollection>(
					Context, PCGExRecursionTracker::SourceTestData, PCGExData::EIOInit::NoInit, true);

				if (TestDataCollection->IsEmpty())
				{
					bShouldStop = true;
				}
				else
				{
					TSharedPtr<PCGExPointFilter::FManager> TestDataFilters = nullptr;
					TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> TestFilterFactories;

					if (!bShouldStop && PCGExFactories::GetInputFactories(
						Context, PCGExRecursionTracker::SourceTrackerFilters, TestFilterFactories,
						PCGExFactories::PointFilters, false))
					{
						PCGEX_MAKE_SHARED(DummyFacade, PCGExData::FFacade, TestDataCollection->Pairs[0].ToSharedRef())
						TestDataFilters = MakeShared<PCGExPointFilter::FManager>(DummyFacade.ToSharedRef());
						TestDataFilters->bWillBeUsedWithCollections = true;

						if (TestDataFilters->Init(Context, TestFilterFactories))
						{
							bShouldStop = true;

							for (const TSharedPtr<PCGExData::FPointIO>& IO : TestDataCollection->Pairs)
							{
								if (TestDataFilters->Test(IO, TestDataCollection))
								{
									// At least one valid test data.
									bShouldStop = false;
									break;
								}
							}
						}
					}
				}
			}

			if (bShouldStop && Settings->bOutputNothingOnStop)
			{
				Context->Done();
				return Context->TryComplete();
			}

			for (const TSharedPtr<PCGExData::FPointIO>& Data : ValidInputs)
			{
				const UPCGParamData* OriginalParamData = Cast<UPCGParamData>(Data->InitializationData);
				if (!OriginalParamData) { continue; }

				TSharedPtr<PCGExData::IDataValue> MaxCountTag = Data->Tags->GetValue(TAG_MAX_COUNT_STR);
				if (!MaxCountTag) { continue; }

				UPCGParamData* OutputParamData = const_cast<UPCGParamData*>(OriginalParamData);
				TSharedPtr<PCGExData::IDataValue> RemainderTag = Data->Tags->GetValue(TAG_REMAINDER_STR);

				NumTrackers++;

				int32 MaxCount = FMath::RoundToInt32(MaxCountTag->AsDouble());
				const int32 ClampedRemainder = FMath::Clamp(RemainderTag ? FMath::RoundToInt32(RemainderTag->AsDouble()) : MaxCount, 0, MaxCount);
				const int32 Remainder = ClampedRemainder + Settings->CounterUpdate;
				const float Progress = static_cast<float>(Remainder) / static_cast<float>(MaxCount);

				if (bShouldStop || Remainder < 0 || Settings->bForceOutputContinue)
				{
					if (!Settings->bOutputNothingOnStop)
					{
						OutputParamData = nullptr;
					}
					else
					{
						OutputParamData = OriginalParamData->DuplicateData(Context);
						UPCGMetadata* Metadata = OutputParamData->MutableMetadata();

						Metadata->DeleteAttribute(ContinueAttribute);
						Metadata->CreateAttribute<bool>(ContinueAttribute, Remainder >= 0, true, true);
					}
				}

				if (!OutputParamData) { continue; }

				Data->Tags->Remove(RemoveTags);
				Data->Tags->Append(AddTags);
				Data->Tags->Set<int32>(TAG_MAX_COUNT_STR, MaxCount);
				Data->Tags->Set<int32>(TAG_REMAINDER_STR, Remainder);

				Context->StageOutput(OutputParamData, PCGPinConstants::DefaultOutputLabel, Data->Tags->Flatten(), false, false, false);

				if (Settings->bOutputProgress)
				{
					UPCGParamData* ProgressData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
					UPCGMetadata* Metadata = ProgressData->MutableMetadata();

					Metadata->CreateAttribute<float>(ProgressAttribute, Settings->bOneMinus ? 1 - Progress : Progress, true, true);
					Metadata->AddEntry();

					Context->StageOutput(ProgressData, PCGExRecursionTracker::OutputProgressLabel, Data->Tags->Flatten(), false, true, false);
				}
			}

			if (NumTrackers == 0 && !Settings->bOutputNothingOnStop) { StageResult(false); }
		}
	}

	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
