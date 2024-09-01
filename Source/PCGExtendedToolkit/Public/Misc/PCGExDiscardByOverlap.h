// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExPointsToBounds.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExtendedToolkit/Public/Transform/PCGExTransform.h"

#include "PCGExDiscardByOverlap.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Overlap Test Mode"))
enum class EPCGExOverlapTestMode : uint8
{
	Fast UMETA(DisplayName = "Fast", ToolTip="Only test using datasets' overall bounds"),
	Precise UMETA(DisplayName = "Precise", ToolTip="Test every points' bounds"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Overlap Pruning Logic"))
enum class EPCGExOverlapPruningLogic : uint8
{
	LowFirst UMETA(DisplayName = "Low to High", ToolTip="Lower weights are pruned first."),
	HighFirst UMETA(DisplayName = "High to Low", ToolTip="Higher weights are pruned first."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExOverlapScoresWeighting
{
	GENERATED_BODY()

	FPCGExOverlapScoresWeighting()
	{
	}

	/** How much of the dynamic weights to account for vs. static ones */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double DynamicBalance = 1;

	/** Overlap count weight (how many sets overlap) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Dynamic Weights", meta = (PCG_Overridable))
	double OverlapCount = 2;

	/** Overlap Sub-Count weight (how many points overlap) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Dynamic Weights", meta = (PCG_Overridable))
	double OverlapSubCount = 1;

	/** Overlap volume weight (cumulative volume overlap) \n Note that each sub point adds its own intersection volume whether or not it occupies an already computed volume in space. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Dynamic Weights", meta = (PCG_Overridable))
	double OverlapVolume = 0;

	/** Overlap volume density weight (cumulative volume overlap / number of overlapping points) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Dynamic Weights", meta = (PCG_Overridable))
	double OverlapVolumeDensity = 0;


	/** How much of the static weights to account for vs. dynamic ones */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double StaticBalance = 0.5;

	/** Number of points' weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Static Weights", meta = (PCG_Overridable))
	double NumPoints = 1;

	/** Volume weight \n Note that each sub point adds its own intersection volume whether or not it occupies an already computed volume in space. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Static Weights", meta = (PCG_Overridable))
	double Volume = 0;

	/** Volume density. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Static Weights", meta = (PCG_Overridable))
	double VolumeDensity = 0;

	double StaticWeightSum = 0;
	double DynamicWeightSum = 0;

	void Init()
	{
		StaticWeightSum = FMath::Abs(NumPoints) + FMath::Abs(Volume) + FMath::Abs(VolumeDensity);
		NumPoints /= StaticWeightSum;
		Volume /= StaticWeightSum;
		VolumeDensity /= StaticWeightSum;

		DynamicWeightSum = FMath::Abs(OverlapCount) + FMath::Abs(OverlapSubCount) + FMath::Abs(OverlapVolume) + FMath::Abs(OverlapVolumeDensity);
		OverlapCount /= DynamicWeightSum;
		OverlapSubCount /= DynamicWeightSum;
		OverlapVolume /= DynamicWeightSum;
		OverlapVolumeDensity /= DynamicWeightSum;

		const double Balance = FMath::Abs(DynamicBalance) + FMath::Abs(StaticBalance);
		DynamicBalance /= Balance;
		StaticBalance /= Balance;
	}

	void ResetMin()
	{
		OverlapCount =
			OverlapSubCount =
			OverlapVolume =
			OverlapVolumeDensity =
			NumPoints =
			Volume =
			VolumeDensity = TNumericLimits<double>::Min();
	}

	void Max(const FPCGExOverlapScoresWeighting& Other)
	{
		OverlapCount = FMath::Max(OverlapCount, Other.OverlapCount);
		OverlapSubCount = FMath::Max(OverlapSubCount, Other.OverlapSubCount);
		OverlapVolume = FMath::Max(OverlapVolume, Other.OverlapVolume);
		OverlapVolumeDensity = FMath::Max(OverlapVolumeDensity, Other.OverlapVolumeDensity);
		NumPoints = FMath::Max(NumPoints, Other.NumPoints);
		Volume = FMath::Max(Volume, Other.Volume);
		VolumeDensity = FMath::Max(VolumeDensity, Other.VolumeDensity);
	}
};

namespace PCGExDiscardByOverlap
{
	PCGEX_ASYNC_STATE(State_InitialOverlap)
	PCGEX_ASYNC_STATE(State_PreciseOverlap)
	PCGEX_ASYNC_STATE(State_ProcessFastOverlap)
	PCGEX_ASYNC_STATE(State_ProcessPreciseOverlap)

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
	//~End UPCGExPointsProcessorSettings

public:
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

	/** The minimum amount two sub-points must overlap to be added to the comparison. \n The higher, the more "overlap" there must be. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double MinThreshold = 0.1;

	/** How to interpret the min overlap value. \n Discrete means distance in world space \n Relative means uses percentage (0-1) of the averaged radius. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMeanMeasure ThresholdMeasure = EPCGExMeanMeasure::Relative;

private:
	friend class FPCGExDiscardByOverlapElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDiscardByOverlapContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExDiscardByOverlapElement;
	virtual ~FPCGExDiscardByOverlapContext() override;

	mutable FRWLock OverlapLock;
	TMap<uint64, PCGExDiscardByOverlap::FOverlap*> OverlapMap;

	PCGExDiscardByOverlap::FOverlap* RegisterOverlap(
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
		FPruneTask(PCGExData::FPointIO* InPointIO)
			: FPCGExTask(InPointIO)

		{
		}

		virtual bool ExecuteTask() override
		{
			FPCGExDiscardByOverlapContext* Context = Manager->GetContext<FPCGExDiscardByOverlapContext>();
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
		FPointBounds(const FPCGPoint& InPoint, const FBox& InBounds):
			Point(&InPoint), Bounds(InBounds)
		{
		}

		const FPCGPoint* Point;
		FBoxSphereBounds Bounds;
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointBoundsSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

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

	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		friend struct FPCGExDiscardByOverlapContext;

		const UPCGExDiscardByOverlapSettings* LocalSettings = nullptr;
		FPCGExDiscardByOverlapContext* LocalTypedContext = nullptr;

		const TArray<FPCGPoint>* InPoints = nullptr;
		FBox Bounds = FBox(ForceInit);

		using TBoundsOctree = TOctree2<FPointBounds*, FPointBoundsSemantics>;
		TBoundsOctree* Octree = nullptr;

		TArray<FPointBounds*> LocalPointBounds;

		mutable FRWLock RegistrationLock;
		TArray<FOverlap*> Overlaps;
		TArray<FOverlap*> ManagedOverlaps;

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

		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		FORCEINLINE const FBox& GetBounds() const { return Bounds; }
		FORCEINLINE const TArray<FPointBounds*>& GetPointBounds() const { return LocalPointBounds; }
		FORCEINLINE const TBoundsOctree* GetOctree() const { return Octree; }

		//virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		virtual ~FProcessor() override;

		FORCEINLINE bool HasOverlaps() const { return !Overlaps.IsEmpty(); }

		void RegisterOverlap(FProcessor* InManaged, const FBox& Intersection);
		void RemoveOverlap(FOverlap* InOverlap, TArray<PCGExDiscardByOverlap::FProcessor*>& Stack);
		void Prune(TArray<PCGExDiscardByOverlap::FProcessor*>& Stack);

		FORCEINLINE void RegisterPointBounds(const int32 Index, FPointBounds* InPointBounds)
		{
			LocalPointBounds[Index] = InPointBounds;
			const FBox B = InPointBounds->Bounds.GetBox();
			Bounds += B;
			TotalVolume += B.GetVolume();
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
		virtual void Write() override;

	protected:
		void UpdateWeightValues();
		void UpdateWeight(const FPCGExOverlapScoresWeighting& InMax);
	};
}
