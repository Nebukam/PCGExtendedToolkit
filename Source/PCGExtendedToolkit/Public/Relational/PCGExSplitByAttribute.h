#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
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

	/** The name of the attribute to read unique identifier from. Note that floating point attribute types will be rounded. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName AttributeName = TEXT("GroupID");

	/** Tolerance to use when evaluating uniqueness of numerical attributes. 1 = 1:1 relationship; 2+ will 'group' values using a modulo operation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	int32 FilterSize = 1;

	/** Upscale value to multiply sampled values by. Since sampled values are rounded to ints, it can sometimes be useful to capture differences in floating point values.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Upscale = 1.0f;

	/** The name of the attribute to write the unique group ID into. This is used for caching bucket index.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	FName AttributeWrite = TEXT("PCGExUniqueBucketID");

};

class FPCGExBucketEntryElement : public FPCGPointProcessingElementBase
{

public:
	template<typename T>
	static int ProcessBucket(T BucketID, TArray<FPCGExBucketEntry>& BucketEntries, TMap<T, int32>& StringBucketsMap);
	
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	
	
};