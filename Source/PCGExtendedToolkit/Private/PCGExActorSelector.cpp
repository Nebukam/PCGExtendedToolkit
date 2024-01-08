// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGExActorSelector.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "Helpers/PCGActorHelpers.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGExActorSelector)

namespace PCGExActorSelector
{
	// Filter is required if it is not disabled and if we are gathering all world actors or gathering all children.
	bool FilterRequired(const FPCGExActorSelectorSettings& InSettings)
	{
		return (InSettings.ActorFilter == EPCGExActorFilter::AllWorldActors || InSettings.bIncludeChildren) && !InSettings.bDisableFilter;
	}

	// Need to pass a pointer of pointer to the found actor. The lambda will capture this pointer and modify its value when an actor is found.
	TFunction<bool(AActor*)> GetFilteringFunction(const FPCGExActorSelectorSettings& InSettings, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck, TArray<AActor*>& InFoundActors)
	{
		if (!FilterRequired(InSettings))
		{
			return [&InFoundActors, &BoundsCheck, &SelfIgnoreCheck](AActor* Actor) -> bool
			{
				if (BoundsCheck(Actor) && SelfIgnoreCheck(Actor))
				{
					InFoundActors.Add(Actor);
				}
				return true;
			};
		}

		const bool bMultiSelect = InSettings.bSelectMultiple;

		switch (InSettings.ActorSelection)
		{
		case EPCGExActorSelection::ByTag:
			return [ActorSelectionTag = InSettings.ActorSelectionTag, &InFoundActors, bMultiSelect, &BoundsCheck, &SelfIgnoreCheck](AActor* Actor) -> bool
			{
				if (Actor && Actor->ActorHasTag(ActorSelectionTag) && BoundsCheck(Actor) && SelfIgnoreCheck(Actor))
				{
					InFoundActors.Add(Actor);
					return bMultiSelect;
				}

				return true;
			};

		case EPCGExActorSelection::ByClass:
			return [ActorSelectionClass = InSettings.ActorSelectionClass, &InFoundActors, bMultiSelect, &BoundsCheck, &SelfIgnoreCheck](AActor* Actor) -> bool
			{
				if (Actor && Actor->IsA(ActorSelectionClass) && BoundsCheck(Actor) && SelfIgnoreCheck(Actor))
				{
					InFoundActors.Add(Actor);
					return bMultiSelect;
				}

				return true;
			};

		case EPCGExActorSelection::ByName:
			UE_LOG(LogPCG, Error, TEXT("PCGExActorSelector::GetFilteringFunction: Unsupported value for EPCGExActorSelection - selection by name is no longer supported."));
			break;

		default:
			break;
		}

		return [](AActor* Actor) -> bool { return false; };
	}

	TArray<AActor*> FindActors(const FPCGExActorSelectorSettings& Settings, const UPCGComponent* InComponent, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExActorSelector::FindActor);

		UWorld* World = InComponent ? InComponent->GetWorld() : nullptr;
		AActor* Self = InComponent ? InComponent->GetOwner() : nullptr;

		TArray<AActor*> FoundActors;

		if (!World)
		{
			return FoundActors;
		}

		// Early out if we have not the information necessary.
		const bool bNoTagInfo = Settings.ActorSelection == EPCGExActorSelection::ByTag && Settings.ActorSelectionTag == NAME_None;
		const bool bNoClassInfo = Settings.ActorSelection == EPCGExActorSelection::ByClass && !Settings.ActorSelectionClass;

		if (FilterRequired(Settings) && (bNoTagInfo || bNoClassInfo))
		{
			return FoundActors;
		}

		// We pass FoundActor ref, that will be captured by the FilteringFunction
		// It will modify the FoundActor pointer to the found actor, if found.
		TFunction<bool(AActor*)> FilteringFunction = GetFilteringFunction(Settings, BoundsCheck, SelfIgnoreCheck, FoundActors);

