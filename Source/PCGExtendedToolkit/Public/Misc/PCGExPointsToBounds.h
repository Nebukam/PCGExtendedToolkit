// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#include "PCGExPointsToBounds.generated.h"

class FPCGExComputeIOBounds;

namespace PCGExPointsToBounds
{
	class FComputeIOBoundsTask;

	class /*PCGEXTENDEDTOOLKIT_API*/ FBounds : public TSharedFromThis<FBounds>
	{
	public:
		FBox Bounds = FBox(ForceInit);
		const TSharedPtr<PCGExData::FPointIO> PointIO;

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

		explicit FBounds(const TSharedPtr<PCGExData::FPointIO>& InPointIO):
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

		~FBounds() = default;
	};

	static FBox GetBounds(const FPCGPoint& Point, const EPCGExPointBoundsSource Source)
	{
		switch (Source)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			return Point.GetDensityBounds().GetBox();
		case EPCGExPointBoundsSource::ScaledBounds:
			return FBoxCenterAndExtent(Point.Transform.GetLocation(), Point.GetScaledExtents()).GetBox();
		case EPCGExPointBoundsSource::Bounds:
			return FBoxCenterAndExtent(Point.Transform.GetLocation(), Point.GetExtents()).GetBox();
		}
	}
}


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPointsToBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PointsToBounds, "Points to Bounds", "Merge points group to a single point representing their bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Overlap overlap test mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** Bound point is the result of its contents */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bBlendProperties = false;

	/** Defines how fused point properties and attributes are merged into the final point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bBlendProperties"))
	FPCGExBlendingDetails BlendingSettings = FPCGExBlendingDetails(EPCGExDataBlendingType::None);

	/** Write point counts */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bWritePointsCount = false;

	/** Attribute to write points count to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bWritePointsCount"))
	FName PointsCountAttributeName = NAME_None;

private:
	friend class FPCGExPointsToBoundsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointsToBoundsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPointsToBoundsElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointsToBoundsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPointsToBounds
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FComputeIOBoundsTask final : public PCGExMT::FPCGExTask
	{
	public:
		FComputeIOBoundsTask(const EPCGExPointBoundsSource InBoundsSource, const TSharedPtr<FBounds>& InBounds) :
			FPCGExTask(),
			BoundsSource(InBoundsSource),
			Bounds(InBounds)
		{
		}

		EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;
		TSharedPtr<FBounds> Bounds;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InGroup) override;
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPointsToBoundsContext, UPCGExPointsToBoundsSettings>
	{
		TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;
		TSharedPtr<FBounds> Bounds;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};
}

namespace PCGExPointsToBounds
{
	static void ComputeBounds(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const TSharedPtr<PCGExData::FPointIOCollection>& IOGroup,
		TArray<TSharedPtr<FBounds>>& OutBounds,
		const EPCGExPointBoundsSource BoundsSource)
	{
		for (const TSharedPtr<PCGExData::FPointIO>& PointIO : IOGroup->Pairs)
		{
			PCGEX_MAKE_SHARED(Bounds, FBounds, PointIO)
			OutBounds.Add(Bounds);
			PCGEX_START_TASK(FComputeIOBoundsTask, BoundsSource, Bounds)
		}
	}
}
