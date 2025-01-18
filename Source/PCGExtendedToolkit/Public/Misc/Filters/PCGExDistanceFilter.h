// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"

#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"


#include "PCGExDistanceFilter.generated.h"


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDistanceFilterConfig
{
	GENERATED_BODY()

	FPCGExDistanceFilterConfig()
	{
	}

	/** Distance method to be used for source & target points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExDistanceDetails DistanceDetails;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Type of OperandB */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType CompareAgainst = EPCGExInputValueType::Constant;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Distance Threshold", EditCondition="CompareAgainst!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector DistanceThreshold;

	/** Operand B for testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Distance Threshold", EditCondition="CompareAgainst==EPCGExInputValueType::Constant", EditConditionHides))
	double DistanceThresholdConstant = 0;

	/** Rounding mode for relative measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** If enabled, a collection will never be tested against itself */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bIgnoreSelf = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDistanceFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExDistanceFilterConfig Config;

	TArray<const PointOctree*> Octrees;
	TArray<uintptr_t> TargetsPtr;

	virtual bool SupportsLiveTesting() override;

	virtual bool Init(FPCGExContext* InContext) override;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;

	virtual bool GetRequiresPreparation(FPCGExContext* InContext) override { return true; }
	virtual bool Prepare(FPCGExContext* InContext) override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FDistanceFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit FDistanceFilter(const TObjectPtr<const UPCGExDistanceFilterFactory>& InDefinition)
			: FSimpleFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
			Octrees = &TypedFilterFactory->Octrees;
			TargetsPtr = &TypedFilterFactory->TargetsPtr;
			bIgnoreSelf = TypedFilterFactory->Config.bIgnoreSelf;
		}

		const TObjectPtr<const UPCGExDistanceFilterFactory> TypedFilterFactory;

		TSharedPtr<PCGExDetails::FDistances> Distances;

		const TArray<const UPCGPointData::PointOctree*>* Octrees = nullptr;
		const TArray<uintptr_t>* TargetsPtr = nullptr;
		uintptr_t SelfPtr = 0;
		bool bIgnoreSelf = false;

		const FPCGPoint* InPointsStart = nullptr;
		int32 NumTargets = -1;

		const double DistanceThreshold = 0;
		TSharedPtr<PCGExData::TBuffer<double>> DistanceThresholdGetter;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const FPCGPoint& Point) const override
		{
			double BestDist = MAX_dbl;

			if (Distances->bOverlapIsZero)
			{
				bool bOverlap = false;
				for (int i = 0; i < NumTargets; i++)
				{
					const UPCGPointData::PointOctree* TargetOctree = *(Octrees->GetData() + i);
					if (const uintptr_t Current = *(TargetsPtr->GetData() + i); Current == SelfPtr)
					{
						if (bIgnoreSelf) { continue; }
						
						// Ignore current point when testing against self
						TargetOctree->FindNearbyElements(
							Point.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
							{
								double Dist = Distances->GetDistSquared(Point, *PointRef.Point, bOverlap);
								if (bOverlap) { Dist = 0; }
								if (Dist > BestDist) { return; }
								BestDist = Dist;
							});
					}
					else
					{
						TargetOctree->FindNearbyElements(
							Point.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
							{
								double Dist = Distances->GetDistSquared(Point, *PointRef.Point, bOverlap);
								if (bOverlap) { Dist = 0; }
								if (Dist > BestDist) { return; }
								BestDist = Dist;
							});
					}
				}
			}
			else
			{
				for (int i = 0; i < NumTargets; i++)
				{
					const UPCGPointData::PointOctree* TargetOctree = *(Octrees->GetData() + i);

					if (const uintptr_t Current = *(TargetsPtr->GetData() + i); Current == SelfPtr)
					{
						if (bIgnoreSelf) { continue; }
						
						// Ignore current point when testing against self
						TargetOctree->FindNearbyElements(
							Point.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
							{
								const double Dist = Distances->GetDistSquared(Point, *PointRef.Point);
								if (Dist > BestDist) { return; }
								BestDist = Dist;
							});
					}
					else
					{
						TargetOctree->FindNearbyElements(
							Point.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
							{
								const double Dist = Distances->GetDistSquared(Point, *PointRef.Point);
								if (Dist > BestDist) { return; }
								BestDist = Dist;
							});
					}
				}
			}

			return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FMath::Sqrt(BestDist), TypedFilterFactory->Config.DistanceThresholdConstant, TypedFilterFactory->Config.Tolerance);
		}

		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			const double B = DistanceThresholdGetter ? DistanceThresholdGetter->Read(PointIndex) : TypedFilterFactory->Config.DistanceThresholdConstant;

			const FPCGPoint& SourcePt = PointDataFacade->Source->GetInPoint(PointIndex);
			double BestDist = MAX_dbl;

			if (Distances->bOverlapIsZero)
			{
				bool bOverlap = false;
				for (int i = 0; i < NumTargets; i++)
				{
					const UPCGPointData::PointOctree* TargetOctree = *(Octrees->GetData() + i);
					if (const uintptr_t Current = *(TargetsPtr->GetData() + i); Current == SelfPtr)
					{
						if (bIgnoreSelf) { continue; }
						
						// Ignore current point when testing against self
						TargetOctree->FindNearbyElements(
							SourcePt.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
							{
								if (const ptrdiff_t OtherIndex = PointRef.Point - PointDataFacade->GetIn()->GetPoints().GetData();
									OtherIndex == PointIndex) { return; }

								double Dist = Distances->GetDistSquared(SourcePt, *PointRef.Point, bOverlap);
								if (bOverlap) { Dist = 0; }
								if (Dist > BestDist) { return; }
								BestDist = Dist;
							});
					}
					else
					{						
						TargetOctree->FindNearbyElements(
							SourcePt.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
							{
								double Dist = Distances->GetDistSquared(SourcePt, *PointRef.Point, bOverlap);
								if (bOverlap) { Dist = 0; }
								if (Dist > BestDist) { return; }
								BestDist = Dist;
							});
					}
				}
			}
			else
			{
				for (int i = 0; i < NumTargets; i++)
				{
					const UPCGPointData::PointOctree* TargetOctree = *(Octrees->GetData() + i);

					if (const uintptr_t Current = *(TargetsPtr->GetData() + i); Current == SelfPtr)
					{
						if (bIgnoreSelf) { continue; }
						
						// Ignore current point when testing against self
						TargetOctree->FindNearbyElements(
							SourcePt.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
							{
								if (const ptrdiff_t OtherIndex = PointRef.Point - PointDataFacade->GetIn()->GetPoints().GetData();
									OtherIndex == PointIndex) { return; }

								const double Dist = Distances->GetDistSquared(SourcePt, *PointRef.Point);
								if (Dist > BestDist) { return; }
								BestDist = Dist;
							});
					}
					else
					{
						TargetOctree->FindNearbyElements(
							SourcePt.Transform.GetLocation(), [&](const FPCGPointRef& PointRef)
							{
								const double Dist = Distances->GetDistSquared(SourcePt, *PointRef.Point);
								if (Dist > BestDist) { return; }
								BestDist = Dist;
							});
					}
				}
			}

			return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, FMath::Sqrt(BestDist), B, TypedFilterFactory->Config.Tolerance);
		}

		virtual ~FDistanceFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDistanceFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		DistanceFilterFactory, "Filter : Distance", "Creates a filter definition that compares the distance from the point to the nearest target.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDistanceFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
