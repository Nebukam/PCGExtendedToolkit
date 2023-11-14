// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "Data/PCGExRelationsParamsData.h"
#include "PCGExCreateRelationsParams.generated.h"

UENUM(BlueprintType)
enum class EPCGExRelationsModel : uint8
{
	Custom UMETA(DisplayName = "Custom Sockets", Tooltip="Edit socket individually."),
	Grid3D UMETA(DisplayName = "3D Grid", Tooltip="A 3D-like model, with 6 sockets, 2 for each axis (Plus/Minus)."),
	GridXY UMETA(DisplayName = "2D Grid - XY", Tooltip="A 2D-like model, with 4 sockets, 2 for each X & Y axis (Plus/Minus)."),
	GridXZ UMETA(DisplayName = "2D Grid - XZ", Tooltip="A 2D-like model, with 4 sockets, 2 for each X & Z axis (Plus/Minus)."),
	GridYZ UMETA(DisplayName = "2D Grid - YZ", Tooltip="A 2D-like model, with 4 sockets, 2 for each Y & Z axis (Plus/Minus)."),
	FFork UMETA(DisplayName = "Forward Fork", Tooltip="A fork-like model, with 2 forward sockets, lefty and righty."),
};

/** Outputs a single RelationalParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Relational|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateRelationsParamsSettings : public UPCGSettings
{
	GENERATED_BODY()

	UPCGExCreateRelationsParamsSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(RelationalParams, "Relational Params", "Builds a collection of PCG-compatible data from the selected actors.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Attribute name to store relation data to. Used as prefix.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName RelationIdentifier = "RelationIdentifier";

	/** Attribute name to store relation data to. Used as prefix.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExRelationsModel RelationsModel = EPCGExRelationsModel::Grid3D;
	
	/** Custom relation sockets model.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(TitleProperty="{SocketName}", EditCondition="RelationsModel==EPCGExRelationsModel::Custom", EditConditionHides))
	TArray<FPCGExSocketDescriptor> CustomSockets;

	/** Preset relation sockets model. Don't bother editing this, it will be overriden in code -- instead, copy it, choose Custom model and paste.*/
	UPROPERTY(VisibleAnywhere, Category = Settings, meta=(TitleProperty="{SocketName}", EditCondition="RelationsModel!=EPCGExRelationsModel::Custom", EditConditionHides))
	TArray<FPCGExSocketDescriptor> SocketsPreset;
	
	/** Overrides individual socket values with a global one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bApplyGlobalOverrides = false;

	/** Individual socket properties overrides */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bApplyGlobalOverrides"))
	FPCGExSocketGlobalOverrides GlobalOverrides;

	const TArray<FPCGExSocketDescriptor>& GetSockets() const;
	
protected:
	virtual void InitDefaultSockets();
	void InitSocketContent(TArray<FPCGExSocketDescriptor>& OutSockets) const;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCreateRelationsParamsElement : public FSimplePCGElement
{
protected:
	template <typename T>
	T* BuildParams(FPCGContext* Context) const;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
