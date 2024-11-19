// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGEx.h"
#include "PCGExPointsProcessor.h"

#include "PCGExToggleTopology.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExToggleTopologySettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExToggleTopologyElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS(ToggleTopology, "Topology : Toggle", "Registers or unregister PCGEx spawned dynamic meshes.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPrimitives; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bToggle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFilterByTag = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFilterByTag"))
	FName FilterByTag = FName("PCGExTopology");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	TSoftObjectPtr<AActor> TargetActor;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExToggleTopologyContext final : FPCGContext
{
	friend class FPCGExToggleTopologyElement;
	bool bWait = true;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExToggleTopologyElement final : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	//virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
