// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExAssetCollection.h"
#include "GameFramework/Actor.h"
#include "Helpers/PCGExArrayHelpers.h"

#include "PCGExActorCollection.generated.h"

class UPCGExActorCollection;

// =====================================================================================
// Actor Collection Entry
// =====================================================================================

USTRUCT(BlueprintType, DisplayName="[PCGEx] Actor Collection Entry")
struct PCGEXCOLLECTIONS_API FPCGExActorCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExActorCollectionEntry() = default;

	// Type System

	virtual PCGExAssetCollection::FTypeId GetTypeId() const override { return PCGExAssetCollection::TypeIds::Actor; }

	// Actor-Specific Properties
	
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	TSoftClassPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides, DisplayAfter="bIsSubCollection"))
	TObjectPtr<UPCGExActorCollection> SubCollection;

	virtual const UPCGExAssetCollection* GetSubCollectionPtr() const override;

	virtual void ClearSubCollection() override;

	// Lifecycle
	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

#if WITH_EDITOR
	virtual void EDITOR_Sanitize() override;
#endif
};

// Actor Collection
UCLASS(BlueprintType, DisplayName="[PCGEx] Actor Collection")
class PCGEXCOLLECTIONS_API UPCGExActorCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()
	PCGEX_ASSET_COLLECTION_BODY(FPCGExActorCollectionEntry)

public:
	friend struct FPCGExActorCollectionEntry;

	// Type System
	virtual PCGExAssetCollection::FTypeId GetTypeId() const override
	{
		return PCGExAssetCollection::TypeIds::Actor;
	}

	// Entries Array
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPCGExActorCollectionEntry> Entries;

#if WITH_EDITOR
	// Editor Functions
	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData) override;
#endif
};
