// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPin.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExCoreMacros.h"
#include "Core/PCGExContext.h"
#include "Core/PCGExElement.h"
#include "Core/PCGExSettings.h"
#include "Helpers/PCGExDataCacheHelpers.h"

#include "PCGExGetCachedData.generated.h"

/**
 * Get Cached Data.
 * Reads data stored on the target actor's PCGEx Data Cache component by Set Cached Data. The cache is empty on
 * the first generation, so branch on the Status pin. Data pins with nothing to output are deactivated.
 * Cached data is handed out by pointer (never mutated); the target is resolved and read on the game thread
 * during preparation, staging happens off-thread.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category = "PCGEx|Misc", meta = (Keywords = "pcgex cache read restore previous generation data", PCGExNodeLibraryDoc = "utilities/data-cache/get-cached-data"))
class UPCGExGetCachedDataSettings : public UPCGExSettings
{
	GENERATED_BODY()

	friend class FPCGExGetCachedDataElement;

public:
	UPCGExGetCachedDataSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(GetCachedData, "Get Cached Data", "Reads data stored on the target actor's PCGEx Data Cache component by Set Cached Data. Empty on the first generation; branch on the Status pin.");

	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
	virtual FLinearColor GetNodeTitleColor() const override;
#endif

	virtual FString GetAdditionalTitleInformation() const override;

	/** Data pins with nothing on them gray out instead of vanishing, so downstream wires survive a cache miss. */
	virtual bool OutputPinsCanBeDeactivated() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** ID to read. Must match the ID used on Set Cached Data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "!bReadAllEntries"))
	FName CacheID = FName("Default");

	/** Read every entry on the cache instead of a single ID. Enable Tag With Cache ID to tell them apart. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bReadAllEntries = false;

	/** Which actor hosts the cache. Ignored when the Target Actor pin is connected. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "IsTargetPinUnconnected()"))
	EPCGExDataCacheTarget Target = EPCGExDataCacheTarget::ExecutingActor;

	/** Attribute holding the actor reference on the Target Actor pin. A component reference resolves to its owner. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition = "!IsTargetPinUnconnected()"))
	FPCGAttributePropertyInputSelector ActorReferenceAttribute;

	/** Extra output pins. Cached data whose stored pin label matches one of these exactly is routed there; anything
	 *  else goes to Out. Copy-paste the Set node's Custom Input Pins here. Labels colliding with Out or Status are ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pins", meta = (TitleProperty = "{Label}"))
	TArray<FPCGPinProperties> CustomOutputPins;

	/** Tag every output with 'CacheID:<id>' so entries can be told apart, mostly useful with Read All Entries. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bTagWithCacheID = false;

	/** Emit a Status attribute set: one row per target actor with Found, EntryCount, ActorReference and CacheID. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bOutputStatus = true;

	/** Suppress the warning when no target actor could be resolved. A cache miss never warns. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietMissingTargetWarning = false;

	/** Custom output pins minus None labels, reserved labels and duplicates; the pins actually declared. */
	TArray<FPCGPinProperties> GetSanitizedCustomOutputPins() const;

protected:
#if WITH_EDITOR
	UFUNCTION()
	bool IsTargetPinUnconnected() const;
#endif
};

struct FPCGExGetCachedDataContext final : FPCGExContext
{
	struct FStatusRow
	{
		FSoftObjectPath Actor;
		bool bFound = false;
		int32 EntryCount = 0;
	};

	/** Cached data copied out during Boot (game thread). Pin is the label it was stored with. */
	TArray<FPCGTaggedData> Reads;

	/** One per target actor; a single not-found row when no actor resolved, so Status always has something to branch on. */
	TArray<FStatusRow> StatusRows;

	/** GC root for Reads: StageOutput(None) does not root, and the cache may drop its own reference before we flush. */
	TSet<TObjectPtr<const UPCGData>> ReferencedObjects;

protected:
	virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) override;
};

class FPCGExGetCachedDataElement final : public IPCGExElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(GetCachedData)
	// Target resolution touches actors and may spawn the PCG World Actor; the read itself is a pointer copy.
	PCGEX_ELEMENT_MAIN_THREAD_ONLY_IN_PREPARE()

	/** The cache changes with no dependency-CRC change; a cached result would be stale. */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
