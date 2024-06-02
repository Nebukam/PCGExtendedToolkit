// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExPointsToBounds.generated.h"

class FPCGExComputeIOBounds;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Point Bounds Source"))
enum class EPCGExPointBoundsSource : uint8
{
	DensityBounds UMETA(DisplayName = "Density Bounds", ToolTip="TBD"),
	ScaledExtents UMETA(DisplayName = "Scaled Extents", ToolTip="TBD"),
	Extents UMETA(DisplayName = "Extents", ToolTip="TBD")
};

namespace PCGExPointsToBounds
{
	constexpr PCGExMT::AsyncState State_ComputeBounds = __COUNTER__;

	struct PCGEXTENDEDTOOLKIT_API FBounds
	{
		FBox Bounds = FBox(ForceInit);
		PCGExData::FPointIO* PointIO;

		TSet<FBounds*> Overlaps;

		TMap<FBounds*, FBox> FastOverlaps;
		TMap<FBounds*, int32> PreciseOverlapCount;
		TMap<FBounds*, double> PreciseOverlapAmount;

		double FastVolume = 0;
		double FastOverlapAmount = 0;
		double PreciseVolume = 0;

		double TotalPreciseOverlapAmount = 0;
		int32 TotalPreciseOverlapCount = 0;

		mutable FRWLock OverlapLock;

		explicit FBounds(PCGExData::FPointIO* InPointIO):
			PointIO(InPointIO)
		{
			Overlaps.Empty();
			FastOverlaps.Empty();
			PreciseOverlapAmount.Empty();
			PreciseOverlapCount.Empty();
		}

		void RemoveOverlap(const FBounds* OtherBounds)
		{
			FWriteScopeLock WriteLock(OverlapLock);

			if (!Overlaps.Contains(OtherBounds)) { return; }

			Overlaps.Remove(OtherBounds);
			FastOverlaps.Remove(OtherBounds);

			if (PreciseOverlapAmount.Contains(OtherBounds))
			{
				TotalPreciseOverlapAmount -= *PreciseOverlapAmount.Find(OtherBounds);
				TotalPreciseOverlapCount -= *PreciseOverlapCount.Find(OtherBounds);
				PreciseOverlapAmount.Remove(OtherBounds);
				PreciseOverlapCount.Remove(OtherBounds);
			}
		}

		bool OverlapsWith(const FBounds* OtherBounds) const
		{
			FReadScopeLock ReadLock(OverlapLock);
			return Overlaps.Contains(OtherBounds);
		}

		void AddPreciseOverlap(FBounds* OtherBounds, const int32 InCount, const double InAmount)
		{
			if (OverlapsWith(OtherBounds)) { return; }

			{
				FWriteScopeLock WriteLock(OverlapLock);
				Overlaps.Add(OtherBounds);

				PreciseOverlapCount.Add(OtherBounds, InCount);
				PreciseOverlapAmount.Add(OtherBounds, InAmount);

				TotalPreciseOverlapCount += InCount;
				TotalPreciseOverlapAmount += InAmount;
			}

			OtherBounds->AddPreciseOverlap(this, InCount, InAmount);
		}

		~FBounds()
		{
			Overlaps.Empty();
			FastOverlaps.Empty();
			PreciseOverlapAmount.Empty();
			PreciseOverlapCount.Empty();
		}
	};

	static void ComputeBounds(
		FPCGExAsyncManager* Manager,
		PCGExData::FPointIOCollection* IOGroup,
		TArray<FBounds*>& OutBounds,
		const EPCGExPointBoundsSource BoundsSource)
	{
		for (PCGExData::FPointIO* PointIO : IOGroup->Pairs)
		{
			FBounds* Bounds = new FBounds(PointIO);
			OutBounds.Add(Bounds);
			Manager->Start<FPCGExComputeIOBounds>(PointIO->IOIndex, PointIO, BoundsSource, Bounds);
		}
	}

	static FBox GetBounds(const FPCGPoint& Point, const EPCGExPointBoundsSource Source)
	{
		switch (Source)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			return Point.GetDensityBounds().GetBox();
			break;
		case EPCGExPointBoundsSource::ScaledExtents:
			return FBoxCenterAndExtent(Point.Transform.GetLocation(), Point.GetScaledExtents()).GetBox();
			break;
		case EPCGExPointBoundsSource::Extents:
			return FBoxCenterAndExtent(Point.Transform.GetLocation(), Point.GetExtents()).GetBox();
			break;
		}
	}
}


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExPointsToBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPointsToBoundsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PointsToBounds, "Points to Bounds", "Merge points group to a single point representing their bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorMiscAdd; }
#endif

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
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Overlap overlap test mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledExtents;

	/** Bound point is the result of its contents */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bBlendProperties = false;

	/** Defines how fused point properties and attributes are merged into the final point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bBlendProperties"))
	FPCGExBlendingSettings BlendingSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::None);

	/** Write point counts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bWritePointsCount = false;

	/** Attribute to write points count to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWritePointsCount"))
	FName PointsCountAttributeName = NAME_None;

private:
	friend class FPCGExPointsToBoundsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsToBoundsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPointsToBoundsElement;

	virtual ~FPCGExPointsToBoundsContext() override;

	TArray<PCGExPointsToBounds::FBounds*> IOBounds;

	bool bWritePointsCount;

	PCGExDataBlending::FMetadataBlender* MetadataBlender;

	TArray<FPCGPoint>* OutPoints;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsToBoundsElement : public FPCGExPointsProcessorElementBase
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExComputeIOBounds : public FPCGExNonAbandonableTask
{
public:
	FPCGExComputeIOBounds(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                      const EPCGExPointBoundsSource InBoundsSource, PCGExPointsToBounds::FBounds* InBounds) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		BoundsSource(InBoundsSource), Bounds(InBounds)
	{
	}

	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledExtents;
	PCGExPointsToBounds::FBounds* Bounds = nullptr;

	virtual bool ExecuteTask() override;
};
