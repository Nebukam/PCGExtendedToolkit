﻿// Copyright 2025 Timothé Lapetite and contributors
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

TArray<FPCGPreConfiguredSettingsInfo> UPCGExRecursionTrackerSettings::GetPreconfiguredInfo() const
{
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGExRecursionTrackerType>({}, FTEXT("Recursion Tracker : {0}"));
}
#endif

bool UPCGExRecursionTrackerSettings::HasDynamicPins() const
{
	return Type == EPCGExRecursionTrackerType::Branch;
}

void UPCGExRecursionTrackerSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExRecursionTrackerType>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			Type = static_cast<EPCGExRecursionTrackerType>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

EPCGDataType UPCGExRecursionTrackerSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	if (!InPin->IsOutputPin()
		|| InPin->Properties.Label == PCGPinConstants::DefaultInputLabel
		|| InPin->Properties.Label == PCGExRecursionTracker::OutputContinueLabel
		|| InPin->Properties.Label == PCGExRecursionTracker::OutputStopLabel)
	{
		return Super::GetCurrentPinTypes(InPin);
	}

	return EPCGDataType::Param;
}

TArray<FPCGPinProperties> UPCGExRecursionTrackerSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (Type == EPCGExRecursionTrackerType::Branch) { PCGEX_PIN_ANY(PCGPinConstants::DefaultInputLabel, "Data to branch out", Required) }

	PCGEX_PIN_PARAMS(PCGExRecursionTracker::SourceTrackerLabel, "Tracker(s)", Required)
	PCGEX_PIN_FILTERS(PCGExRecursionTracker::SourceTrackerFilters, "Filters incoming data, if any.", Advanced)

	if (Type == EPCGExRecursionTrackerType::Simple
		&& Mode != EPCGExRecursionTrackerMode::Create
		&& bDoAdditionalDataTesting)
	{
		PCGEX_PIN_ANY(PCGExRecursionTracker::SourceTestData, "Collections on that will be tested using the filters below. If no filter is provided, only fail on empty data.", Normal)
		PCGEX_PIN_FILTERS(PCGExPointFilter::SourceFiltersLabel, "Collection filters used on the collections above.", Normal)
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExRecursionTrackerSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (Type == EPCGExRecursionTrackerType::Branch)
	{
		PCGEX_PIN_ANY(PCGExRecursionTracker::OutputContinueLabel, "Input data will be redirected to this pin if any tracker can continue.", Normal)
		if (bGroupBranchPins) { PCGEX_PIN_ANY(PCGExRecursionTracker::OutputStopLabel, "Input data will be redirected to this pin if no tracker can continue.", Normal) }
	}

	PCGEX_PIN_PARAMS(PCGExRecursionTracker::OutputTrackerLabel, "New or updated tracker(s)", Required)

	if (!bGroupBranchPins && Type == EPCGExRecursionTrackerType::Branch)
	{
		PCGEX_PIN_ANY(PCGExRecursionTracker::OutputStopLabel, "Input data will be redirected to this pin if no tracker can continue.", Normal)
	}

	if (Mode != EPCGExRecursionTrackerMode::Create)
	{
#define PCGEX_OUTPUT_EXTRA(_NAME) if (bOutput##_NAME){ PCGEX_PIN_PARAMS(PCGExRecursionTracker::Output##_NAME##Label, "See toggle tooltip.", Normal)}

		PCGEX_OUTPUT_EXTRA(Progress)
		PCGEX_OUTPUT_EXTRA(Index)
		PCGEX_OUTPUT_EXTRA(Remainder)

#undef PCGEX_OUTPUT_EXTRA
	}

	return PinProperties;
}

FPCGElementPtr UPCGExRecursionTrackerSettings::CreateElement() const { return MakeShared<FPCGExRecursionTrackerElement>(); }

#pragma endregion

bool FPCGExRecursionTrackerElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(RecursionTracker)

	const bool bDoAdditionalDataTesting = Settings->Type == EPCGExRecursionTrackerType::Simple ? Settings->bDoAdditionalDataTesting : false;

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

	int32 SafeMax = Settings->MaxCount;
	int32 RemainderOffset = 0;
	EPCGExRecursionTrackerMode SafeMode = Settings->Mode;

	TArray<FPCGTaggedData> TaggedData = Context->InputData.GetParamsByPin(PCGExRecursionTracker::SourceTrackerLabel);
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

	auto Branch = [&](bool bContinue)
	{
		if (Settings->Type != EPCGExRecursionTrackerType::Branch) { return; }

		TArray<FPCGTaggedData> InOutData = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		if (bContinue)
		{
			for (FPCGTaggedData& Data : InOutData) { Data.Pin = PCGExRecursionTracker::OutputContinueLabel; }
			Context->OutputData.InactiveOutputPinBitmask |= Settings->bGroupBranchPins ? 1ULL << 1 : 1ULL << 2;
		}
		else
		{
			for (FPCGTaggedData& Data : InOutData) { Data.Pin = PCGExRecursionTracker::OutputStopLabel; }
			Context->OutputData.InactiveOutputPinBitmask |= 1ULL << 0;
		}

		Context->OutputData.TaggedData.Append(InOutData);
	};

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

		Context->StageOutput(NewParamData, PCGExRecursionTracker::OutputTrackerLabel, Tags->Flatten(), false, true, false);
	};

	if (SafeMode == EPCGExRecursionTrackerMode::Create)
	{
		if (ValidInputs.IsEmpty())
		{
			Branch(true);
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

				Context->StageOutput(NewParamData, PCGExRecursionTracker::OutputTrackerLabel, IO->Tags->Flatten(), false, true, false);
			}
		}
	}
	else
	{
		if (ValidInputs.IsEmpty())
		{
			Branch(false);
		}
		else
		{
			int32 NumTrackers = 0;
			bool bShouldStop = false;

			if (bDoAdditionalDataTesting)
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

			bool bAnyContinue = false;

			for (const TSharedPtr<PCGExData::FPointIO>& Input : ValidInputs)
			{
				const UPCGParamData* OriginalParamData = Cast<UPCGParamData>(Input->InitializationData);
				if (!OriginalParamData) { continue; }

				TSharedPtr<PCGExData::IDataValue> MaxCountTag = Input->Tags->GetValue(TAG_MAX_COUNT_STR);
				if (!MaxCountTag) { continue; }

				UPCGParamData* OutputParamData = const_cast<UPCGParamData*>(OriginalParamData);
				TSharedPtr<PCGExData::IDataValue> RemainderTag = Input->Tags->GetValue(TAG_REMAINDER_STR);

				NumTrackers++;

				int32 MaxCount = FMath::RoundToInt32(MaxCountTag->AsDouble());
				const int32 ClampedRemainder = FMath::Clamp(RemainderTag ? FMath::RoundToInt32(RemainderTag->AsDouble()) : MaxCount, 0, MaxCount);
				const int32 Remainder = ClampedRemainder + Settings->CounterUpdate;
				const float Progress = static_cast<float>(Remainder) / static_cast<float>(MaxCount);

				const bool bContinue = Remainder >= 0;
				if (bContinue) { bAnyContinue = true; }

				if (bShouldStop || !bContinue || Settings->bForceOutputContinue)
				{
					OutputParamData = OriginalParamData->DuplicateData(Context);
					UPCGMetadata* Metadata = OutputParamData->MutableMetadata();

					Metadata->DeleteAttribute(ContinueAttribute);
					Metadata->CreateAttribute<bool>(ContinueAttribute, bContinue, true, true);
				}

				Input->Tags->Remove(RemoveTags);
				Input->Tags->Append(AddTags);
				Input->Tags->Set<int32>(TAG_MAX_COUNT_STR, MaxCount);
				Input->Tags->Set<int32>(TAG_REMAINDER_STR, Remainder);
				const TSet<FString> FlattenedTags = Input->Tags->Flatten();

				Context->StageOutput(OutputParamData, PCGExRecursionTracker::OutputTrackerLabel, FlattenedTags, false, false, false);

#define PCGEX_OUTPUT_EXTRA(_NAME, _TYPE, _VALUE)\
				if (Settings->bOutput##_NAME){\
					UPCGParamData* Extra = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);\
					UPCGMetadata* Metadata = Extra->MutableMetadata();\
					Metadata->CreateAttribute<_TYPE>(FPCGAttributeIdentifier(PCGExRecursionTracker::Output##_NAME##Label, PCGMetadataDomainID::Default), _VALUE, true, true);\
					Metadata->AddEntry();\
					Context->StageOutput(Extra, PCGExRecursionTracker::Output##_NAME##Label, FlattenedTags, false, true, false);}

				PCGEX_OUTPUT_EXTRA(Progress, float, Settings->bOneMinus ? 1 - Progress : Progress)
				PCGEX_OUTPUT_EXTRA(Index, int32, FMath::Clamp(MaxCount - ClampedRemainder, 0, MaxCount))
				PCGEX_OUTPUT_EXTRA(Remainder, int32, Remainder)

#undef PCGEX_OUTPUT_EXTRA
			}

			if (NumTrackers == 0) { StageResult(false); }
			Branch(bAnyContinue);
		}
	}

	Context->Done();
	return Context->TryComplete();
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
