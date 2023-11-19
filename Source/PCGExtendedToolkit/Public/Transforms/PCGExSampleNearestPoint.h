// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSampleNearestPoint.generated.h"

UENUM(BlueprintType)
enum class EPCGExSampleMethod : uint8
{
	TargetsWithinRange UMETA(DisplayName = "All Targets Within Range", ToolTip="TBD"),
	AllTargets  UMETA(DisplayName = "All Targets", ToolTip="TBD"),
	ClosestTarget UMETA(DisplayName = "Closest Target", ToolTip="TBD"),
	FarthestTarget UMETA(DisplayName = "Farthest Target", ToolTip="TBD"),
};

UENUM(BlueprintType)
enum class EPCGExWeightMethod : uint8
{
	NormalizedTargetDistance  UMETA(DisplayName = "Normalized Target Distance", ToolTip="TBD"),
	RemappedNormalizedTargetDistance  UMETA(DisplayName = "Normalized Target Distance", ToolTip="TBD"),
};

namespace PCGExNearestPoint
{
	struct PCGEXTENDEDTOOLKIT_API FTargetInfos
	{
		FTargetInfos()
		{
		}

		double Distance = 0;
		double Weight = 0;
		int32 Index = -1;
		PCGMetadataEntryKey EntryKey = PCGInvalidEntryKey;
	};

	struct PCGEXTENDEDTOOLKIT_API FTargetsCompoundInfos
	{
		FTargetsCompoundInfos()
		{
		}

		int32 NumTargets = 0;
		double TotalWeight = 0;
		
	};
	
}

/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multithread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNearestPointSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNearestPoint, "Sample Nearest Point", "Find the closest point on the nearest collidable surface.");
#endif

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling")
	EPCGExSampleMethod SampleMethod = EPCGExSampleMethod::TargetsWithinRange;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(InlineEditConditionToggle, EditCondition="SampleMethod==EPCGExSampleMethod::TargetsWithinRange"))
	bool bUseLocalRange = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(EditCondition="bUseLocalRange && SampleMethod==EPCGExSampleMethod::TargetsWithinRange", EditConditionHides))
	FPCGExInputDescriptorWithSingleField LocalRange;
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteLocation = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteLocation"))
	FName Location = FName("NearestSurfaceLocation");

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteDirection = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDirection"))
	FName Direction = FName("DirectionToNearestSurface");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteNormal"))
	FName Normal = FName("NearestSurfaceNormal");


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(InlineEditConditionToggle))
	bool bWriteDistance = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bWriteDistance"))
	FName Distance = FName("NearestSurfaceDistance");


	/** Maximum distance to check for closest surface. Input 0 to sample all target points.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Collision & Metrics")
	double MaxDistance = 1000;

};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPointContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSampleNearestPointElement;

public:
	UPCGPointData* Targets = nullptr;
	UPCGPointData::PointOctree* Octree = nullptr;

	double MaxDistance = 1000;
	bool bUseOctree = false;
	int64 NumTargets = 0;

	//TODO: Setup target local inputs
	
	PCGEX_OUT_ATTRIBUTE(Location, FVector)
	PCGEX_OUT_ATTRIBUTE(Direction, FVector)
	PCGEX_OUT_ATTRIBUTE(Normal, FVector)
	PCGEX_OUT_ATTRIBUTE(Distance, double)
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNearestPointElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;
	virtual bool Validate(FPCGContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
