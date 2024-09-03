// Copyright Timothé Lapetite 2024
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

	PCGEX_ASYNC_STATE(State_ComputeBounds)

	struct /*PCGEXTENDEDTOOLKIT_API*/ FBounds
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
		PCGExMT::FTaskManager* Manager,
		PCGExData::FPointIOCollection* IOGroup,
		TArray<FBounds*>& OutBounds,
		const EPCGExPointBoundsSource BoundsSource)
	{
		for (PCGExData::FPointIO* PointIO : IOGroup->Pairs)
		{
			FBounds* Bounds = new FBounds(PointIO);
			OutBounds.Add(Bounds);
			Manager->Start<FComputeIOBoundsTask>(PointIO->IOIndex, PointIO, BoundsSource, Bounds);
		}
	}

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
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
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

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointsToBoundsContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExPointsToBoundsElement;

	virtual ~FPCGExPointsToBoundsContext() override;
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
		FComputeIOBoundsTask(PCGExData::FPointIO* InPointIO,
		                     const EPCGExPointBoundsSource InBoundsSource, FBounds* InBounds) :
			FPCGExTask(InPointIO),
			BoundsSource(InBoundsSource), Bounds(InBounds)
		{
		}

		EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;
		FBounds* Bounds = nullptr;

		virtual bool ExecuteTask() override;
	};

	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGExDataBlending::FMetadataBlender* MetadataBlender = nullptr;
		FBounds* Bounds = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
	};
}
