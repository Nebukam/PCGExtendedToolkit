// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "UObject/NameTypes.h"

#include "PCGExActorSelector.generated.h"

class AActor;
class UPCGComponent;
class UWorld;

UENUM()
enum class EPCGExActorSelection : uint8
{
	ByTag,
	// Deprecated - actor labels are unavailable in shipping builds
	ByName UMETA(Hidden),
	ByClass,
	ByPath UMETA(Hidden),
	// Hidden because actors are not tracked by paths.
	Unknown UMETA(Hidden)
};

UENUM()
enum class EPCGExActorFilter : uint8
{
	/** This actor (either the original PCG actor or the partition actor if partitioning is enabled). */
	Self,
	/** The parent of this actor in the hierarchy. */
	Parent,
	/** The top most parent of this actor in the hierarchy. */
	Root,
	/** All actors in world. */
	AllWorldActors,
	/** The source PCG actor (rather than the generated partition actor). */
	Original,
};

/**
* Structure to specify a selection criteria for an object/actor
* Object can be selected using the EPCGExActorSelection::ByClass or EPCGExActorSelection::ByPath
* Actors have more options for selection with Self/Parent/Root/Original and also EPCGExActorSelection::ByTag
*/
USTRUCT()
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSelectionKey
{
	GENERATED_BODY()

	FPCGExSelectionKey() = default;

	// For all filters others than AllWorldActor. For AllWorldActors Filter, use the other constructors.
	explicit FPCGExSelectionKey(EPCGExActorFilter InFilter);

	explicit FPCGExSelectionKey(FName InTag);
	explicit FPCGExSelectionKey(TSubclassOf<UObject> InSelectionClass);

	static FPCGExSelectionKey CreateFromPath(const FSoftObjectPath& InObjectPath);
	static FPCGExSelectionKey CreateFromPath(FSoftObjectPath&& InObjectPath);

	bool operator==(const FPCGExSelectionKey& InOther) const;

	friend uint32 GetTypeHash(const FPCGExSelectionKey& In);
	bool IsMatching(const UObject* InObject, const UPCGComponent* InComponent) const;
	bool IsMatching(const UObject* InObject, const TSet<FName>& InRemovedTags, const TSet<UPCGComponent*>& InComponents, TSet<UPCGComponent*>* OptionalMatchedComponents = nullptr) const;

	void SetExtraDependency(const UClass* InExtraDependency);

	UPROPERTY()
	EPCGExActorFilter ActorFilter = EPCGExActorFilter::AllWorldActors;

	UPROPERTY()
	EPCGExActorSelection Selection = EPCGExActorSelection::Unknown;

	UPROPERTY()
	FName Tag = NAME_None;

	UPROPERTY()
	TSubclassOf<UObject> SelectionClass = nullptr;

	// If the Selection is ByPath, contain the path to select.
	UPROPERTY()
	FSoftObjectPath ObjectPath;

	// If it should track a specific object dependency instead of an actor. For example, GetActorData with GetPCGComponent data.
	UPROPERTY()
	TObjectPtr<const UClass> OptionalExtraDependency = nullptr;
};

/*PCGEXTENDEDTOOLKIT_API*/ FArchive& operator<<(FArchive& Ar, FPCGExSelectionKey& Key);

/** Helper struct for organizing queries against the world to gather actors. */
USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExActorSelectorSettings
{
	GENERATED_BODY()

	/** Which actors to consider. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "bShowActorFilter", EditConditionHides, HideEditConditionToggle))
	EPCGExActorFilter ActorFilter = EPCGExActorFilter::Self;

	/** Filters out actors that do not overlap the source component bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "ActorFilter==EPCGExActorFilter::AllWorldActors", EditConditionHides))
	bool bMustOverlapSelf = false;

	/** Whether to consider child actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "bShowIncludeChildren && ActorFilter!=EPCGExActorFilter::AllWorldActors", EditConditionHides))
	bool bIncludeChildren = false;

	/** Enables/disables fine-grained actor filtering options. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "ActorFilter!=EPCGExActorFilter::AllWorldActors && bIncludeChildren", EditConditionHides))
	bool bDisableFilter = false;

	/** How to select when filtering actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "bShowActorSelection && (ActorFilter==EPCGExActorFilter::AllWorldActors || (bIncludeChildren && !bDisableFilter))", EditConditionHides))
	EPCGExActorSelection ActorSelection = EPCGExActorSelection::ByTag;

	/** Tag to match against when filtering actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "bShowActorSelection && (ActorFilter==EPCGExActorFilter::AllWorldActors || (bIncludeChildren && !bDisableFilter)) && ActorSelection==EPCGExActorSelection::ByTag", EditConditionHides))
	FName ActorSelectionTag;

	/** Actor class to match against when filtering actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "bShowActorSelectionClass && bShowActorSelection && (ActorFilter==EPCGExActorFilter::AllWorldActors || (bIncludeChildren && !bDisableFilter)) && ActorSelection==EPCGExActorSelection::ByClass", EditConditionHides, AllowAbstract = "true"))
	TSubclassOf<AActor> ActorSelectionClass;

	/** If true processes all matching actors, otherwise returns data from first match. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "bShowSelectMultiple && ActorFilter==EPCGExActorFilter::AllWorldActors && ActorSelection!=EPCGExActorSelection::ByName", EditConditionHides))
	bool bSelectMultiple = false;

	/** If true, ignores results found from within this actor's hierarchy. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor Selector Settings", meta = (EditCondition = "bShowIgnoreSelfAndChildren && ActorFilter==EPCGExActorFilter::AllWorldActors", EditConditionHides))
	bool bIgnoreSelfAndChildren = false;

	// Properties used to hide some fields when used in different contexts
	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bShowActorFilter = true;

	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bShowIncludeChildren = true;

	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bShowActorSelection = true;

	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bShowActorSelectionClass = true;

	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bShowSelectMultiple = true;

	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bShowIgnoreSelfAndChildren = true;

#if WITH_EDITOR
	FText GetTaskNameSuffix() const;
	FName GetTaskName(const FText& Prefix) const;
#endif

	FPCGExSelectionKey GetAssociatedKey() const;
	static FPCGExActorSelectorSettings ReconstructFromKey(const FPCGExSelectionKey& InKey);
};

namespace PCGExActorSelector
{
	/*PCGEXTENDEDTOOLKIT_API*/ TArray<AActor*> FindActors(const FPCGExActorSelectorSettings& Settings, const UPCGComponent* InComponent, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck);
	/*PCGEXTENDEDTOOLKIT_API*/ AActor* FindActor(const FPCGExActorSelectorSettings& InSettings, const UPCGComponent* InComponent, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck);
}
