// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExMakeClustersUnique.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/packing/make-unique"))
class UPCGExMakeClustersUniqueSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MakeClustersUnique, "Cluster : Make Unique", "Outputs a new, unique data pointer for the input clusters; to avoid overlap and unexpected behaviors.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterOp; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface
};

struct FPCGExMakeClustersUniqueContext final : FPCGExEdgesProcessorContext
{
	friend class UPCGExMakeClustersUniqueSettings;
	friend class FPCGExMakeClustersUniqueElement;
};

class FPCGExMakeClustersUniqueElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(MakeClustersUnique)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
