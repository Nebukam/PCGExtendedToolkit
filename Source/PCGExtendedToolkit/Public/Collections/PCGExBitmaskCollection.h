// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "Details/PCGExDetailsBitmask.h"

#include "PCGExBitmaskCollection.generated.h"

class UPCGExBitmaskCollection;

USTRUCT(BlueprintType, DisplayName="[PCGEx] Bitmask Staging Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmaskCache
{
	GENERATED_BODY()

	UPROPERTY()
	FName Identifier = NAME_None;

	UPROPERTY()
	int64 Bitmask;

	UPROPERTY()
	FVector Direction;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Bitmask Collection Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExBitmaskCollectionEntry
{
	GENERATED_BODY()
	virtual ~FPCGExBitmaskCollectionEntry() = default;

	FPCGExBitmaskCollectionEntry() = default;

	UPROPERTY(EditAnywhere, Category = Settings)
	FName Identifier = NAME_None;

	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExBitmask Bitmask;

	UPROPERTY(EditAnywhere, Category = Settings)
	FVector Direction;

	FORCEINLINE FVector GetDirection() const { return Direction.GetSafeNormal(); }
	FORCEINLINE void GetDirection(FVector& OutDir) const { OutDir = Direction.GetSafeNormal(); }
	
	UPROPERTY(Transient)
	FPCGExBitmaskCache CachedBitmask;

	virtual void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;
	void RebuildCache();
	
};

namespace PCGExBitmaskCollection
{
	class PCGEXTENDEDTOOLKIT_API FCache : public TSharedFromThis<FCache>
	{
	public:
		TArray<FPCGExBitmaskCache> Bitmasks;
		TMap<FName, int32> BitmaskMap;
		TArray<FString> Identifiers;

		explicit FCache() = default;
		~FCache() = default;

		bool TryGetBitmask(const FName Identifier, int64& OutBitmask) const;
		bool TryGetBitmask(const FName Identifier, FPCGExBitmaskCache& OutCachedBitmask) const;

		bool IsEmpty() const { return Bitmasks.IsEmpty(); }
	};
}

UCLASS(BlueprintType, DisplayName="[PCGEx] Bitmask Collection")
class PCGEXTENDEDTOOLKIT_API UPCGExBitmaskCollection : public UDataAsset
{
	mutable FRWLock CacheLock;

	GENERATED_BODY()

	friend struct FPCGExBitmaskCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
	PCGExBitmaskCollection::FCache* LoadCache();
	virtual void InvalidateCache();

	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;

	virtual void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const;

#if WITH_EDITOR
	bool HasCircularDependency(const UPCGExBitmaskCollection* OtherCollection) const;
	bool HasCircularDependency(TSet<const UPCGExBitmaskCollection*>& InReferences) const;
	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;	
	TArray<FName> EDITOR_GetIdentifierOptions() const;
#endif

	

#if WITH_EDITORONLY_DATA
	/** Dev notes/comments. Editor-only data.  */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayPriority=-1, MultiLine))
	FString Notes;
#endif

	virtual void BuildCache();

	virtual bool IsValidIndex(const int32 InIndex) const { return false; }
	virtual int32 NumEntries() const { return 0; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="{Identifier}   |   {Direction}"))
	TArray<FPCGExBitmaskCollectionEntry> Entries;

protected:
	UPROPERTY()
	bool bCacheNeedsRebuild = true;

	TSharedPtr<PCGExBitmaskCollection::FCache> Cache;

#if WITH_EDITOR
	void EDITOR_SetDirty()
	{
		Cache.Reset();
		bCacheNeedsRebuild = true;
		InvalidateCache();
	}
#endif
};
