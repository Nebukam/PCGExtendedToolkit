// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Core/PCGExPointsProcessor.h"
#include "Graphs/Union/PCGExIntersections.h"
#include "PCGExFindClustersData.generated.h"

UENUM()
enum class EPCGExClusterDataSearchMode : uint8
{
	All          = 0 UMETA(DisplayName = "All"),
	VtxFromEdges = 1 UMETA(DisplayName = "Vtx from Edges"),
	EdgesFromVtx = 2 UMETA(DisplayName = "Edges from Vtx"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/find-clusters-data"))
class UPCGExFindClustersDataSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindClustersData, "Find Clusters", "Find vtx/edge pairs inside a soup of data collections");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(FilterHub); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputPin() const override { return PCGExClusters::Labels::OutputVerticesLabel; }

	FName GetSearchOutputPin() const
	{
		return SearchMode == EPCGExClusterDataSearchMode::VtxFromEdges ? FName("Edges") : FName("Vtx");
	}

	//~End UPCGExPointsProcessorSettings

	/** Search mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	EPCGExClusterDataSearchMode SearchMode = EPCGExClusterDataSearchMode::All;

	/** Warning about inputs mismatch and triage */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	bool bSkipTrivialWarnings = false;

	/** Warning that you'll get anyway if you try these inputs in a cluster node*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	bool bSkipImportantWarnings = false;
};

struct FPCGExFindClustersDataContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFindClustersDataElement;

	PCGExDataId SearchKey;
	TSharedPtr<PCGExData::FPointIO> SearchKeyIO;
};

class FPCGExFindClustersDataElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FindClustersData)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
