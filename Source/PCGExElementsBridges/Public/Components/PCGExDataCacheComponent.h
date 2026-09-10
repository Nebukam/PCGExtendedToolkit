// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGData.h"
#include "Components/ActorComponent.h"
#include "Misc/TransactionallySafeRWLock.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExDataCacheComponent.generated.h"

UENUM()
enum class EPCGExDataCacheWriteMode : uint8
{
	Replace  = 0 UMETA(DisplayName = "Replace", Tooltip = "Replace whatever is stored under the cache ID with the input data (an empty input stores an empty entry)."),
	Append   = 1 UMETA(DisplayName = "Append", Tooltip = "Append the input data to whatever is already stored under the cache ID."),
	Remove   = 2 UMETA(DisplayName = "Remove", Tooltip = "Remove the entry stored under the cache ID. Inputs are only passed through."),
	ClearAll = 3 UMETA(DisplayName = "Clear All", Tooltip = "Remove every entry on the target cache. Cache ID is ignored; inputs are only passed through."),
};

/** One cached collection. Data objects are outered to the owning cache component so they serialize with the actor. */
USTRUCT()
struct PCGEXELEMENTSBRIDGES_API FPCGExDataCacheEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Data Cache")
	FPCGDataCollection Data;

	/** Execution source (usually a PCG component) that last wrote this entry. */
	UPROPERTY(VisibleAnywhere, Category = "Data Cache")
	FSoftObjectPath Writer;

	/** Incremented on every write to this ID. */
	UPROPERTY(VisibleAnywhere, Category = "Data Cache")
	int32 Revision = 0;
};

/**
 * Holds PCG data collections under named IDs, persisted with the owning actor the same way a PCG component
 * persists its generated output. One per actor (see Find / FindOrCreate). Never a PCG managed resource:
 * cleanup and regeneration leave it alone, so a refresh can read back what a previous pass stored.
 *
 * Reads are safe from any thread. Writes are game-thread only: adopting data re-outers it (Rename) and
 * flattens it (Modify), neither of which is thread-safe.
 */
UCLASS(ClassGroup = (Procedural), meta = (BlueprintSpawnableComponent, DisplayName = "PCGEx Data Cache"))
class PCGEXELEMENTSBRIDGES_API UPCGExDataCacheComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPCGExDataCacheComponent();

	/** The actor's cache component, or null. */
	static UPCGExDataCacheComponent* Find(const AActor* InActor);

	/** The actor's cache component, created as an instance component if missing. Game thread only. */
	static UPCGExDataCacheComponent* FindOrCreate(AActor* InActor);

	/** Copies the entry stored under InId (preview entries shadow persisted ones). Any thread. */
	bool Read(const FName InId, FPCGDataCollection& OutData) const;

	/** Copies every entry, preview entries shadowing persisted ones with the same ID. Any thread. */
	void ReadAll(TArray<TPair<FName, FPCGDataCollection>>& OutEntries) const;

	bool Contains(const FName InId) const;
	int32 NumEntries() const;

	/**
	 * Adopts InData under InId. Game thread only. Every data object must be a private duplicate outered to the
	 * transient package: the whole data network is flattened and re-outered to this component. bPreview routes
	 * the write to the transient preview map (data flagged RF_Transient, package never dirtied).
	 */
	void Write(const FName InId, const EPCGExDataCacheWriteMode InMode, TArray<FPCGTaggedData>&& InData, const UObject* InWriter, const bool bPreview);

	/** Drops the entry under InId from both maps. Game thread only. */
	void Remove(const FName InId);

	/** Drops every entry from both maps. Game thread only. */
	void ClearAll();

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Data Cache", meta = (DisplayName = "Clear Cache", ShortToolTip = "Remove every cached entry from this component."))
	void EDITOR_ClearCache();
#endif

protected:
	/** Persisted entries. */
	UPROPERTY(VisibleAnywhere, Category = "Data Cache")
	TMap<FName, FPCGExDataCacheEntry> Entries;

	/** Entries written by preview-mode generations. Never saved; shadow persisted entries on read. */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Data Cache")
	TMap<FName, FPCGExDataCacheEntry> PreviewEntries;

	/** Guards both maps. Held only around map access, never around Rename/Flatten. */
	mutable FTransactionallySafeRWLock Lock;

	/** Re-outers every data object this component owns in InData back to the transient package so GC can reclaim it. */
	void ReleaseData(const FPCGDataCollection& InData) const;

	/** Flattens and re-outers InData's networks to this component. Returns the data that could be adopted. */
	TArray<FPCGTaggedData> AdoptData(TArray<FPCGTaggedData>&& InData, const bool bPreview) const;
};
