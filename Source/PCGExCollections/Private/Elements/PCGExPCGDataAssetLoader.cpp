// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExPCGDataAssetLoader.h"

#include "PCGDataAsset.h"
#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Collections/PCGExPCGDataAssetCollection.h"
#include "Data/PCGExDataTags.h"
#include "Data/Utils/PCGExDataForward.h"
#include "Fitting/PCGExFittingTasks.h"

#define LOCTEXT_NAMESPACE "PCGExPCGDataAssetLoaderElement"
#define PCGEX_NAMESPACE PCGDataAssetLoader

#pragma region FPCGExPCGDataAssetHelper

FPCGExPCGDataAssetHelper::FPCGExPCGDataAssetHelper(int32 NumPoints)
{
	PointEntries.SetNum(NumPoints);
	FMemory::Memzero(PointEntries.GetData(), NumPoints * sizeof(const FPCGExPCGDataAssetCollectionEntry*));
}

FPCGExPCGDataAssetHelper::~FPCGExPCGDataAssetHelper()
{
	PCGExHelpers::SafeReleaseHandle(LoadHandle);
}

void FPCGExPCGDataAssetHelper::Add(int32 PointIndex, const FPCGExPCGDataAssetCollectionEntry* Entry)
{
	if (!Entry || Entry->bIsSubCollection)
	{
		return;
	}

	// Store entry for this point (array access is thread-safe if indices don't overlap in parallel)
	PointEntries[PointIndex] = Entry;

	// Register unique entry (thread-safe via lock)
	{
		FWriteScopeLock WriteLock(EntriesLock);
		if (!UniqueEntries.Contains(Entry))
		{
			UniqueEntries.Add(Entry, Entry->Staging.Path);
		}
	}
}

void FPCGExPCGDataAssetHelper::LoadAssets(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, FOnLoadEnd&& OnLoadEnd)
{
	if (UniqueEntries.IsEmpty())
	{
		OnLoadEnd(false);
		return;
	}

	// Collect all paths to load
	TSharedPtr<TSet<FSoftObjectPath>> PathsToLoad = MakeShared<TSet<FSoftObjectPath>>();
	for (const auto& Pair : UniqueEntries) { if (Pair.Value.IsValid()) { PathsToLoad->Add(Pair.Value); } }

	PCGExHelpers::Load(
		TaskManager,
		[PathsToLoad]() { return PathsToLoad->Array(); },
		[PCGEX_ASYNC_THIS_CAPTURE, OnLoadEnd](const bool bSuccess, TSharedPtr<FStreamableHandle> StreamableHandle)
		{
			PCGEX_ASYNC_THIS
			This->LoadHandle = StreamableHandle;

			if (bSuccess)
			{
				// Map loaded assets back to entries
				for (const auto& Pair : This->UniqueEntries)
				{
					if (Pair.Value.IsValid())
					{
						TSoftObjectPtr<UPCGDataAsset> SoftPtr(Pair.Value);
						if (UPCGDataAsset* LoadedAsset = SoftPtr.Get())
						{
							This->LoadedAssets.Add(Pair.Key, LoadedAsset);
						}
					}
				}
			}

			OnLoadEnd(bSuccess);
		});
}

UPCGDataAsset* FPCGExPCGDataAssetHelper::GetAssetForPoint(int32 PointIndex) const
{
	const FPCGExPCGDataAssetCollectionEntry* Entry = PointEntries[PointIndex];
	if (!Entry)
	{
		return nullptr;
	}

	const TObjectPtr<UPCGDataAsset>* Found = LoadedAssets.Find(Entry);
	return Found ? Found->Get() : nullptr;
}

const FPCGExPCGDataAssetCollectionEntry* FPCGExPCGDataAssetHelper::GetEntryForPoint(int32 PointIndex) const
{
	return PointEntries[PointIndex];
}

bool FPCGExPCGDataAssetHelper::HasValidEntry(int32 PointIndex) const
{
	return PointEntries[PointIndex] != nullptr;
}

void FPCGExPCGDataAssetHelper::GetUniqueAssets(TArray<TPair<const FPCGExPCGDataAssetCollectionEntry*, UPCGDataAsset*>>& OutAssets) const
{
	OutAssets.Reserve(LoadedAssets.Num());
	for (const auto& Pair : LoadedAssets)
	{
		OutAssets.Emplace(Pair.Key, Pair.Value.Get());
	}
}

#pragma endregion

#pragma region UPCGSettings

