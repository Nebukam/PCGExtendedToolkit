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
EPCGExMetadataBlendingOperationType _NAME##Blending = EPCGExMetadataBlendingOperationType::Skip;

#define PCGEX_FUSE_CONTEXT(_NAME)\
EPCGExMetadataBlendingOperationType _NAME##Blending;

namespace PCGExFuse
{
	constexpr PCGExMT::AsyncState State_FindingRootPoints = 100;
	constexpr PCGExMT::AsyncState State_MergingPoints = 101;

	struct PCGEXTENDEDTOOLKIT_API FFusedPoint
	{
		int32 MainIndex = -1;
		FVector Position = FVector::ZeroVector;
		TArray<int32> Fused;
		TArray<double> Distances;
		double MaxDistance = 0;

		FFusedPoint()
		{
			Fused.Empty();
			Distances.Empty();
		}

		void Add(int32 Index, double Distance)
		{
			Fused.Add(Index);
			Distances.Add(Distance);
			MaxDistance = FMath::Max(MaxDistance, Distance);
		}
	};

}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExFusePointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer);

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FusePoints, "Fuse Points", "Fuse points based on distance.");
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGExPointIO::EInit GetPointOutputInitMode() const override;

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExMetadataBlendingOperationType Blending = EPCGExMetadataBlendingOperationType::Average;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bComponentWiseRadius = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="!bComponentWiseRadius", EditConditionHides))
	double Radius = 10;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bComponentWiseRadius", EditConditionHides))
	FVector Radiuses = FVector(10);

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Blending==EPCGExMetadataBlendingOperationType::Weight", EditConditionHides))
	bool bUseLocalWeight = false;

	/** Attribute used to read weight from, as a double. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseLocalWeight && Blending==EPCGExMetadataBlendingOperationType::Weight", EditConditionHides))
	FPCGExInputDescriptorWithSingleField WeightAttribute;

#pragma region Property overrides

	//PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(PCGEX_FUSE_UPROPERTY)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideDensity = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Density", EditCondition="bOverrideDensity"))
	EPCGExMetadataBlendingOperationType DensityBlending = EPCGExMetadataBlendingOperationType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideExtents = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Extents", EditCondition="bOverrideExtents"))
	EPCGExMetadataBlendingOperationType ExtentsBlending = EPCGExMetadataBlendingOperationType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideColor = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Color", EditCondition="bOverrideColor"))
	EPCGExMetadataBlendingOperationType ColorBlending = EPCGExMetadataBlendingOperationType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverridePosition = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Position", EditCondition="bOverridePosition"))
	EPCGExMetadataBlendingOperationType PositionBlending = EPCGExMetadataBlendingOperationType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideRotation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Rotation", EditCondition="bOverrideRotation"))
	EPCGExMetadataBlendingOperationType RotationBlending = EPCGExMetadataBlendingOperationType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Scale", EditCondition="bOverrideScale"))
	EPCGExMetadataBlendingOperationType ScaleBlending = EPCGExMetadataBlendingOperationType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideSteepness = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Steepness", EditCondition="bOverrideSteepness"))
	EPCGExMetadataBlendingOperationType SteepnessBlending = EPCGExMetadataBlendingOperationType::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideSeed = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Seed", EditCondition="bOverrideSeed"))
	EPCGExMetadataBlendingOperationType SeedBlending = EPCGExMetadataBlendingOperationType::Average;

#pragma endregion

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (TitleProperty="{Blending} {TitlePropertyName}"))
	TMap<FName, EPCGExMetadataBlendingOperationType> BlendingOverrides;

private:
	friend class FPCGExFusePointsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFusePointsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExFusePointsElement;

public:
	int32 CurrentIndex = 0;
	double Radius;
	bool bComponentWiseRadius;
	FVector Radiuses;

	TMap<FName, EPCGExMetadataBlendingOperationType> BlendingOverrides;
	UPCGExMetadataBlender* Blender;
	
	TArray<PCGExFuse::FFusedPoint> FusedPoints;
	TArray<FPCGPoint>* OutPoints;
	
	PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(PCGEX_FUSE_CONTEXT)

	mutable FRWLock PointsLock;

};

class PCGEXTENDEDTOOLKIT_API FPCGExFusePointsElement : public FPCGExPointsProcessorElementBase
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

#undef PCGEX_FUSE_FOREACH_POINTPROPERTYNAME
#undef PCGEX_FUSE_UPROPERTY
#undef PCGEX_FUSE_CONTEXT
