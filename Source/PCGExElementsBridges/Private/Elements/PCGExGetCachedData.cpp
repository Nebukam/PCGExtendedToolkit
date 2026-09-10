// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExGetCachedData.h"

#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGGraphExecutionStateInterface.h"
#include "PCGNode.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h" // PCGPointDataConstants
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#include "PCGExCoreSettingsCache.h"
#include "Components/PCGExDataCacheComponent.h"

#define LOCTEXT_NAMESPACE "PCGExGetCachedData"
#define PCGEX_NAMESPACE GetCachedData

#pragma region UPCGExGetCachedDataSettings

UPCGExGetCachedDataSettings::UPCGExGetCachedDataSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActorReferenceAttribute.SetAttributeName(PCGPointDataConstants::ActorReferenceAttribute);
}

#if WITH_EDITOR
FLinearColor UPCGExGetCachedDataSettings::GetNodeTitleColor() const
{
	return PCGEX_NODE_COLOR_OPTIN_NAME(Action);
}

bool UPCGExGetCachedDataSettings::IsTargetPinUnconnected() const
{
	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	return !Node || !Node->IsInputPinConnected(PCGExDataCache::TargetActorPinLabel);
}
#endif

FString UPCGExGetCachedDataSettings::GetAdditionalTitleInformation() const
{
	if (bReadAllEntries) { return TEXT("All"); }
	return CacheID.IsNone() ? FString() : CacheID.ToString();
}

TArray<FPCGPinProperties> UPCGExGetCachedDataSettings::GetSanitizedCustomOutputPins() const
{
	TArray<FPCGPinProperties> Pins;
	TSet<FName> Seen = {PCGPinConstants::DefaultOutputLabel, PCGExDataCache::StatusPinLabel};

	for (const FPCGPinProperties& Pin : CustomOutputPins)
	{
		if (Pin.Label.IsNone() || Seen.Contains(Pin.Label)) { continue; }
		Seen.Add(Pin.Label);
		Pins.Add(Pin);
	}

	return Pins;
}

TArray<FPCGPinProperties> UPCGExGetCachedDataSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGExDataCache::TargetActorPinLabel, "Actor references naming the actor(s) whose cache to read. When connected, overrides the Target setting.", Advanced)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExGetCachedDataSettings::OutputPinProperties() const
{
	// Order is load-bearing: AdvanceWork culls by pin index (custom pins, then Out, then Status).
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Append(GetSanitizedCustomOutputPins());
	PCGEX_PIN_ANY(PCGPinConstants::DefaultOutputLabel, "Cached data whose stored pin label matches no custom output pin.", Normal)
	if (bOutputStatus)
	{
		PCGEX_PIN_PARAM(PCGExDataCache::StatusPinLabel, "One row per target actor: Found, EntryCount, ActorReference, CacheID.", Normal)
	}
	return PinProperties;
}

FPCGElementPtr UPCGExGetCachedDataSettings::CreateElement() const
{
	return MakeShared<FPCGExGetCachedDataElement>();
}

#pragma endregion

#pragma region FPCGExGetCachedDataContext

void FPCGExGetCachedDataContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	FPCGExContext::AddExtraStructReferencedObjects(Collector);
	Collector.AddReferencedObjects(ReferencedObjects);
}

#pragma endregion

#pragma region FPCGExGetCachedDataElement

bool FPCGExGetCachedDataElement::Boot(FPCGExContext* InContext) const
{
	if (!IPCGExElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(GetCachedData)
	check(IsInGameThread());

	if (!Settings->bReadAllEntries && Settings->CacheID.IsNone())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidCacheID", "Cache ID is None."));
		return false;
	}

	TArray<AActor*> Actors;
	PCGExDataCache::ResolveTargetActors(Context, Settings->Target, Settings->ActorReferenceAttribute, Actors);

	if (Actors.IsEmpty())
	{
		if (!Settings->bQuietMissingTargetWarning)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoTargetActor", "No target actor could be resolved."));
		}
		Context->StatusRows.Emplace();
		return true;
	}

#if WITH_EDITOR
	// Engine mirror (FPCGDataFromActorElement::ProcessActor): data owned by an actor outside the source's persistent
	// level is duplicated to the transient package, so the graph cache never pins that level's objects.
	const IPCGGraphExecutionSource* Source = Context->ExecutionSource.Get();
	const IPCGGraphExecutionSource* OriginalSource = Source ? Source->GetExecutionState().GetOriginalSource() : nullptr;
	const UObject* SourceOwner = OriginalSource ? OriginalSource->GetExecutionState().GetTarget() : nullptr;
	const ULevel* PersistentLevel = (SourceOwner && SourceOwner->GetWorld()) ? SourceOwner->GetWorld()->PersistentLevel : nullptr;