TArray<FPCGPinProperties> UPCGExPCGDataAssetLoaderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAM(PCGExPCGDataAssetLoader::SourceStagingMap, "Collection map information from staging nodes.", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPCGDataAssetLoaderSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGPinConstants::DefaultOutputLabel, "Spawned point data from PCGDataAssets.", Normal)
	PCGEX_PIN_ANY(FName("Others"), "Any non-point data.", Normal)
	PCGEX_PIN_POINTS(FName("BaseData"), "Base instances - one per unique point data.", Advanced)
	return PinProperties;
}
#pragma endregion

PCGEX_INITIALIZE_ELEMENT(PCGDataAssetLoader)

void FPCGExPCGDataAssetLoaderContext::RegisterNonPointData(const FPCGTaggedData& InTaggedData)
{
	if (!InTaggedData.Data) { return; }

	uint32 DUID = InTaggedData.Data->GetUniqueID();

	{
		FReadScopeLock ReadScopeLock(NonPointDataLock);
		if (UniqueNonPointData.Contains(DUID)) { return; }
	}

	{
		FReadScopeLock WriteScopeLock(NonPointDataLock);

		bool bIsAlreadySet = false;
		UniqueNonPointData.Add(DUID, &bIsAlreadySet);

		if (bIsAlreadySet) { return; }

		FPCGTaggedData& Data = NonPointData.Add_GetRef(InTaggedData);
		Data.Tags.Add(FString::Printf(TEXT("Pin:%s"), *Data.Pin.ToString()));
		Data.Pin = FName("Others");
	}
}

PCGEX_ELEMENT_BATCH_POINT_IMPL(PCGDataAssetLoader)

bool FPCGExPCGDataAssetLoaderElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PCGDataAssetLoader)

	// Setup collection unpacker - this loads the collections (blocking during boot is OK)
	Context->CollectionUnpacker = MakeShared<PCGExCollections::FPickUnpacker>();
	Context->CollectionUnpacker->UnpackPin(InContext, PCGExPCGDataAssetLoader::SourceStagingMap);

	if (!Context->CollectionUnpacker->HasValidMapping())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Could not rebuild a valid asset mapping from the provided map."));
		return false;
	}

	// Setup output collections
	Context->BaseDataCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->BaseDataCollection->OutputPin = FName("BaseData");

	Context->SpawnedCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->SpawnedCollection->OutputPin = PCGPinConstants::DefaultOutputLabel;

	return true;
}

bool FPCGExPCGDataAssetLoaderElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPCGDataAssetLoaderElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PCGDataAssetLoader)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	// Stage outputs
	//Context->MainPoints->StageOutputs();
	Context->BaseDataCollection->StageOutputs();
	Context->SpawnedCollection->StageOutputs();

	if (!Context->NonPointData.IsEmpty()) { Context->OutputData.TaggedData.Append(Context->NonPointData); }
	else { Context->OutputData.InactiveOutputPinBitmask |= 1ULL << (1); }

	return Context->TryComplete();
}

