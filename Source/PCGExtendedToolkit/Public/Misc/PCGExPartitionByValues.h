#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Data/PCGSpatialData.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExPartitionByValues.generated.h"

namespace PCGExPartitionByValues
{
	extern const FName SourceLabel;
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPartitioningRules : public FPCGExInputSelectorSettingsBase
{
	GENERATED_BODY()

	FPCGExPartitioningRules(): FPCGExInputSelectorSettingsBase()
	{
	}

	FPCGExPartitioningRules(const FPCGExPartitioningRules& Other): FPCGExInputSelectorSettingsBase(Other)
	{
		FilterSize = Other.FilterSize;
		Upscale = Other.Upscale;
	}

public:
	/** Filter Size. Higher values means fewer, larger groups. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double FilterSize = 1.0;

	/** Upscale multiplier, applied before filtering. Handy to deal with floating point values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Upscale = 1.0;
};

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExRelationalProcessingData
{
	GENERATED_BODY()

public:
	FPCGContext* Context = nullptr;
	FPCGTaggedData* Source = nullptr;
	const UPCGPointData** InPointData = nullptr;

	const FPCGExPartitioningRules* Rules = nullptr;

	TMap<int64, UPCGPointData*>* Partitions = nullptr;
	
	TArray<FPCGPoint>* PointsBuffer = nullptr;

	bool bWriteKeyToAttribute = false;
	FName AttributeName = NAME_None;
	TMap<int64, FPCGMetadataAttribute<int64>*>* OutAttributes = nullptr;
	
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPartitionByValuesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PartitonByValues")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PartitonByValues", "NodeTitle", "Partiton by Values"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Rules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPartitioningRules PartitioningRules;

	/** Whether to write the partition Key to an attribute. Useful for debugging. Note: They key is not the index, but instead the filtered value used to distribute into partitions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bWriteKeyToAttribute;

	/** Name of the int64 attribute to write the partition Key to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bWriteKeyToAttribute", EditConditionHides))
	FName KeyAttributeName = "PartitionID";
};

class PCGEXTENDEDTOOLKIT_API FPCGExPartitionByValuesElement : public FPCGPointProcessingElementBase
{
public:

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	template <typename T>
	static void DistributePoint(const FPCGPoint& Point, const T& InValue, FPCGExRelationalProcessingData* Data);

	static void AsyncPointAttributeProcessing(FPCGExRelationalProcessingData* Data);
	static void AsyncPointPropertyProcessing(FPCGExRelationalProcessingData* Data);
	static void AsyncPointExtraPropertyProcessing(FPCGExRelationalProcessingData* Data);
};