#endif

	for (AActor* Actor : Actors)
	{
		FPCGExGetCachedDataContext::FStatusRow& Row = Context->StatusRows.Emplace_GetRef();
		Row.Actor = FSoftObjectPath(Actor);

		const UPCGExDataCacheComponent* Cache = UPCGExDataCacheComponent::Find(Actor);
		if (!Cache)
		{
			// Normal on the first generation.
			PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("NoCacheComponent", "Actor '{0}' has no PCGEx Data Cache component."), FText::FromString(Actor->GetName())));
			continue;
		}

		TArray<TPair<FName, FPCGDataCollection>> Entries;
		if (Settings->bReadAllEntries)
		{
			Cache->ReadAll(Entries);
		}
		else
		{
			FPCGDataCollection Collection;
			if (Cache->Read(Settings->CacheID, Collection)) { Entries.Emplace(Settings->CacheID, MoveTemp(Collection)); }
		}

		if (Entries.IsEmpty())
		{
			PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("CacheMiss", "Actor '{0}' has no cached entry for '{1}'."), FText::FromString(Actor->GetName()), FText::FromName(Settings->CacheID)));
			continue;
		}

		Row.bFound = true;

		bool bMustDuplicate = false;
#if WITH_EDITOR
		bMustDuplicate = !PersistentLevel || Actor->GetLevel() != PersistentLevel;
#endif

		for (const TPair<FName, FPCGDataCollection>& Entry : Entries)
		{
			for (const FPCGTaggedData& Stored : Entry.Value.TaggedData)
			{
				if (!Stored.Data) { continue; }

				FPCGTaggedData& Read = Context->Reads.Emplace_GetRef(Stored);
				if (bMustDuplicate) { Read.Data = Cast<UPCGData>(StaticDuplicateObject(Stored.Data.Get(), GetTransientPackage())); }
				if (Settings->bTagWithCacheID) { Read.Tags.Add(PCGExDataCache::CacheIDTagPrefix + Entry.Key.ToString()); }

				Context->ReferencedObjects.Add(Read.Data.GetObjectPtr());
				Row.EntryCount++;
			}
		}
	}

	return true;
}

bool FPCGExGetCachedDataElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(GetCachedData)

	// Same order as OutputPinProperties: custom pins, then Out, then Status.
	TArray<FName> CustomLabels;
	for (const FPCGPinProperties& Pin : Settings->GetSanitizedCustomOutputPins()) { CustomLabels.Add(Pin.Label); }

	TSet<FName> ActivePins;
	Context->IncreaseStagedOutputReserve(Context->Reads.Num());

	for (const FPCGTaggedData& Read : Context->Reads)
	{
		const FName Pin = CustomLabels.Contains(Read.Pin) ? Read.Pin : PCGPinConstants::DefaultOutputLabel;
		ActivePins.Add(Pin);
		Context->StageOutput(const_cast<UPCGData*>(Read.Data.Get()), Pin, PCGExData::EStaging::None, Read.Tags);
	}

	// Cull data pins that got nothing (bit j == output pin index j). Status is never culled.
	uint64 InactiveMask = 0;
	for (int32 i = 0; i < CustomLabels.Num(); i++)
	{
		if (!ActivePins.Contains(CustomLabels[i])) { InactiveMask |= 1ull << i; }
	}
	if (!ActivePins.Contains(PCGPinConstants::DefaultOutputLabel)) { InactiveMask |= 1ull << CustomLabels.Num(); }
	Context->OutputData.InactiveOutputPinBitmask = InactiveMask;

	if (Settings->bOutputStatus)
	{
		UPCGParamData* Status = FPCGContext::NewObject_AnyThread<UPCGParamData>(Context);
		UPCGMetadata* Metadata = Status->MutableMetadata();

		FPCGMetadataAttribute<bool>* FoundAttr = Metadata->CreateAttribute<bool>(PCGExDataCache::FoundAttributeName, false, false, true);
		FPCGMetadataAttribute<int32>* CountAttr = Metadata->CreateAttribute<int32>(PCGExDataCache::EntryCountAttributeName, 0, false, true);
		FPCGMetadataAttribute<FSoftObjectPath>* ActorAttr = Metadata->CreateAttribute<FSoftObjectPath>(PCGPointDataConstants::ActorReferenceAttribute, FSoftObjectPath(), false, true);
		FPCGMetadataAttribute<FName>* IdAttr = Metadata->CreateAttribute<FName>(PCGExDataCache::CacheIDAttributeName, NAME_None, false, true);

		const FName RowId = Settings->bReadAllEntries ? NAME_None : Settings->CacheID;
		for (const FPCGExGetCachedDataContext::FStatusRow& Row : Context->StatusRows)
		{
			const PCGMetadataEntryKey Key = Metadata->AddEntry();
			FoundAttr->SetValue(Key, Row.bFound);
			CountAttr->SetValue(Key, Row.EntryCount);
			ActorAttr->SetValue(Key, Row.Actor);
			IdAttr->SetValue(Key, RowId);
		}

		Context->StageOutput(Status, PCGExDataCache::StatusPinLabel, PCGExData::EStaging::Mutable);
	}

	Context->Done();
	return Context->TryComplete();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
