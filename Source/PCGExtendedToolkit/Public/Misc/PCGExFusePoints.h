// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExDebugManager.h"
#include "Data/PCGExAttributeHelpers.h"

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
EPCGExFuseMethod _NAME##FuseMethod = EPCGExFuseMethod::Skip;

#define PCGEX_FUSE_CONTEXT(_NAME)\
EPCGExFuseMethod _NAME##FuseMethod;

UENUM(BlueprintType)
enum class EPCGExFuseMethod : uint8
{
	Skip UMETA(DisplayName = "Skip", ToolTip="Uses the first point"),
	Average UMETA(DisplayName = "Average", ToolTip="Average data"),
	Min UMETA(DisplayName = "Min", ToolTip="TBD"),
	Max UMETA(DisplayName = "Max", ToolTip="TBD"),
	Weight UMETA(DisplayName = "Weight", ToolTip="TBD - Requires"),
	//Local attribute used as Weight
};

namespace PCGExFuse
{
	struct FFusedPoint
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

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInputDescriptorWithFuseMethod : public FPCGExInputDescriptor
{
	GENERATED_BODY()

	FPCGExInputDescriptorWithFuseMethod(): FPCGExInputDescriptor()
	{
	}

	FPCGExInputDescriptorWithFuseMethod(const FPCGExInputDescriptorWithFuseMethod& Other)
		: FPCGExInputDescriptor(Other),
		  FuseMethod(Other.FuseMethod)
	{
	}

public:
	/** Sub-component order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Fuse Method", PCG_Overridable, DisplayAfter="Selector"))
	EPCGExFuseMethod FuseMethod = EPCGExFuseMethod::Average;

	~FPCGExInputDescriptorWithFuseMethod()
	{
	}
};


UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExFusePointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer);
	void RefreshFuseMethodHiddenNames();

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FusePoints, "Fuse Points", "Fuse points based on distance.");
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

public:
	/** Deterministic fusing is not multi-threaded, hence will run MUCH MORE slowly. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bDeterministic = false;
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFuseMethod FuseMethod = EPCGExFuseMethod::Average;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bComponentWiseRadius = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="!bComponentWiseRadius", EditConditionHides))
	double Radius = 10;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bComponentWiseRadius", EditConditionHides))
	FVector Radiuses = FVector(10);

	/** Attribute used to read weight from, as a double. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExInputDescriptorWithSingleField WeightAttribute;

#pragma region Property overrides

	//PCGEX_FUSE_FOREACH_POINTPROPERTYNAME(PCGEX_FUSE_UPROPERTY)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideDensity = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Density", EditCondition="bOverrideDensity"))
	EPCGExFuseMethod DensityFuseMethod = EPCGExFuseMethod::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideExtents = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Extents", EditCondition="bOverrideExtents"))
	EPCGExFuseMethod ExtentsFuseMethod = EPCGExFuseMethod::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideColor = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Color", EditCondition="bOverrideColor"))
	EPCGExFuseMethod ColorFuseMethod = EPCGExFuseMethod::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverridePosition = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Position", EditCondition="bOverridePosition"))
	EPCGExFuseMethod PositionFuseMethod = EPCGExFuseMethod::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideRotation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Rotation", EditCondition="bOverrideRotation"))
	EPCGExFuseMethod RotationFuseMethod = EPCGExFuseMethod::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Scale", EditCondition="bOverrideScale"))
	EPCGExFuseMethod ScaleFuseMethod = EPCGExFuseMethod::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideSteepness = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Steepness", EditCondition="bOverrideSteepness"))
	EPCGExFuseMethod SteepnessFuseMethod = EPCGExFuseMethod::Average;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (InlineEditConditionToggle))
	bool bOverrideSeed = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (DisplayName="Seed", EditCondition="bOverrideSeed"))
	EPCGExFuseMethod SeedFuseMethod = EPCGExFuseMethod::Average;

#pragma endregion

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Overrides", meta = (TitleProperty="{FuseMethod} {HiddenDisplayName}"))
	TArray<FPCGExInputDescriptorWithFuseMethod> FuseMethodOverrides;

private:
	friend class FPCGExFusePointsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFusePointsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExFusePointsElement;

public:
	bool bDeterministic;
	int32 CurrentIndex = 0;
	EPCGExFuseMethod FuseMethod;
	double Radius;
	bool bComponentWiseRadius;
	FVector Radiuses;

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
