// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "Data/PCGExGraphParamsData.h"
#include "PCGExCreateGraphParams.generated.h"

UENUM(BlueprintType)
enum class EPCGExGraphModel : uint8
{
	Custom UMETA(DisplayName = "Custom Sockets", Tooltip="Edit socket individually."),
	Grid UMETA(DisplayName = "3D Grid", Tooltip="A 3D-like model, with 6 sockets, 2 for each axis (Plus/Minus)."),
	PlaneXY UMETA(DisplayName = "Plane - XY", Tooltip="A 2D-like model, with 4 sockets, 2 for each X & Y axis (Plus/Minus)."),
	PlaneXZ UMETA(DisplayName = "Plane - XZ", Tooltip="A 2D-like model, with 4 sockets, 2 for each X & Z axis (Plus/Minus)."),
	PlaneYZ UMETA(DisplayName = "Plane - YZ", Tooltip="A 2D-like model, with 4 sockets, 2 for each Y & Z axis (Plus/Minus)."),
	TwoSidedX UMETA(DisplayName = "Two Sided - X", Tooltip="A wide front-back model, with 2 opposite sockets, over the X axis."),
	TwoSidedY UMETA(DisplayName = "Two Sided - Y", Tooltip="A wide front-back model, with 2 opposite sockets, over the Y axis."),
	TwoSidedZ UMETA(DisplayName = "Two Sided - Z", Tooltip="A wide front-back model, with 2 opposite sockets, over the Z axis."),
	FFork UMETA(DisplayName = "Forward Fork", Tooltip="A fork-like model, with 2 forward sockets, lefty and righty."),
};

/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateGraphParamsSettings : public UPCGSettings
{
	GENERATED_BODY()

	UPCGExCreateGraphParamsSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(GraphParams, "Graph Params", "Builds a collection of PCG-compatible data from the selected actors.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Attribute name to store graph data to. Used as prefix.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName GraphIdentifier = "GraphIdentifier";

	/** Preset graph model.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExGraphModel GraphModel = EPCGExGraphModel::Grid;

	/** If true, preset will have Input & Output types instead of "Any"*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="GraphModel!=EPCGExGraphModel::Custom"))
	bool bTypedPreset = true;
	
	/** Custom graph sockets model.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{SocketName}", EditCondition="GraphModel==EPCGExGraphModel::Custom", EditConditionHides))
	TArray<FPCGExSocketDescriptor> CustomSockets;

	/** Preset graph sockets model. Don't bother editing this, it will be overriden in code -- instead, copy it, choose Custom model and paste.*/
	UPROPERTY(VisibleAnywhere, Category = Settings, meta=(TitleProperty="{SocketName}", EditCondition="GraphModel!=EPCGExGraphModel::Custom", EditConditionHides))
	TArray<FPCGExSocketDescriptor> PresetSockets;
	
	/** Overrides individual socket values with a global one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bApplyGlobalOverrides = false;

	/** Individual socket properties overrides */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bApplyGlobalOverrides"))
	FPCGExSocketGlobalOverrides GlobalOverrides;

	const TArray<FPCGExSocketDescriptor>& GetSockets() const;
	
protected:
	virtual void InitDefaultSockets();
	void InitSocketContent(TArray<FPCGExSocketDescriptor>& OutSockets) const;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCreateGraphParamsElement : public FSimplePCGElement
{
protected:
	template <typename T>
	T* BuildParams(FPCGContext* Context) const;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
