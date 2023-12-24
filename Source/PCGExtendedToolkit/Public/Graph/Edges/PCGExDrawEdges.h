// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExDrawEdges.generated.h"

#define PCGEX_WRITEEDGEEXTRA_FOREACH(MACRO)\
MACRO(EdgeLength, double)

namespace PCGExDataBlending
{
	class FMetadataBlender;
	struct FPropertiesBlender;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawEdgesSettings : public UPCGExEdgesProcessorSettings, public IPCGExDebug
{
	GENERATED_BODY()

public:
	UPCGExDrawEdgesSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DrawEdges, "Draw Edges", "Draws debug edges");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorDebug; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin IPCGExDebug interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

#if WITH_EDITOR
	virtual bool IsDebugEnabled() const override { return bEnabled && bDebug; }
#endif

	//~End IPCGExDebug interface

	/** Draw color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Color", meta=(PCG_Overridable))
	FColor Color = FColor::Cyan;
	
private:
	friend class FPCGExDrawEdgesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDrawEdgesContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExDrawEdgesElement;

	virtual ~FPCGExDrawEdgesContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDrawEdgesElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
