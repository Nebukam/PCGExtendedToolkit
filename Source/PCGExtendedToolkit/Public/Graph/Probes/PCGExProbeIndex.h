// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExProbeFactoryProvider.h"
#include "PCGExProbeOperation.h"

#include "PCGExProbeIndex.generated.h"

namespace PCGExProbing
{
	struct FCandidate;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Mean Measure"))
enum class EPCGExProbeTargetMode : uint8
{
	Target UMETA(DisplayName = "Target", ToolTip="Target index is used as-is to create a connection"),
	OneWayOffset UMETA(DisplayName = "One-way Offset", ToolTip="Target index is used as an offset value from the current point' index"),
	TwoWayOffset UMETA(DisplayName = "Two-way Offset", ToolTip="Target index is used as both a positive and negative offset value from the current point' index"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExProbeConfigIndex : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigIndex() :
		FPCGExProbeConfigBase(false)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExProbeTargetMode Mode = EPCGExProbeTargetMode::Target;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Ignore;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFetchType TargetIndex = EPCGExFetchType::Constant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, EditCondition="TargetIndex==EPCGExFetchType::Constant", EditConditionHides))
	int32 TargetConstant = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="TargetIndex==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector TargetAttribute;
};

/**
 * 
 */
UCLASS(DisplayName = "Index")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeIndex : public UPCGExProbeOperation
{
	GENERATED_BODY()

public:
	virtual bool RequiresDirectProcessing() override;
	virtual bool PrepareForPoints(const PCGExData::FPointIO* InPointIO) override;
	virtual void ProcessNode(const int32 Index, const FPCGPoint& Point, TSet<uint64>* Stacks, const FVector& ST, TSet<uint64>* OutEdges) override;

	FPCGExProbeConfigIndex Config;
	PCGExData::FCache<int32>* TargetCache;

protected:
	int32 MaxIndex = -1;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeFactoryIndex : public UPCGExProbeFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExProbeConfigIndex Config;
	virtual UPCGExProbeOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExProbeIndexProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ProbeIndex, "Probe : Index", "Connects to a specific index, ignoring search radius.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigIndex Config;


#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
