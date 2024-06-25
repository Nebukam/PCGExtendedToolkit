// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"
#include "PCGExFindClustersData.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExFindClustersDataSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindClustersData, "Find Clusters Data", "Find vtx/edge pairs among a soup of data collections");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorEdge; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface

	/** Warning about inputs mismatch and triage */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	bool bSkipTrivialWarnings = false;

	/** Warning that you'll get anyway if you try these inputs in a cluster node*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	bool bSkipImportantWarnings = false;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindClustersDataContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExFindClustersDataElement;

	virtual ~FPCGExFindClustersDataContext() override;

	PCGExData::FPointIOCollection* MainEdges = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFindClustersDataElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
