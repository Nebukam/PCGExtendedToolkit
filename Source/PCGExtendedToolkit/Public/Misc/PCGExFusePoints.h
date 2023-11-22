// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExCompare.h"
#include "PCGExPointsProcessor.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExFusePoints.generated.h"

UENUM(BlueprintType)
enum class EPCGExFuseMethod : uint8
{
	Average UMETA(DisplayName = "Average", ToolTip="Average data"),
	Min UMETA(DisplayName = "Min", ToolTip="TBD"),
	Max UMETA(DisplayName = "Max", ToolTip="TBD"),
	Centroid UMETA(DisplayName = "Centroid Weight", ToolTip="TBD"),
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

		FFusedPoint()
		{
			Fused.Empty();
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

	virtual PCGEx::EIOInit GetPointOutputInitMode() const override;

public:
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
	
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (TitleProperty="{FuseMethod} {HiddenDisplayName}"))
	TArray<FPCGExInputDescriptorWithFuseMethod> FuseMethodOverrides;
	
private:
	friend class FPCGExFusePointsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFusePointsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExFusePointsElement;

public:
	EPCGExFuseMethod FuseMethod;
	double Radius;
	bool bComponentWiseRadius;
	FVector Radiuses;
	
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
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
