// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"

#include "PCGExToggleTopology.generated.h"

UENUM()
enum class EPCGExToggleTopologyAction : uint8
{
	Toggle = 0 UMETA(DisplayName = "Toggle", ToolTip="..."),
	Remove = 1 UMETA(DisplayName = "Remove", ToolTip="..."),
};

UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="topology/cluster-surface/toggle-topology"))
class UPCGExToggleTopologySettings : public UPCGExSettings
{
	GENERATED_BODY()

	friend class FPCGExToggleTopologyElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ToggleTopology, "Topology : Toggle (DEPRECATED)", "Registers/unregister or Removes PCGEx spawned dynamic meshes. Use OutputMode : Dynamic Mesh to use the mesh with the PCG Geometry Script interop stack from now on.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::DynamicMesh; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExToggleTopologyAction Action = EPCGExToggleTopologyAction::Toggle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Action == EPCGExToggleTopologyAction::Toggle", EditConditionHides))
	bool bToggle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFilterByTag = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFilterByTag", EditConditionHides))
	FName CommaSeparatedTagFilters = FName("PCGExTopology");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	TSoftObjectPtr<AActor> TargetActor;
};

struct FPCGExToggleTopologyContext final : FPCGExContext
{
	friend class FPCGExToggleTopologyElement;
	bool bWait = true;
};

class FPCGExToggleTopologyElement final : public IPCGExElement
{
public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ToggleTopology)

	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
};
