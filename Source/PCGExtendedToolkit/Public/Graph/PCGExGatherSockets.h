// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"

#include "PCGExGatherSockets.generated.h"


/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExGatherSocketsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGExGatherSocketsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS(GraphParams, "Custom Graph : Assemble Sockets", "Assembles Sockets Params into a single, consolidated Custom Graph Sockets object.")
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorSocket; }
	
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	/** Attribute name to store graph data to. Used as prefix.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName GraphIdentifier = "GraphIdentifier";

	/** Overrides individual socket values with a global one.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bApplyGlobalOverrides = false;

	/** Individual socket properties overrides */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bApplyGlobalOverrides"))
	FPCGExSocketGlobalOverrides GlobalOverrides;

	/** An array containing the computed socket names, for easy copy-paste. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, EditFixedSize, Category = Settings, meta=(AdvancedDisplay, TitleProperty="{BaseName}"))
	TArray<FPCGExSocketQualityOfLifeInfos> GeneratedSocketNames;

	TArray<FPCGExSocketDescriptor> InputSockets;

	/** An array containing the computed socket names, for easy copy-paste. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, EditFixedSize, Category = Settings, meta=(AdvancedDisplay, TitleProperty="{BaseName}"))
	TArray<FPCGExSocketQualityOfLifeInfos> SocketNames;

	const TArray<FPCGExSocketDescriptor>& GetSockets() const;

	void RefreshSocketNames();

protected:
};

class PCGEXTENDEDTOOLKIT_API FPCGExGatherSocketsElement : public IPCGElement
{
#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
};
