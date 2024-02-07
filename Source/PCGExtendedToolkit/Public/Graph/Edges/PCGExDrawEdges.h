// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExDrawEdges.generated.h"

#define PCGEX_FOREACH_SAMPLING_FIELD(MACRO)\
MACRO(EdgeLength, double)

namespace PCGExDataBlending
{
	class FMetadataBlender;
	struct FPropertiesBlender;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExDrawEdgesSettings : public UPCGExEdgesProcessorSettings
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

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin IPCGExDebug interface
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End IPCGExDebug interface

	/** Draw color. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta=(PCG_Overridable))
	FColor Color = FColor::Cyan;

	/** Lerp to secondary color based on cluster index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bLerpColor = false;

	/** Lerp to secondary color based on cluster index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta=(PCG_Overridable, EditCondition="bLerpColor"))
	FColor SecondaryColor = FColor::Red;

	/** Draw thickness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta=(PCG_Overridable, ClampMin=0.01, ClampMax=100))
	double Thickness = 0.5;

	/** Depth priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta=(PCG_Overridable))
	int32 DepthPriority = 0;

	/** Debug drawing toggle. Exposed to have more control on debug draw in sub-graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug", meta=(PCG_Overridable))
	bool bPCGExDebug = true;

private:
	friend class FPCGExDrawEdgesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDrawEdgesContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExDrawEdgesElement;

	double MaxLerp = 1;
	double CurrentLerp = 0;

	virtual ~FPCGExDrawEdgesContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDrawEdgesElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
