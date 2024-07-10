// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataDetails.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExBoundsPathIntersection.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExBoundsPathIntersectionSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BoundsPathIntersection, "Path × Bounds Intersection", "Find intersection with target input points.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOmitSinglePointOutputs = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBoxIntersectionDetails IntersectionDetails;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBoundsPathIntersectionContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExBoundsPathIntersectionElement;

	virtual ~FPCGExBoundsPathIntersectionContext() override;

	PCGExData::FFacade* BoundsDataFacade = nullptr;
	
};

class PCGEXTENDEDTOOLKIT_API FPCGExBoundsPathIntersectionElement final : public FPCGExPathProcessorElement
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

namespace PCGExPathIntersections
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		bool bClosedPath = false;
		int32 LastIndex = 0;
		PCGExGeo::FPointBoxCloud* Cloud = nullptr;
		PCGExGeo::FSegmentation* Segmentation = nullptr;

		PCGEx::TFAttributeWriter<bool>* IsIntersectionWriter = nullptr;
		PCGEx::TFAttributeWriter<int32>* BoundIndexWriter = nullptr;
		PCGEx::TFAttributeWriter<bool>* IsInsideWriter = nullptr;
		
	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
			
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
