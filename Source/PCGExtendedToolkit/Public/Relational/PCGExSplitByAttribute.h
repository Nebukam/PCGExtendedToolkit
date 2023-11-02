#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Data/PCGSpatialData.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExSplitByAttribute.generated.h"

namespace PCGExBucketEntry
{
	extern const FName SourceLabel;
	extern const FName TargetLabel;
}

struct PCGEXTENDEDTOOLKIT_API FPCGExBucketEntry
{
public:
	int ID = -1;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBucketSettings : public FPCGExSelectorSettingsBase
{
	GENERATED_BODY()

	FPCGExBucketSettings(): FPCGExSelectorSettingsBase()
	{
	}

	FPCGExBucketSettings(const FPCGExBucketSettings& Other): FPCGExSelectorSettingsBase(Other)
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
struct PCGEXTENDEDTOOLKIT_API FPCGExBucketProcessingData
{
	GENERATED_BODY()

public:
	FPCGContext* Context = nullptr;
	FPCGTaggedData* Source = nullptr;
	const UPCGPointData* InPointData = nullptr;

	const FPCGExBucketSettings* Settings = nullptr;

	TMap<int64, UPCGPointData*>* Buckets = nullptr;

	TArray<FPCGPoint>* TempPoints = nullptr;
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExSplitByAttribute : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SplitByAttribute")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("SplitByAttribute", "NodeTitle", "Split by Attribute"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:

	/** Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBucketSettings BucketSettings;

};

class FPCGExBucketEntryElement : public FPCGPointProcessingElementBase
{
public:

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	template <typename T>
	static void DistributePoint(const FPCGPoint& Point, const T& InValue, FPCGExBucketProcessingData* Data);

	static void AsyncPointAttributeProcessing(FPCGExBucketProcessingData* Data);
	static void AsyncPointPropertyProcessing(FPCGExBucketProcessingData* Data);
	static void AsyncPointExtraPropertyProcessing(FPCGExBucketProcessingData* Data);
};
