// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExSampleNeighbors.generated.h"

namespace PCGExContours
{
	struct PCGEXTENDEDTOOLKIT_API FCandidate
	{
		explicit FCandidate(const int32 InNodeIndex)
			: NodeIndex(InNodeIndex)
		{
		}

		int32 NodeIndex;
		double Distance = TNumericLimits<double>::Max();
		double Dot = -1;
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNeighborsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleNeighborsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNeighbors, "Sample : Neighbors", "Sample graph node' neighbors values.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

	/** Distance method to be used for node & neighbors points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	FPCGExDistanceSettings DistanceSettings;

	/** Curve that balances weight over distance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Weighting", meta=(PCG_Overridable))
	TSoftObjectPtr<UCurveFloat> WeightOverDistance;

	/** Attributes to sample from the neighboring vtx */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Attributes", meta=(PCG_Overridable))
	TMap<FName, EPCGExDataBlendingType> VtxAttributes;

	/** Attributes to sample from the neighboring edges */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Attributes", meta=(PCG_Overridable))
	TMap<FName, EPCGExDataBlendingType> EdgeAttributes;

private:
	friend class FPCGExSampleNeighborsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborsContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExSampleNeighborsElement;

	virtual ~FPCGExSampleNeighborsContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborsElement : public FPCGExEdgesProcessorElement
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
