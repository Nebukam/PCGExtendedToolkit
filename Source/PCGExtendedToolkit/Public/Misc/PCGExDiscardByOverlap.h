// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"


#include "PCGExDiscardByOverlap.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Overlap Test Mode"))
enum class EPCGExOverlapTestMode : uint8
{
	Fast    = 0 UMETA(DisplayName = "Fast", ToolTip="Only test using datasets' overall bounds"),
	Precise = 1 UMETA(DisplayName = "Precise", ToolTip="Test every points' bounds"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Overlap Pruning Logic"))
enum class EPCGExOverlapPruningLogic : uint8
{
	LowFirst  = 0 UMETA(DisplayName = "Low to High", ToolTip="Lower weights are pruned first."),
	HighFirst = 1 UMETA(DisplayName = "High to Low", ToolTip="Higher weights are pruned first."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExOverlapScoresWeighting
{
	GENERATED_BODY()

	FPCGExOverlapScoresWeighting()
	{
	}

	/** How much of the dynamic weights to account for vs. static ones */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Dynamic Weights", EditAnywhere, meta = (PCG_Overridable))
	double DynamicBalance = 1;

	/** Overlap count weight (how many sets overlap) */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Dynamic Weights", EditAnywhere, meta = (PCG_Overridable))
	double OverlapCount = 2;

	/** Overlap Sub-Count weight (how many points overlap) */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Dynamic Weights", EditAnywhere, meta = (PCG_Overridable))
	double OverlapSubCount = 1;

	/** Overlap volume weight (cumulative volume overlap)  Note that each sub point adds its own intersection volume whether or not it occupies an already computed volume in space. */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Dynamic Weights", EditAnywhere, meta = (PCG_Overridable))
	double OverlapVolume = 0;

	/** Overlap volume density weight (cumulative volume overlap / number of overlapping points) */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Dynamic Weights", EditAnywhere, meta = (PCG_Overridable))
	double OverlapVolumeDensity = 0;

	/** How much of the static weights to account for vs. dynamic ones */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Static Weights", EditAnywhere, meta = (PCG_Overridable))
	double StaticBalance = 0.5;

	/** Number of points weight (points in a given set) */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Static Weights", EditAnywhere, meta = (PCG_Overridable))
	double NumPoints = 1;

	/** Volume weight  Note that each sub point adds its own intersection volume whether or not it occupies an already computed volume in space. */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Static Weights", EditAnywhere, meta = (PCG_Overridable))
	double Volume = 0;

	/** Volume density. */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Static Weights", EditAnywhere, meta = (PCG_Overridable))
	double VolumeDensity = 0;

	/** Weight of custom tags scores, if any. */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Static Weights", EditAnywhere, meta = (PCG_Overridable))
	double CustomTagWeight = 0;

	/** Lets you add custom 'score' by tags. If the tag is found on the collection, its score will be added to the computation, letting you have more granular control. */
	UPROPERTY(BlueprintReadWrite, Category = "Settings|Static Weights", EditAnywhere, meta = (PCG_Overridable))
	TMap<FString, double> TagScores;

	double CustomTagScore = 0;

	double StaticWeightSum = 0;
	double DynamicWeightSum = 0;

	void Init();
	void ResetMin();
	void Max(const FPCGExOverlapScoresWeighting& Other);
};

namespace PCGExDiscardByOverlap
{
	struct FOverlapStats;
	struct FOverlap;
	class FProcessor;
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
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filter which points can be considered for overlap.", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Overlap overlap test mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOverlapTestMode TestMode = EPCGExOverlapTestMode::Precise;

	/** Point bounds to be used to compute overlaps */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** Scores weighting */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExOverlapScoresWeighting Weighting;

	/** Pruning order & prioritization */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOverlapPruningLogic Logic = EPCGExOverlapPruningLogic::HighFirst;

	/** The minimum amount two sub-points must overlap to be added to the comparison.  The higher, the more "overlap" there must be. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double MinThreshold = 0.1;

	/** How to interpret the min overlap value.  Discrete means distance in world space  Relative means uses percentage (0-1) of the averaged radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMeanMeasure ThresholdMeasure = EPCGExMeanMeasure::Relative;

	/** If enabled, points that are filtered out from overlap detection are still accounted for in static metrics/maths. i.e they still participate to the overall bounds shape etc instead of being thoroughly ignored. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bIncludeFilteredInMetrics = true;

private:
	friend class FPCGExDiscardByOverlapElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDiscardByOverlapContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExDiscardByOverlapElement;

	mutable FRWLock OverlapLock;
	TMap<uint64, TSharedPtr<PCGExDiscardByOverlap::FOverlap>> OverlapMap;

	TSharedPtr<PCGExDiscardByOverlap::FOverlap> RegisterOverlap(
		PCGExDiscardByOverlap::FProcessor* InManager,
		PCGExDiscardByOverlap::FProcessor* InManaged,
		const FBox& InIntersection);

	FPCGExOverlapScoresWeighting Weights;
	FPCGExOverlapScoresWeighting MaxScores;
	void UpdateMaxScores(const TArray<PCGExDiscardByOverlap::FProcessor*>& InStack);

	void Prune();
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
	class /*PCGEXTENDEDTOOLKIT_API*/ FPruneTask final : public PCGExMT::FPCGExTask
	{
	public:
		FPruneTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
			: FPCGExTask(InPointIO)

		{
		}

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			FPCGExDiscardByOverlapContext* Context = AsyncManager->GetContext<FPCGExDiscardByOverlapContext>();
			Context->Prune();
			return false;
		}
	};

	class FProcessor;

	struct /*PCGEXTENDEDTOOLKIT_API*/ FOverlapStats
	{
		int32 OverlapCount = 0;
		double OverlapVolume = 0;
		double OverlapVolumeAvg = 0;
		double RelativeOverlapCount = 0;
		double RelativeOverlapVolume = 0;

		FORCEINLINE void Add(const FOverlapStats& Other)
		{
			OverlapCount += Other.OverlapCount;
			OverlapVolume += Other.OverlapVolume;
		}

		FORCEINLINE void Remove(const FOverlapStats& Other)
		{
			OverlapCount -= Other.OverlapCount;
			OverlapVolume -= Other.OverlapVolume;
		}

		FORCEINLINE void Add(const FOverlapStats& Other, const int32 MaxCount, const double MaxVolume)
		{
			Add(Other);
			UpdateRelative(MaxCount, MaxVolume);
		}

		FORCEINLINE void Remove(const FOverlapStats& Other, const int32 MaxCount, const double MaxVolume)
		{
			Remove(Other);
			UpdateRelative(MaxCount, MaxVolume);
		}

		FORCEINLINE void UpdateRelative(const int32 MaxCount, const double MaxVolume)
		{
			OverlapVolumeAvg = OverlapVolume / OverlapCount;
			RelativeOverlapCount = OverlapCount / MaxCount;
			RelativeOverlapVolume = OverlapVolume / MaxVolume;
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FOverlap
	{
		uint64 HashID = 0;
		FBox Intersection = FBox(NoInit);
		bool IsValid = true;

		FProcessor* Manager = nullptr;
		FProcessor* Managed = nullptr;

		FOverlapStats Stats;

		FOverlap(FProcessor* InManager, FProcessor* InManaged, const FBox& InIntersection);
		FORCEINLINE FProcessor* GetOther(const FProcessor* InCandidate) const { return Manager == InCandidate ? Managed : Manager; }
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointBounds
	{
		FPointBounds(const int32 InIndex, const FPCGPoint& InPoint, const FBox& InBounds):
			Index(InIndex), Point(&InPoint), Bounds(InBounds)
		{
		}

		const int32 Index;
		const FPCGPoint* Point;
		FBoxSphereBounds Bounds;
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointBoundsSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FPointBounds* InPoint)
		{
			return InPoint->Bounds;
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

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExDiscardByOverlapContext, UPCGExDiscardByOverlapSettings>
	{
		friend struct FPCGExDiscardByOverlapContext;

		const TArray<FPCGPoint>* InPoints = nullptr;
		FBox Bounds = FBox(ForceInit);

		using TBoundsOctree = TOctree2<FPointBounds*, FPointBoundsSemantics>;
		TUniquePtr<TBoundsOctree> Octree;

		TArray<TSharedPtr<FPointBounds>> LocalPointBounds;

		mutable FRWLock RegistrationLock;
		TArray<TSharedPtr<FOverlap>> Overlaps;
		TArray<TSharedPtr<FOverlap>> ManagedOverlaps;

		int32 NumPoints = 0;
		double TotalVolume = 0;
		double VolumeDensity = 0;
		double TotalDensity = 0;

	public:
		FPCGExOverlapScoresWeighting RawScores;

		double StaticWeight = 0;
		double DynamicWeight = 0;
		double Weight = 0;

		FOverlapStats Stats;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		FORCEINLINE const FBox& GetBounds() const { return Bounds; }
		FORCEINLINE const TArray<TSharedPtr<FPointBounds>>& GetPointBounds() const { return LocalPointBounds; }
		FORCEINLINE const TBoundsOctree* GetOctree() const { return Octree.Get(); }

		//virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		FORCEINLINE bool HasOverlaps() const { return !Overlaps.IsEmpty(); }

		void RegisterOverlap(FProcessor* InManaged, const FBox& Intersection);
		void RemoveOverlap(const TSharedPtr<FOverlap>& InOverlap, TArray<FProcessor*>& Stack);
		void Prune(TArray<FProcessor*>& Stack);

		FORCEINLINE void RegisterPointBounds(const int32 Index, const TSharedPtr<FPointBounds>& InPointBounds)
		{
			const bool bValidPoint = PointFilterCache[Index];
			if (!bValidPoint && !Settings->bIncludeFilteredInMetrics) { return; }

			const FBox& B = InPointBounds->Bounds.GetBox();
			Bounds += B;
			TotalVolume += B.GetVolume();

			if (bValidPoint) { LocalPointBounds[Index] = InPointBounds; }
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
		virtual void Write() override;

		void UpdateWeightValues();
		void UpdateWeight(const FPCGExOverlapScoresWeighting& InMax);
	};
}
