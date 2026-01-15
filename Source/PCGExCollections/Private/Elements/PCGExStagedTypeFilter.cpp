// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExStagedTypeFilter.h"

#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExStagedTypeFilterElement"
#define PCGEX_NAMESPACE StagedTypeFilter

#pragma region FPCGExStagedTypeFilterConfig

#pragma endregion

#pragma region UPCGSettings

#if WITH_EDITOR
void UPCGExStagedTypeFilterSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	TypeConfig.PostEditChangeProperty(PropertyChangedEvent);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExStagedTypeFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAM(PCGExStagedTypeFilter::SourceStagingMap, "Collection map information from staging nodes.", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExStagedTypeFilterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();

	if (bOutputFilteredOut)
	{
		PCGEX_PIN_POINTS(PCGExStagedTypeFilter::OutputFilteredOut, "Points that were filtered out.", Normal)
	}

	return PinProperties;
}
#pragma endregion

PCGEX_INITIALIZE_ELEMENT(StagedTypeFilter)
PCGEX_ELEMENT_BATCH_POINT_IMPL(StagedTypeFilter)

bool FPCGExStagedTypeFilterElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(StagedTypeFilter)

	// Setup collection unpacker
	Context->CollectionUnpacker = MakeShared<PCGExCollections::FPickUnpacker>();
	Context->CollectionUnpacker->UnpackPin(InContext, PCGExStagedTypeFilter::SourceStagingMap);

	if (!Context->CollectionUnpacker->HasValidMapping())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Could not rebuild a valid asset mapping from the provided map."));
		return false;
	}

	// Setup filtered out collection if needed
	if (Settings->bOutputFilteredOut)
	{
		Context->FilteredOutCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->FilteredOutCollection->OutputPin = PCGExStagedTypeFilter::OutputFilteredOut;
	}

	return true;
}

bool FPCGExStagedTypeFilterElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExStagedTypeFilterElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(StagedTypeFilter)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	if (Context->FilteredOutCollection)
	{
		Context->FilteredOutCollection->StageOutputs();
	}

	return Context->TryComplete();
}

namespace PCGExStagedTypeFilter
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExStagedTypeFilter::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Get hash attribute
		EntryHashGetter = PointDataFacade->GetReadable<int64>(PCGExCollections::Labels::Tag_EntryIdx, PCGExData::EIOSide::In, true);
		if (!EntryHashGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Missing staging hash attribute. Make sure points were staged with Collection Map output."));
			return false;
		}

		// Initialize mask
		Mask.Init(1, PointDataFacade->GetNum());

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::StagedTypeFilter::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		const bool bIncludeMode = Settings->FilterMode == EPCGExStagedTypeFilterMode::Include;
		int32 LocalKept = 0;

		PCGEX_SCOPE_LOOP(Index)
		{
			const uint64 Hash = EntryHashGetter->Read(Index);

			PCGExAssetCollection::FTypeId TypeId = PCGExAssetCollection::TypeIds::None;

			if (Hash != 0 && Hash != static_cast<uint64>(-1))
			{
				int16 SecondaryIndex = 0;
				FPCGExEntryAccessResult Result = Context->CollectionUnpacker->ResolveEntry(Hash, SecondaryIndex);

				if (Result.IsValid())
				{
					TypeId = Result.Entry->GetTypeId();
				}
			}

			const bool bMatchesConfig = Settings->TypeConfig.Matches(TypeId);

			// In Include mode: keep if matches. In Exclude mode: keep if doesn't match.
			const bool bKeep = bIncludeMode ? bMatchesConfig : !bMatchesConfig;

			if (bKeep)
			{
				Mask[Index] = 1;
				LocalKept++;
			}
			else
			{
				Mask[Index] = 0;
			}
		}

		FPlatformAtomics::InterlockedAdd(&NumKept, LocalKept);
	}

	void FProcessor::CompleteWork()
	{
		const int32 NumPoints = PointDataFacade->GetNum();
		const int32 NumFiltered = NumPoints - NumKept;

		if (NumFiltered == 0)
		{
			// All points kept, nothing to do
			return;
		}

		if (NumKept == 0)
		{
			// All points filtered out
			if (Settings->bOutputFilteredOut && Context->FilteredOutCollection)
			{
				// Move entire dataset to filtered out
				TSharedPtr<PCGExData::FPointIO> FilteredIO = Context->FilteredOutCollection->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::Forward);
			}

			// Clear main output
			PointDataFacade->Source->Disable();
			return;
		}

		// Output filtered out points if requested
		if (Settings->bOutputFilteredOut && Context->FilteredOutCollection)
		{
			// Create inverted mask for filtered out points
			TArray<int8> InvertedMask;
			InvertedMask.SetNum(NumPoints);
			for (int32 i = 0; i < NumPoints; i++) { InvertedMask[i] = Mask[i] ? 0 : 1; }

			TSharedPtr<PCGExData::FPointIO> FilteredIO = Context->FilteredOutCollection->Emplace_GetRef(PointDataFacade->GetIn(), PCGExData::EIOInit::Duplicate);
			if (FilteredIO) { (void)FilteredIO->Gather(InvertedMask); }
		}

		// Gather kept points
		(void)PointDataFacade->Source->Gather(Mask);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
