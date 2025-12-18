// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCollectionHelpers.h"
#include "PCGExAssetCollection.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExActorCollection.generated.h"

namespace PCGExAssetCollection
{
	enum class ELoadingFlags : uint8;
}

class UPCGExActorCollection;

namespace PCGExActorCollection
{
	void GetBoundingBoxBySpawning(const TSoftClassPtr<AActor>& InActorClass, FVector& Origin, FVector& BoxExtent, const bool bOnlyCollidingComponents = true, const bool bIncludeFromChildActors = true);
}

USTRUCT(BlueprintType, DisplayName="[PCGEx] Actor Collection Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExActorCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExActorCollectionEntry() = default;

	virtual PCGExAssetCollection::EType GetType() const override { return FPCGExAssetCollectionEntry::GetType() | PCGExAssetCollection::EType::Actor; }

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	TSoftClassPtr<AActor> Actor;

	/** If enabled, the cached bounds will only account for collicable components on the actor. */
	UPROPERTY(EditAnywhere, Category = "Settings|Bounds", meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	bool bOnlyCollidingComponents = false;

	/** If enabled, the cached bounds will also account for child actors. */
	UPROPERTY(EditAnywhere, Category = "Settings|Bounds", meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	bool bIncludeFromChildActors = true;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides, DisplayAfter="bIsSubCollection"))
	TObjectPtr<UPCGExActorCollection> SubCollection;

	virtual void ClearSubCollection() override;

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const override;

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, const bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

	virtual UPCGExAssetCollection* GetSubCollectionVoid() const override;

#if WITH_EDITOR
	virtual void EDITOR_Sanitize() override;
#endif
};

UCLASS(BlueprintType, DisplayName="[PCGEx] Actor Collection")
class PCGEXTENDEDTOOLKIT_API UPCGExActorCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExActorCollectionEntry;

public:
	virtual PCGExAssetCollection::EType GetType() const override { return PCGExAssetCollection::EType::Actor; }

#if WITH_EDITOR
	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPCGExActorCollectionEntry> Entries;

	PCGEX_ASSET_COLLECTION_BOILERPLATE(UPCGExActorCollection, FPCGExActorCollectionEntry)
};