		// In case of iterating over all actors in the world, call our filtering function and get out.
		if (Settings.ActorFilter == EPCGExActorFilter::AllWorldActors)
		{
			// A potential optimization if we know the sought actors are collide-able could be to obtain overlaps via a collision query.
			UPCGActorHelpers::ForEachActorInWorld<AActor>(World, FilteringFunction);

			// FoundActor is set by the FilteringFunction (captured)
			return FoundActors;
		}

		// Otherwise, gather all the actors we need to check
		TArray<AActor*> ActorsToCheck;
		switch (Settings.ActorFilter)
		{
		case EPCGExActorFilter::Self:
			if (Self)
			{
				ActorsToCheck.Add(Self);
			}
			break;

		case EPCGExActorFilter::Parent:
			if (Self)
			{
				if (AActor* Parent = Self->GetParentActor())
				{
					ActorsToCheck.Add(Parent);
				}
				else
				{
					// If there is no parent, set the owner as the parent.
					ActorsToCheck.Add(Self);
				}
			}
			break;

		case EPCGExActorFilter::Root:
			{
				AActor* Current = Self;
				while (Current != nullptr)
				{
					AActor* Parent = Current->GetParentActor();
					if (Parent == nullptr)
					{
						ActorsToCheck.Add(Current);
						break;
					}
					Current = Parent;
				}

				break;
			}

		default:
			break;
		}

		if (Settings.bIncludeChildren)
		{
			const int32 InitialCount = ActorsToCheck.Num();
			for (int32 i = 0; i < InitialCount; ++i)
			{
				ActorsToCheck[i]->GetAttachedActors(ActorsToCheck, /*bResetArray=*/ false, /*bRecursivelyIncludeAttachedActors=*/ true);
			}
		}

		for (AActor* Actor : ActorsToCheck)
		{
			// FoundActor is set by the FilteringFunction (captured)
			if (!FilteringFunction(Actor))
			{
				break;
			}
		}

		return FoundActors;
	}

	AActor* FindActor(const FPCGExActorSelectorSettings& InSettings, const UPCGComponent* InComponent, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck)
	{
		// In order to make sure we don't try to select multiple, we'll do a copy of the settings here.
		FPCGExActorSelectorSettings Settings = InSettings;
		Settings.bSelectMultiple = false;

		TArray<AActor*> Actors = FindActors(Settings, InComponent, BoundsCheck, SelfIgnoreCheck);
		return Actors.IsEmpty() ? nullptr : Actors[0];
	}
}

FPCGExActorSelectionKey::FPCGExActorSelectionKey(const EPCGExActorFilter InFilter)
{
	check(InFilter != EPCGExActorFilter::AllWorldActors);
	ActorFilter = InFilter;
}

FPCGExActorSelectionKey::FPCGExActorSelectionKey(const FName InTag)
{
	Selection = EPCGExActorSelection::ByTag;
	Tag = InTag;
	ActorFilter = EPCGExActorFilter::AllWorldActors;
}

FPCGExActorSelectionKey::FPCGExActorSelectionKey(const TSubclassOf<AActor> InSelectionClass)
{
	Selection = EPCGExActorSelection::ByClass;
	ActorSelectionClass = InSelectionClass;
	ActorFilter = EPCGExActorFilter::AllWorldActors;
}

void FPCGExActorSelectionKey::SetExtraDependency(const UClass* InExtraDependency)
{
	OptionalExtraDependency = InExtraDependency;
}

bool FPCGExActorSelectionKey::IsMatching(const AActor* InActor, const UPCGComponent* InComponent) const
{
	if (!InActor)
	{
		return false;
	}

	// If we filter something else than all world actors, matching depends on the component.
	// Re-use the same mecanism than Get Actor Data, which should be cheap since we don't look for all actors in the world.
	if (ActorFilter != EPCGExActorFilter::AllWorldActors)
	{
		// InKey provide the info for selecting a given actor.
		// We reconstruct the selector settings from this key, and we also force it to SelectMultiple, since
		// we want to gather all the actors that matches this given key, to find if ours matches.
		FPCGExActorSelectorSettings SelectorSettings = FPCGExActorSelectorSettings::ReconstructFromKey(*this);
		SelectorSettings.bSelectMultiple = true;
		const TArray<AActor*> AllActors = PCGExActorSelector::FindActors(SelectorSettings, InComponent, [](const AActor*) { return true; }, [](const AActor*) { return true; });
		return AllActors.Contains(InActor);
	}

	switch (Selection)
	{
	case EPCGExActorSelection::ByTag:
		return InActor->ActorHasTag(Tag);
	case EPCGExActorSelection::ByClass:
		return InActor->IsA(ActorSelectionClass);
	default:
		return false;
	}
}

