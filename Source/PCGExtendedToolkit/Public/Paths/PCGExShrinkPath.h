// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExShrinkPath.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Shrink Mode"))
enum class EPCGExPathShrinkMode : uint8
{
	Count    = 0 UMETA(DisplayName = "Count", ToolTip="TBD"),
	Distance = 1 UMETA(DisplayName = "Distance", ToolTip="TBD."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Shrink Distance Cut Type"))
enum class EPCGExPathShrinkDistanceCutType : uint8
{
	NewPoint = 0 UMETA(DisplayName = "New Point", ToolTip="TBD"),
	Previous = 1 UMETA(DisplayName = "Previous (Ceil)", ToolTip="TBD."),
	Next     = 2 UMETA(DisplayName = "Next (Floor)", ToolTip="TBD."),
	Closest  = 3 UMETA(DisplayName = "Closest (Round)", ToolTip="TBD."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Shrink Endpoint"))
enum class EPCGExShrinkEndpoint : uint8
{
	Both  = 0 UMETA(DisplayName = "Start and End", ToolTip="TBD"),
	Start = 1 UMETA(DisplayName = "Start", ToolTip="TBD."),
	End   = 2 UMETA(DisplayName = "End", ToolTip="TBD."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Shrink Constant Mode"))
enum class EPCGExShrinkConstantMode : uint8
{
	Shared   = 0 UMETA(DisplayName = "Shared", ToolTip="Both start & end distance use the primary value."),
	Separate = 1 UMETA(DisplayName = "Separate", ToolTip="Start will use the primary value, end will use the secondary value..")
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShrinkPathEndpointDistanceDetails
{
	GENERATED_BODY()

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType ValueSource = EPCGExFetchType::Constant;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ValueSource==EPCGExFetchType::Constant", EditConditionHides))
	double Distance = 10;

	/** Distance or count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ValueSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DistanceAttribute;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPathShrinkDistanceCutType CutType = EPCGExPathShrinkDistanceCutType::NewPoint;

	bool SanityCheck(const FPCGContext* Context) const
	{
		if (ValueSource == EPCGExFetchType::Attribute) { PCGEX_VALIDATE_NAME_C(Context, DistanceAttribute.GetName()) }
		return true;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShrinkPathEndpointCountDetails
{
	GENERATED_BODY()

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType ValueSource = EPCGExFetchType::Constant;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ValueSource==EPCGExFetchType::Constant", EditConditionHides, ClampMin=1))
	int32 Count = 10;

	/** Distance or count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ValueSource==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector CountAttribute;

	bool SanityCheck(const FPCGContext* Context) const
	{
		if (ValueSource == EPCGExFetchType::Attribute) { PCGEX_VALIDATE_NAME_C(Context, CountAttribute.GetName()) }
		return true;
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExShrinkPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExShrinkPathSettings(const FObjectInitializer& ObjectInitializer);
	
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ShrinkPath, "Path : Shrink", "Shrink path from its beginning and end.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(FName("Stop Conditions"), "", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

public:
	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides))
	EPCGExShrinkEndpoint ShrinkEndpoint = EPCGExShrinkEndpoint::Both;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkEndpoint==EPCGExShrinkEndpoint::Both", EditConditionHides))
	EPCGExShrinkConstantMode SettingsMode = EPCGExShrinkConstantMode::Shared;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPathShrinkMode ShrinkMode = EPCGExPathShrinkMode::Distance;


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode==EPCGExPathShrinkMode::Distance", EditConditionHides))
	FPCGExShrinkPathEndpointDistanceDetails PrimaryDistanceDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode==EPCGExPathShrinkMode::Distance && ShrinkEndpoint==EPCGExShrinkEndpoint::Both && SettingsMode==EPCGExShrinkConstantMode::Separate", EditConditionHides))
	FPCGExShrinkPathEndpointDistanceDetails SecondaryDistanceDetails;


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode==EPCGExPathShrinkMode::Count", EditConditionHides))
	FPCGExShrinkPathEndpointCountDetails PrimaryCountDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode==EPCGExPathShrinkMode::Count && ShrinkEndpoint==EPCGExShrinkEndpoint::Both && SettingsMode==EPCGExShrinkConstantMode::Separate", EditConditionHides))
	FPCGExShrinkPathEndpointCountDetails SecondaryCountDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bEndpointsIgnoreStopConditions = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkEndpoint==EPCGExShrinkEndpoint::Both", EditConditionHides))
	EPCGExShrinkEndpoint ShrinkFirst = EPCGExShrinkEndpoint::Both;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShrinkPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExShrinkPathElement;

	void GetShrinkAmounts(const PCGExData::FPointIO* PointIO, double& Start, double& End, EPCGExPathShrinkDistanceCutType& StartCut, EPCGExPathShrinkDistanceCutType& EndCut) const;
	void GetShrinkAmounts(const PCGExData::FPointIO* PointIO, uint32& Start, uint32& End) const;

	virtual ~FPCGExShrinkPathContext() override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShrinkPathElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExShrinkPath
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
			DefaultPointFilterValue = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
	};
}
