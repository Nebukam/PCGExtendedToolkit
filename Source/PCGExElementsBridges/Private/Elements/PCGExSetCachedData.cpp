// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExSetCachedData.h"

#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGGraphExecutionStateInterface.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h" // PCGPointDataConstants

#include "GameFramework/Actor.h"

#include "PCGExCoreSettingsCache.h"

#define LOCTEXT_NAMESPACE "PCGExSetCachedData"
#define PCGEX_NAMESPACE SetCachedData

namespace PCGExSetCachedData
{
	// Every data pin this node consumes, in declaration order: the default In pin, then the custom pins.
	void GatherInputLabels(const UPCGExSetCachedDataSettings* Settings, TArray<FName>& OutLabels)
	{
		OutLabels.Add(PCGPinConstants::DefaultInputLabel);
		for (const FPCGPinProperties& Pin : Settings->GetSanitizedCustomInputPins()) { OutLabels.Add(Pin.Label); }
	}

	// The output label a given input label passes through to.
	FName PassThroughLabel(const FName InputLabel)
	{
		return InputLabel == PCGPinConstants::DefaultInputLabel ? PCGPinConstants::DefaultOutputLabel : InputLabel;
	}
}

#pragma region UPCGExSetCachedDataSettings

UPCGExSetCachedDataSettings::UPCGExSetCachedDataSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActorReferenceAttribute.SetAttributeName(PCGPointDataConstants::ActorReferenceAttribute);
}

#if WITH_EDITOR
FLinearColor UPCGExSetCachedDataSettings::GetNodeTitleColor() const
{
	return PCGEX_NODE_COLOR_OPTIN_NAME(Action);
}

bool UPCGExSetCachedDataSettings::IsTargetPinUnconnected() const
{
	const UPCGNode* Node = Cast<UPCGNode>(GetOuter());
	return !Node || !Node->IsInputPinConnected(PCGExDataCache::TargetActorPinLabel);
}
#endif

FString UPCGExSetCachedDataSettings::GetAdditionalTitleInformation() const
{
	if (Mode == EPCGExDataCacheWriteMode::ClearAll) { return TEXT("Clear All"); }
	return CacheID.IsNone() ? FString() : CacheID.ToString();
}

TArray<FPCGPinProperties> UPCGExSetCachedDataSettings::GetSanitizedCustomInputPins() const
{
	TArray<FPCGPinProperties> Pins;
	TSet<FName> Seen = {PCGPinConstants::DefaultInputLabel, PCGExDataCache::TargetActorPinLabel};

	for (const FPCGPinProperties& Pin : CustomInputPins)
	{
		if (Pin.Label.IsNone() || Seen.Contains(Pin.Label)) { continue; }
		Seen.Add(Pin.Label);
		Pins.Add(Pin);
	}

	return Pins;
}

TArray<FPCGPinProperties> UPCGExSetCachedDataSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGPinConstants::DefaultInputLabel, "Data to cache. Stored under the In label; read it back from Get Cached Data's Out pin.", Normal)
	PinProperties.Append(GetSanitizedCustomInputPins());
	PCGEX_PIN_ANY(PCGExDataCache::TargetActorPinLabel, "Actor references naming the actor(s) that host the cache. When connected, overrides the Target setting.", Advanced)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSetCachedDataSettings::OutputPinProperties() const
{
	// Pass-through: every data input pin has a same-labelled output (In -> Out).
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(PCGPinConstants::DefaultOutputLabel, "The In data, forwarded.", Normal)
	for (const FPCGPinProperties& Pin : GetSanitizedCustomInputPins())
	{
		FPCGPinProperties& OutPin = PinProperties.Add_GetRef(Pin);
		OutPin.PinStatus = EPCGPinStatus::Normal;
	}
	return PinProperties;
}

FPCGElementPtr UPCGExSetCachedDataSettings::CreateElement() const
{
	return MakeShared<FPCGExSetCachedDataElement>();
}

#pragma endregion

#pragma region FPCGExSetCachedDataElement

bool FPCGExSetCachedDataElement::Boot(FPCGExContext* InContext) const
{
	if (!IPCGExElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SetCachedData)

	TArray<AActor*> Actors;
	PCGExDataCache::ResolveTargetActors(Context, Settings->Target, Settings->ActorReferenceAttribute, Actors);

	if (Actors.IsEmpty())
	{
		if (!Settings->bQuietMissingTargetWarning)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoTargetActor", "No target actor could be resolved; nothing was cached."));
		}
	}

	Context->TargetActors.Reserve(Actors.Num());
	for (AActor* Actor : Actors) { Context->TargetActors.Add(Actor); }

	return true;
}

bool FPCGExSetCachedDataElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(SetCachedData)
	check(IsInGameThread());

	TArray<FName> InputLabels;
	PCGExSetCachedData::GatherInputLabels(Settings, InputLabels);

	const bool bWrites = Settings->Mode == EPCGExDataCacheWriteMode::Replace || Settings->Mode == EPCGExDataCacheWriteMode::Append;
	const bool bNeedsID = Settings->Mode != EPCGExDataCacheWriteMode::ClearAll;

	if (bNeedsID && Settings->CacheID.IsNone())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidCacheID", "Cache ID is None; nothing was cached."));
	}
	else
	{
		IPCGGraphExecutionSource* Source = Context->ExecutionSource.Get();
		const bool bPreview = Source && Source->GetExecutionState().IsInPreviewMode();
		const UObject* Writer = Cast<UObject>(Source);

		for (const TWeakObjectPtr<AActor>& WeakActor : Context->TargetActors)
		{
			AActor* Actor = WeakActor.Get();
			if (!IsValid(Actor)) { continue; }

			// Removal modes never create a component just to find nothing in it.
			UPCGExDataCacheComponent* Cache = bWrites ? UPCGExDataCacheComponent::FindOrCreate(Actor) : UPCGExDataCacheComponent::Find(Actor);
			if (!Cache) { continue; }

			if (!bWrites)
			{
				Cache->Write(Settings->CacheID, Settings->Mode, {}, Writer, bPreview);
				continue;
			}

			// Each target adopts its own private copies: a data object has exactly one outer.
			TArray<FPCGTaggedData> Duplicates;
			for (const FName& Label : InputLabels)
			{
				for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(Label))
				{
					if (!Input.Data) { continue; }

					UPCGData* Duplicate = Input.Data->DuplicateData(Context);
					if (!Duplicate)
					{
						PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("DuplicateFailed", "Failed to duplicate '{0}'; it will be missing from the cache."), FText::FromString(Input.Data->GetName())));
						continue;
					}

					FPCGTaggedData& Copy = Duplicates.Emplace_GetRef();
					Copy.Data = Duplicate;
					Copy.Tags = Input.Tags;
					Copy.Pin = Label;
				}
			}

			Cache->Write(Settings->CacheID, Settings->Mode, MoveTemp(Duplicates), Writer, bPreview);
		}
	}

	// Pass-through, so the node can sit inline.
	for (const FName& Label : InputLabels)
	{
		const FName OutLabel = PCGExSetCachedData::PassThroughLabel(Label);
		for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(Label))
		{
			if (!Input.Data) { continue; }
			Context->StageOutput(const_cast<UPCGData*>(Input.Data.Get()), OutLabel, PCGExData::EStaging::None, Input.Tags);
		}
	}

	Context->Done();
	return Context->TryComplete();
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
