// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExFusePoints.generated.h"

#define PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(MACRO)\
MACRO(Density) \
MACRO(Extents) \
MACRO(Color) \
MACRO(Position) \
MACRO(Rotation)\
MACRO(Scale) \
MACRO(Steepness) \
MACRO(Seed)

#define PCGEX_FUSE_UPROPERTY(_NAME)\
UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))\
bool bOverride##_NAME = false;\
UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (EditCondition="bOverride" #_NAME))\
EPCGExDataBlendingType _NAME##Blending = EPCGExDataBlendingType::Skip;

#define PCGEX_FUSE_CONTEXT(_NAME)\
EPCGExDataBlendingType _NAME##Blending;

namespace PCGExFuse
{
	constexpr PCGExMT::AsyncState State_FindingFusePoints = __COUNTER__;
	constexpr PCGExMT::AsyncState State_MergingPoints = __COUNTER__;

	struct PCGEXTENDEDTOOLKIT_API FFusedPoint
	{
		mutable FRWLock IndicesLock;
		int32 Index = -1;
		FVector Position = FVector::ZeroVector;
		TArray<int32> Fused;
		TArray<double> Distances;
		double MaxDistance = 0;

		FFusedPoint(const int32 InIndex, const FVector& InPosition)
			: Index(InIndex), Position(InPosition)
		{
			Fused.Empty();
			Distances.Empty();
		}

		~FFusedPoint()
		{
			Fused.Empty();
			Distances.Empty();
		}

		void Add(const int32 InIndex, const double Distance)
		{
			FWriteScopeLock WriteLock(IndicesLock);
			Fused.Add(InIndex);
			Distances.Add(Distance);
			MaxDistance = FMath::Max(MaxDistance, Distance);
		}
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExFusePointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FusePoints, "Fuse Points", "Fuse points based on distance.");
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
	/** Uses a per-axis radius, manathan-style */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bComponentWiseRadius = false;

	/** Fuse radius */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="!bComponentWiseRadius", EditConditionHides, ClampMin=0.001))
	double Radius = 10;

	/** Component-wise radiuses */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bComponentWiseRadius", EditConditionHides))
	FVector Radiuses = FVector(10);

	/** Preserve the order of input points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bPreserveOrder = true;

	/** Defines how fused point properties and attributes are merged together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExBlendingSettings BlendingSettings;

private:
	friend class FPCGExFusePointsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFusePointsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExFusePointsElement;

	virtual ~FPCGExFusePointsContext() override;

	bool bPreserveOrder;

	PCGExDataBlending::FMetadataBlender* MetadataBlender;

	double Radius = 0;

	TArray<PCGExFuse::FFusedPoint> FusedPoints;
	TArray<FPCGPoint>* OutPoints;

	mutable FRWLock PointsLock;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFusePointsElement : public FPCGExPointsProcessorElementBase
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

#undef PCGEX_FUSE_FOREACH_POINTPROPERTYNAME
#undef PCGEX_FUSE_UPROPERTY
#undef PCGEX_FUSE_CONTEXT