namespace PCGExPCGDataAssetLoader
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPCGDataAssetLoader::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::NoInit)

		TransformDetails = Settings->TransformDetails;
		if (!TransformDetails.Init(Context, PointDataFacade))
		{
			return false;
		}

		// Get hash attribute
		EntryHashGetter = PointDataFacade->GetReadable<int64>(PCGExCollections::Labels::Tag_EntryIdx, PCGExData::EIOSide::In, true);
		if (!EntryHashGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Missing staging hash attribute. Make sure points were staged with Collection Map output."));
			return false;
		}

		// Setup forward handler if needed
		if (Settings->TargetsForwarding.bEnabled)
		{
			ForwardHandler = Settings->TargetsForwarding.GetHandler(PointDataFacade);
		}

		// Initialize helper for tracking entries - no loading happens here
		AssetHelper = MakeShared<FPCGExPCGDataAssetHelper>(PointDataFacade->GetNum());

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PCGDataAssetLoader::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		// Just collect entries - no loading here (parallel safe)
		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }

			const int64 Hash = EntryHashGetter->Read(Index);
			if (Hash == 0 || Hash == -1) { continue; }

			int16 SecondaryIndex = 0;
			FPCGExEntryAccessResult Result = Context->CollectionUnpacker->ResolveEntry(Hash, SecondaryIndex);

			if (!Result.IsValid()) { continue; }

			// Check if this is a PCGDataAsset entry
			if (!Result.Entry->IsType(PCGExAssetCollection::TypeIds::PCGDataAsset)) { continue; }

			const FPCGExPCGDataAssetCollectionEntry* PCGDataEntry = static_cast<const FPCGExPCGDataAssetCollectionEntry*>(Result.Entry);

			// Register entry for this point (thread-safe)
			AssetHelper->Add(Index, PCGDataEntry);
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PCGDataAssetLoader::OnPointsProcessingComplete);


		// Load all unique PCGDataAssets (blocking, but after parallel phase)
		AssetHelper->LoadAssets(
			TaskManager,
			[PCGEX_ASYNC_THIS_CAPTURE](const bool bSuccess)
			{
				PCGEX_ASYNC_THIS
				This->OnAssetLoadComplete(bSuccess);
			});
	}

	void FProcessor::OnAssetLoadComplete(const bool bSuccess)
	{
		// Add base instances for unique assets
		TSet<UPCGDataAsset*> AddedToBase;
		TArray<TPair<const FPCGExPCGDataAssetCollectionEntry*, UPCGDataAsset*>> UniqueAssets;
		AssetHelper->GetUniqueAssets(UniqueAssets);

		for (const auto& Pair : UniqueAssets)
		{
			UPCGDataAsset* DataAsset = Pair.Value;
			if (!DataAsset) { continue; }

			bool bAlreadyInSet = false;
			AddedToBase.Add(DataAsset, &bAlreadyInSet);
			if (bAlreadyInSet) { continue; }

			// Add original data to base collection
			for (const FPCGTaggedData& TaggedData : DataAsset->Data.GetAllInputs())
			{
				if (const UPCGBasePointData* BasePointData = Cast<UPCGBasePointData>(TaggedData.Data))
				{
					TSharedPtr<PCGExData::FPointIO> BaseIO = Context->BaseDataCollection->Emplace_GetRef(BasePointData, PCGExData::EIOInit::Forward);
					if (BaseIO) { BaseIO->Tags->Append(TaggedData.Tags); }
				}
			}
		}

		// Spawn for each point
		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();
		const int32 NumPoints = PointDataFacade->GetNum();

		for (int32 Index = 0; Index < NumPoints; Index++)
		{
			if (!PointFilterCache[Index]) { continue; }

			if (!AssetHelper->HasValidEntry(Index)) { continue; }

			UPCGDataAsset* DataAsset = AssetHelper->GetAssetForPoint(Index);
			if (!DataAsset) { continue; }

			// Spawn data from asset, applying tag filtering
			for (const FPCGTaggedData& TaggedData : DataAsset->Data.GetAllInputs())
			{
				// Apply tag filtering
				if (Settings->bFilterByTags)
				{
					// Check exclude tags
					bool bExcluded = false;
					for (const FString& ExcludeTag : Settings->ExcludeTags)
					{
						if (TaggedData.Tags.Contains(ExcludeTag))
						{
							bExcluded = true;
							break;
						}
					}

					if (bExcluded) { continue; }

					// Check include tags (if specified)
					if (!Settings->IncludeTags.IsEmpty())
					{
						bool bIncluded = false;
						for (const FString& IncludeTag : Settings->IncludeTags)
						{
							if (TaggedData.Tags.Contains(IncludeTag))
							{
								bIncluded = true;
								break;
							}
						}

						if (!bIncluded) { continue; }
					}
				}

				const UPCGBasePointData* BasePointData = Cast<UPCGBasePointData>(TaggedData.Data);
				if (!BasePointData)
				{
					Context->RegisterNonPointData(TaggedData);
					continue;
				}

				// Create duplicate for spawning
				TSharedPtr<PCGExData::FPointIO> SpawnedIO = Context->SpawnedCollection->Emplace_GetRef(BasePointData, PCGExData::EIOInit::Duplicate);
				if (!SpawnedIO) { continue; }

				SpawnedIO->IOIndex = Index;

				// Forward tags
				if (Settings->bForwardInputTags) { SpawnedIO->Tags->Append(PointDataFacade->Source->Tags.ToSharedRef()); }
				SpawnedIO->Tags->Append(TaggedData.Tags);
				SpawnedIO->Tags->Set<FString>(TEXT("Pin"), TaggedData.Pin.ToString());

				// Forward attributes if configured
				if (ForwardHandler) { ForwardHandler->Forward(Index, SpawnedIO->GetOut()->Metadata); }

				// Transform the spawned data
				PCGEX_LAUNCH(PCGExFitting::Tasks::FTransformPointIO, Index, PointDataFacade->Source, SpawnedIO, &TransformDetails)
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
