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

TArray<FPCGPinProperties> UPCGExRecursionTrackerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	if (Mode != EPCGExRecursionTrackerMode::Create)
	{
		PCGEX_PIN_PARAMS(PCGPinConstants::DefaultInputLabel, "Tracker(s)", Required)

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
	const FString TAG_COUNT_STR = TEXT("PCGEx/Remainder");

	TArray<FString> AddTags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(Settings->AddTags);

	TSharedPtr<PCGExData::FPointIOCollection> TrackersCollection = nullptr;
	int32 SafeMax = Settings->Count;
	
	auto InitTrackersCollection = [&]()
	{
		if (!TrackersCollection)
		{
			TArray<FPCGTaggedData> TaggedData = Context->InputData.GetParamsByPin(PCGPinConstants::DefaultInputLabel);
			TrackersCollection = MakeShared<PCGExData::FPointIOCollection>(Context, TaggedData, PCGExData::EIOInit::NoInit, true);
		}
	};

	EPCGExRecursionTrackerMode SafeMode = Settings->Mode;

	if (SafeMode == EPCGExRecursionTrackerMode::CreateOrMutate)
	{
		InitTrackersCollection();
		if (TrackersCollection->IsEmpty())
		{
			SafeMax += Settings->OffsetOnCreateFromEmpty;
			SafeMode = EPCGExRecursionTrackerMode::Create;
		}
		else { SafeMode = EPCGExRecursionTrackerMode::Mutate; }
	}

	SafeMax = FMath::Max(0, SafeMax);
	
	auto StageResult = [&](bool bResult)
	{
		UPCGParamData* NewParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);

		TSharedPtr<PCGExData::FTags> Tags = MakeShared<PCGExData::FTags>();
		Tags->Append(AddTags);
		Tags->Set<int32>(TAG_MAX_COUNT_STR, SafeMax);

		UPCGMetadata* Metadata = NewParamData->MutableMetadata();
		Metadata->CreateAttribute<bool>(ContinueAttribute, bResult, true, true);
		Metadata->AddEntry();

		Context->StageOutput(NewParamData, PCGPinConstants::DefaultOutputLabel, Tags->Flatten(), false, true, false);
	};

	if (SafeMode == EPCGExRecursionTrackerMode::Create)
	{
		StageResult(true);
	}
	else
	{
		FPCGAttributeIdentifier ProgressAttribute = FPCGAttributeIdentifier(FName("Progress"), PCGMetadataDomainID::Default);

		auto StageProgress = [&](float Progress, const TSharedPtr<PCGExData::FTags>& Tags)
		{
			UPCGParamData* NewParamData = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
			UPCGMetadata* Metadata = NewParamData->MutableMetadata();
			Metadata->CreateAttribute<float>(ProgressAttribute, Settings->bOneMinus ? 1 - Progress : Progress, true, true);
			Metadata->AddEntry();
			Context->StageOutput(NewParamData, PCGExRecursionTracker::OutputProgressLabel, Tags->Flatten(), false, true, false);
		};

		InitTrackersCollection();

		if (TrackersCollection->IsEmpty())
		{
			if (!Settings->bEmptyOutputOnStop) { StageResult(false); }
		}
		else
		{
			int32 NumTrackers = 0;
			bool bForceFail = false;
			if (Settings->bDoAdditionalDataTesting)
			{
				TSharedPtr<PCGExData::FPointIOCollection> TestDataCollection = MakeShared<PCGExData::FPointIOCollection>(Context, PCGExRecursionTracker::SourceTestData, PCGExData::EIOInit::NoInit, true);
				if (TestDataCollection->IsEmpty())
				{
					bForceFail = true;
				}
				else
				{
					TSharedPtr<PCGExPointFilter::FManager> CollectionFilters = nullptr;
					TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> FilterFactories;

					if (!bForceFail && PCGExFactories::GetInputFactories(
						Context, PCGExRecursionTracker::SourceTrackerFilters, FilterFactories,
						PCGExFactories::PointFilters, false))
					{
						PCGEX_MAKE_SHARED(DummyFacade, PCGExData::FFacade, TrackersCollection->Pairs[0].ToSharedRef())
						CollectionFilters = MakeShared<PCGExPointFilter::FManager>(DummyFacade.ToSharedRef());
						CollectionFilters->bWillBeUsedWithCollections = true;
						if (CollectionFilters->Init(Context, FilterFactories))
						{
							bForceFail = true;
							for (const TSharedPtr<PCGExData::FPointIO>& IO : TestDataCollection->Pairs)
							{
								if (CollectionFilters->Test(IO, TestDataCollection))
								{
									bForceFail = false;
									break;
								}
							}
						}
					}
				}
			}

			if (bForceFail && Settings->bEmptyOutputOnStop)
			{
				Context->Done();
				return Context->TryComplete();
			}

			TSet<FString> RemoveTags;
			RemoveTags.Append(PCGExHelpers::GetStringArrayFromCommaSeparatedList(Settings->RemoveTags));

			TSharedPtr<PCGExPointFilter::FManager> TrackerFilters = nullptr;
			TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> TrackerFilterFactories;

			if (!bForceFail && PCGExFactories::GetInputFactories(
				Context, PCGExRecursionTracker::SourceTrackerFilters, TrackerFilterFactories,
				PCGExFactories::PointFilters, false))
			{
				PCGEX_MAKE_SHARED(DummyFacade, PCGExData::FFacade, TrackersCollection->Pairs[0].ToSharedRef())
				TrackerFilters = MakeShared<PCGExPointFilter::FManager>(DummyFacade.ToSharedRef());
				TrackerFilters->bWillBeUsedWithCollections = true;
				if (!TrackerFilters->Init(Context, TrackerFilterFactories)) { TrackerFilters = nullptr; }
			}

			for (const TSharedPtr<PCGExData::FPointIO>& Data : TrackersCollection->Pairs)
			{
				if (TrackerFilters && !TrackerFilters->Test(Data, TrackersCollection)) { continue; }

				const UPCGParamData* OriginalParamData = Cast<UPCGParamData>(Data->InitializationData);
				if (!OriginalParamData) { continue; }

				TSharedPtr<PCGExData::IDataValue> MaxCountTag = Data->Tags->GetValue(TAG_MAX_COUNT_STR);
				if (!MaxCountTag) { continue; }

				UPCGParamData* OutputParamData = const_cast<UPCGParamData*>(OriginalParamData);
				TSharedPtr<PCGExData::IDataValue> CountTag = Data->Tags->GetValue(TAG_COUNT_STR);

				NumTrackers++;

				int32 MaxCount = FMath::RoundToInt32(MaxCountTag->AsDouble());
				const int32 OriginalCount = FMath::Clamp(CountTag ? FMath::RoundToInt32(CountTag->AsDouble()) : MaxCount, 0, MaxCount);
				const int32 Count = OriginalCount - 1;

				if (bForceFail || Count < 0 || Settings->bForceOutputContinue)
				{
					if (!Settings->bEmptyOutputOnStop)
					{
						OutputParamData = OriginalParamData->DuplicateData(Context);
						UPCGMetadata* Metadata = OutputParamData->MutableMetadata();
						Metadata->DeleteAttribute(ContinueAttribute);
						Metadata->CreateAttribute<bool>(ContinueAttribute, Count >= 0, true, true);
					}
					else
					{
						OutputParamData = nullptr;
					}
				}

				if (OutputParamData)
				{
					Data->Tags->Remove(RemoveTags);
					Data->Tags->Append(AddTags);

					Data->Tags->Set<int32>(TAG_COUNT_STR, Count);
					Data->Tags->Set<int32>(TAG_MAX_COUNT_STR, MaxCount);

					Context->StageOutput(OutputParamData, PCGPinConstants::DefaultOutputLabel, Data->Tags->Flatten(), false, false, false);
				}

				if (Settings->bOutputProgress) { StageProgress(static_cast<float>(Count) / static_cast<float>(MaxCount), Data->Tags); }
			}

			if (NumTrackers == 0 && !Settings->bEmptyOutputOnStop)
			{
				StageResult(false);
			}
		}
	}

	TrackersCollection.Reset();
	
	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
