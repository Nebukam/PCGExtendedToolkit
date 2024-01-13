// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPin.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGEx.h"
#include "PCGExPointsProcessor.h"

#include "PCGExForceGarbageCollection.generated.h"

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExForceGarbageCollectionSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExForceGarbageCollectionElement;

public:
	UPCGExForceGarbageCollectionSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS(ForceGarbageCollection, "Force Garbage Collection", "Triggers a garbage collection cycle.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class PCGEXTENDEDTOOLKIT_API FPCGExForceGarbageCollectionElement : public FPCGPointProcessingElementBase
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
