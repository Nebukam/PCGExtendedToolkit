// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"

#include "Graph/PCGExIntersections.h"
#include "PCGExBoundsPathIntersection.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBoundsPathIntersectionSettings : public UPCGExPathProcessorSettings
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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Output"))
	FPCGExBoxIntersectionDetails OutputSettings;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoundsPathIntersectionContext final : FPCGExPathProcessorContext
{
	friend class FPCGExBoundsPathIntersectionElement;

	TSharedPtr<PCGExData::FFacade> BoundsDataFacade;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoundsPathIntersectionElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathIntersections
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBoundsPathIntersectionContext, UPCGExBoundsPathIntersectionSettings>
	{
		bool bClosedLoop = false;
		int32 LastIndex = 0;
		TSharedPtr<PCGExGeo::FPointBoxCloud> Cloud;
		TSharedPtr<PCGExGeo::FSegmentation> Segmentation;

		FPCGExBoxIntersectionDetails Details;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		void FindIntersections(const int32 Index) const;
		void InsertIntersections(const int32 Index) const;
		void OnInsertionComplete();

		FORCEINLINE virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override
		{
			if (Details.InsideForwardHandler)
			{
				TArray<TSharedPtr<PCGExGeo::FPointBox>> Overlaps;
				const bool bContained = Cloud->IsInsideMinusEpsilon(Point.Transform.GetLocation(), Overlaps); // Avoid intersections being captured
				Details.SetIsInside(Index, bContained, bContained ? Overlaps[0]->Index : -1);
			}
			else
			{
				Details.SetIsInside(Index, Cloud->IsInsideMinusEpsilon(Point.Transform.GetLocation()));
			}
		}

		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
