// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExPointsToBounds.h"
#include "PCGExSortPoints.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExtendedToolkit/Public/Transform/PCGExTransform.h"

#include "PCGExDiscardByOverlap.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Overlap Test Mode"))
enum class EPCGExOverlapTestMode : uint8
{
	Fast UMETA(DisplayName = "Fast", ToolTip="Only test using datasets' overall bounds"),
	Precise UMETA(DisplayName = "Precise", ToolTip="Test every points' bounds"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Overlap Pruning Order"))
enum class EPCGExOverlapPruningOrder : uint8
{
	OverlapCount UMETA(DisplayName = "Count > Amount", ToolTip="Overlap count"),
	OverlapAmount UMETA(DisplayName = "Amount > Count", ToolTip="Overlap amount"),
};

namespace PCGExDiscardByOverlap
{
	PCGEX_ASYNC_STATE(State_InitialOverlap)
	PCGEX_ASYNC_STATE(State_PreciseOverlap)
	PCGEX_ASYNC_STATE(State_ProcessFastOverlap)
	PCGEX_ASYNC_STATE(State_ProcessPreciseOverlap)

	struct /*PCGEXTENDEDTOOLKIT_API*/ FOverlapStats
	{
		int32 OverlapCount = 0;
		double OverlapAmount = 0;

		FORCEINLINE void Add(const FOverlapStats& Other)
		{
			OverlapCount += Other.OverlapCount;
			OverlapAmount += Other.OverlapAmount;
		}

		FORCEINLINE void Remove(const FOverlapStats& Other)
		{
			OverlapCount -= Other.OverlapCount;
			OverlapAmount -= Other.OverlapAmount;
		}

		FORCEINLINE bool IsValid() const { return OverlapCount > 0; }
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FOverlap
	{
		mutable FRWLock InsertionLock;

		uint64 HashID = 0;
		FBox Intersection = FBox(NoInit);

		const PCGExData::FFacade* Manager = nullptr;
		const PCGExData::FFacade* Managed = nullptr;

		FOverlapStats Stats;

		FOverlap(const PCGExData::FFacade* InManager, const PCGExData::FFacade* InManaged, const FBox& InIntersection):
			Manager(InManager), Managed(InManaged), Intersection(InIntersection)
		{
			HashID = PCGEx::H64U(InManager->Source->IOIndex, InManaged->Source->IOIndex);
		}

		FORCEINLINE const PCGExData::FFacade* GetOtherFacade(const PCGExData::FFacade* InFacade) const { return Manager == InFacade ? Managed : Manager; }
	};
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDiscardByOverlapSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DiscardByOverlap, "Discard By Overlap", "Discard entire datasets based on how they overlap with each other.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Overlap overlap test mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOverlapTestMode TestMode = EPCGExOverlapTestMode::Fast;

	/** Point bounds to be used to compute overlaps */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** Pruning order & prioritization */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOverlapPruningOrder PruningOrder = EPCGExOverlapPruningOrder::OverlapCount;

	/** Pruning order & prioritization \n Ascending = Smaller get pruned first \n Descending = Larger get pruned first  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSortDirection Order = EPCGExSortDirection::Ascending;

private:
	friend class FPCGExDiscardByOverlapElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDiscardByOverlapContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExDiscardByOverlapElement;
	virtual ~FPCGExDiscardByOverlapContext() override;

	mutable FRWLock OverlapLock;
	TArray<PCGExDiscardByOverlap::FOverlap*> Overlaps;
	TMap<uint64, PCGExDiscardByOverlap::FOverlap*> OverlapMap;

	PCGExDiscardByOverlap::FOverlap* RegisterOverlap(const PCGExData::FFacade* InManager, const PCGExData::FFacade* InManaged, const FBox& Intersection);
	PCGExDiscardByOverlap::FOverlap* GetOverlap(const PCGExData::FFacade* InManager, const PCGExData::FFacade* InManaged);
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDiscardByOverlapElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExDiscardByOverlap
{
	struct PCG_API FPointBounds
	{
		FPointBounds(const FPCGPoint& InPoint, const FBox& InBounds):
			Point(&InPoint), LocalBounds(InBounds)
		{
			FTransform TR = InPoint.Transform;
			TR.SetScale3D(FVector::OneVector);
			WorldBounds = FBoxSphereBounds(InBounds.TransformBy(TR));
		}

		const FPCGPoint* Point;
		FBoxSphereBounds WorldBounds;
		FBox LocalBounds;
	};

	struct PCG_API FPointBoundsSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FPointBounds* InPoint)
		{
			return InPoint->WorldBounds;
		}

		FORCEINLINE static const bool AreElementsEqual(const FPointBounds* A, const FPointBounds* B)
		{
			return A->Point == B->Point;
		}

		FORCEINLINE static void ApplyOffset(FPointBounds& InPoint)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FPointBounds* Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		const UPCGExDiscardByOverlapSettings* LocalSettings = nullptr;
		FPCGExDiscardByOverlapContext* LocalTypedContext = nullptr;

		const TArray<FPCGPoint>* InPoints = nullptr;
		int32 NumPoints = 0;
		FBox Bounds = FBox(ForceInit);
		using TBoundsOctree = TOctree2<FPointBounds*, FPointBoundsSemantics>;
		TBoundsOctree* Octree = nullptr;

		TArray<FPointBounds*> LocalPointBounds;

		mutable FRWLock OverlapLock;
		TArray<FOverlap*> Overlaps;
		TArray<FOverlap*> ManagedOverlaps;
		TArray<int32, FOverlap*> OverlapsMap;

		void RegisterOverlap(FOverlap* InOverlap);

	public:
		FOverlapStats Stats;

		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		FORCEINLINE const FBox& GetBounds() const { return Bounds; }
		FORCEINLINE const TArray<FPointBounds*>& GetPointBounds() const { return LocalPointBounds; }
		FORCEINLINE const TBoundsOctree* GetOctree() const { return Octree; }

		//virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
		int32 RemoveOverlap(const PCGExData::FFacade* InFacade);
	};
}