bool FPCGExActorSelectionKey::operator==(const FPCGExActorSelectionKey& InOther) const
{
	if (ActorFilter != InOther.ActorFilter || Selection != InOther.Selection || OptionalExtraDependency != InOther.OptionalExtraDependency)
	{
		return false;
	}

	switch (Selection)
	{
	case EPCGExActorSelection::ByTag:
		return Tag == InOther.Tag;
	case EPCGExActorSelection::ByClass:
		return ActorSelectionClass == InOther.ActorSelectionClass;
	case EPCGExActorSelection::Unknown: // Fall-through
	case EPCGExActorSelection::ByName:
		return true;
	default:
		{
			checkNoEntry();
			return true;
		}
	}
}

uint32 GetTypeHash(const FPCGExActorSelectionKey& In)
{
	uint32 HashResult = HashCombine(GetTypeHash(In.ActorFilter), GetTypeHash(In.Selection));
	HashResult = HashCombine(HashResult, GetTypeHash(In.Tag));
	HashResult = HashCombine(HashResult, GetTypeHash(In.ActorSelectionClass));
	HashResult = HashCombine(HashResult, GetTypeHash(In.OptionalExtraDependency));

	return HashResult;
}

#if WITH_EDITOR
FText FPCGExActorSelectorSettings::GetTaskNameSuffix() const
{
	if (ActorFilter == EPCGExActorFilter::AllWorldActors)
	{
		if (ActorSelection == EPCGExActorSelection::ByClass)
		{
			return (ActorSelectionClass.Get() ? ActorSelectionClass->GetDisplayNameText() : FText::FromName(NAME_None));
		}
		if (ActorSelection == EPCGExActorSelection::ByTag)
		{
			return FText::FromName(ActorSelectionTag);
		}
	}
	else if (const UEnum* EnumPtr = StaticEnum<EPCGExActorFilter>())
	{
		return EnumPtr->GetDisplayNameTextByValue(static_cast<__underlying_type(EPCGExActorFilter)>(ActorFilter));
	}

	return FText();
}

FName FPCGExActorSelectorSettings::GetTaskName(const FText& Prefix) const
{
	return FName(FText::Format(NSLOCTEXT("PCGActorSelectorSettings", "NodeTitleFormat", "{0} ({1})"), Prefix, GetTaskNameSuffix()).ToString());
}
#endif

FPCGExActorSelectionKey FPCGExActorSelectorSettings::GetAssociatedKey() const
{
	if (ActorFilter != EPCGExActorFilter::AllWorldActors)
	{
		return FPCGExActorSelectionKey(ActorFilter);
	}

	switch (ActorSelection)
	{
	case EPCGExActorSelection::ByTag:
		return FPCGExActorSelectionKey(ActorSelectionTag);
	case EPCGExActorSelection::ByClass:
		return FPCGExActorSelectionKey(ActorSelectionClass);
	default:
		return FPCGExActorSelectionKey();
	}
}

FPCGExActorSelectorSettings FPCGExActorSelectorSettings::ReconstructFromKey(const FPCGExActorSelectionKey& InKey)
{
	FPCGExActorSelectorSettings Result{};
	Result.ActorFilter = InKey.ActorFilter;
	Result.ActorSelection = InKey.Selection;
	Result.ActorSelectionTag = InKey.Tag;
	Result.ActorSelectionClass = InKey.ActorSelectionClass;

	return Result;
}
