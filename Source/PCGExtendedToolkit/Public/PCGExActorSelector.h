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
	AllWorldActors
};

struct PCGEXTENDEDTOOLKIT_API FPCGExActorSelectionKey
{
	FPCGExActorSelectionKey() = default;

	// For all filters others than AllWorldActor. For AllWorldActors Filter, use the other constructors.
	explicit FPCGExActorSelectionKey(EPCGExActorFilter InFilter);

	explicit FPCGExActorSelectionKey(FName InTag);
	explicit FPCGExActorSelectionKey(TSubclassOf<AActor> InSelectionClass);

	bool operator==(const FPCGExActorSelectionKey& InOther) const;

	friend uint32 GetTypeHash(const FPCGExActorSelectionKey& In);
	bool IsMatching(const AActor* InActor, const UPCGComponent* InComponent) const;

	void SetExtraDependency(const UClass* InExtraDependency);

	EPCGExActorFilter ActorFilter = EPCGExActorFilter::AllWorldActors;
	EPCGExActorSelection Selection = EPCGExActorSelection::Unknown;
	FName Tag = NAME_None;
	TSubclassOf<AActor> ActorSelectionClass = nullptr;

	// If it should track a specific object dependency instead of an actor. For example, GetActorData with GetPCGComponent data.
	const UClass* OptionalExtraDependency = nullptr;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExActorSelectorSettings
{
	GENERATED_BODY()

	/** Which actors to consider. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bShowActorFilter", EditConditionHides, HideEditConditionToggle))
	EPCGExActorFilter ActorFilter = EPCGExActorFilter::Self;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ActorFilter==EPCGExActorFilter::AllWorldActors", EditConditionHides))
	bool bMustOverlapSelf = false;

	/** Whether to consider child actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bShowIncludeChildren && ActorFilter!=EPCGExActorFilter::AllWorldActors", EditConditionHides))
	bool bIncludeChildren = false;

	/** Enables/disables fine-grained actor filtering options. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ActorFilter!=EPCGExActorFilter::AllWorldActors && bIncludeChildren", EditConditionHides))
	bool bDisableFilter = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bShowActorSelection && (ActorFilter==EPCGExActorFilter::AllWorldActors || (bIncludeChildren && !bDisableFilter))", EditConditionHides))
	EPCGExActorSelection ActorSelection = EPCGExActorSelection::ByTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bShowActorSelection && (ActorFilter==EPCGExActorFilter::AllWorldActors || (bIncludeChildren && !bDisableFilter)) && ActorSelection==EPCGExActorSelection::ByTag", EditConditionHides))
	FName ActorSelectionTag;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bShowActorSelectionClass && bShowActorSelection && (ActorFilter==EPCGExActorFilter::AllWorldActors || (bIncludeChildren && !bDisableFilter)) && ActorSelection==EPCGExActorSelection::ByClass", EditConditionHides, AllowAbstract = "true"))
	TSubclassOf<AActor> ActorSelectionClass;

	/** If true processes all matching actors, otherwise returns data from first match. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bShowSelectMultiple && ActorFilter==EPCGExActorFilter::AllWorldActors && ActorSelection!=EPCGExActorSelection::ByName", EditConditionHides))
	bool bSelectMultiple = false;

	/** If true, ignores results found from within this actor's hierarchy */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ActorFilter==EPCGExActorFilter::AllWorldActors", EditConditionHides))
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

#if WITH_EDITOR
	FText GetTaskNameSuffix() const;
	FName GetTaskName(const FText& Prefix) const;
#endif

	FPCGExActorSelectionKey GetAssociatedKey() const;
	static FPCGExActorSelectorSettings ReconstructFromKey(const FPCGExActorSelectionKey& InKey);
};

namespace PCGExActorSelector
{
	TArray<AActor*> FindActors(const FPCGExActorSelectorSettings& Settings, const UPCGComponent* InComponent, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck);
	AActor* FindActor(const FPCGExActorSelectorSettings& InSettings, const UPCGComponent* InComponent, const TFunction<bool(const AActor*)>& BoundsCheck, const TFunction<bool(const AActor*)>& SelfIgnoreCheck);
}
