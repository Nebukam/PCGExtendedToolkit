﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExPointFilter.h"


#include "PCGExShrinkPath.generated.h"

UENUM()
enum class EPCGExPathShrinkMode : uint8
{
	Count    = 0 UMETA(DisplayName = "Count", ToolTip="TBD"),
	Distance = 1 UMETA(DisplayName = "Distance", ToolTip="TBD."),
};

UENUM()
enum class EPCGExPathShrinkDistanceCutType : uint8
{
	NewPoint = 0 UMETA(DisplayName = "New Point", ToolTip="TBD"),
	Previous = 1 UMETA(DisplayName = "Previous (Ceil)", ToolTip="TBD."),
	Next     = 2 UMETA(DisplayName = "Next (Floor)", ToolTip="TBD."),
	Closest  = 3 UMETA(DisplayName = "Closest (Round)", ToolTip="TBD."),
};

UENUM()
enum class EPCGExShrinkEndpoint : uint8
{
	Both  = 0 UMETA(DisplayName = "Start and End", ToolTip="TBD"),
	Start = 1 UMETA(DisplayName = "Start", ToolTip="TBD."),
	End   = 2 UMETA(DisplayName = "End", ToolTip="TBD."),
};

UENUM()
enum class EPCGExShrinkConstantMode : uint8
{
	Shared   = 0 UMETA(DisplayName = "Shared", ToolTip="Both start & end distance use the primary value."),
	Separate = 1 UMETA(DisplayName = "Separate", ToolTip="Start will use the primary value, end will use the secondary value..")
};

USTRUCT(BlueprintType)
struct FPCGExShrinkPathEndpointDistanceDetails
{
	GENERATED_BODY()

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType AmountInput = EPCGExInputValueType::Constant;

	/** Distance or count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Distance (Attr)", EditCondition="AmountInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector DistanceAttribute;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Distance", EditCondition="AmountInput == EPCGExInputValueType::Constant", EditConditionHides))
	double Distance = 10;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPathShrinkDistanceCutType CutType = EPCGExPathShrinkDistanceCutType::NewPoint;

	bool SanityCheck(const FPCGContext* Context) const
	{
		if (AmountInput == EPCGExInputValueType::Attribute) { PCGEX_VALIDATE_NAME_C(Context, DistanceAttribute.GetName()) }
		return true;
	}
};

USTRUCT(BlueprintType)
struct FPCGExShrinkPathEndpointCountDetails
{
	GENERATED_BODY()

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType ValueSource = EPCGExInputValueType::Constant;

	/** Distance or count */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Count (Attr)", EditCondition="ValueSource != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector CountAttribute;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Count", EditCondition="ValueSource == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 Count = 10;

	bool SanityCheck(const FPCGContext* Context) const
	{
		if (ValueSource == EPCGExInputValueType::Attribute) { PCGEX_VALIDATE_NAME_C(Context, CountAttribute.GetName()) }
		return true;
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(Keywords = "reduce extend", PCGExNodeLibraryDoc="paths/shrink"))
class UPCGExShrinkPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	explicit UPCGExShrinkPathSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathShrink, "Path : Shrink", "Shrink path from its beginning and end.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

	//~Begin UPCGExPointsProcessorSettings
public:
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceStopConditionLabel, "", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides))
	EPCGExShrinkEndpoint ShrinkEndpoint = EPCGExShrinkEndpoint::Both;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkEndpoint == EPCGExShrinkEndpoint::Both", EditConditionHides))
	EPCGExShrinkConstantMode SettingsMode = EPCGExShrinkConstantMode::Shared;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPathShrinkMode ShrinkMode = EPCGExPathShrinkMode::Distance;


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode == EPCGExPathShrinkMode::Distance", EditConditionHides))
	FPCGExShrinkPathEndpointDistanceDetails PrimaryDistanceDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode == EPCGExPathShrinkMode::Distance && ShrinkEndpoint == EPCGExShrinkEndpoint::Both && SettingsMode == EPCGExShrinkConstantMode::Separate", EditConditionHides))
	FPCGExShrinkPathEndpointDistanceDetails SecondaryDistanceDetails;


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode == EPCGExPathShrinkMode::Count", EditConditionHides))
	FPCGExShrinkPathEndpointCountDetails PrimaryCountDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="ShrinkMode == EPCGExPathShrinkMode::Count && ShrinkEndpoint == EPCGExShrinkEndpoint::Both && SettingsMode == EPCGExShrinkConstantMode::Separate", EditConditionHides))
	FPCGExShrinkPathEndpointCountDetails SecondaryCountDetails;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bEndpointsIgnoreStopConditions = false;

	/** If enabled, the point cut from the start will inherit from the original first point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bPreserveFirstMetadata = false;

	/** If enabled, the point cut from the start will inherit from the original last point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bPreserveLastMetadata = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietClosedLoopWarning = false;
};

struct FPCGExShrinkPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExShrinkPathElement;

	void GetShrinkAmounts(
		const TSharedRef<PCGExData::FPointIO>& PointIO,
		double& Start, double& End,
		EPCGExPathShrinkDistanceCutType& StartCut, EPCGExPathShrinkDistanceCutType& EndCut) const;

	void GetShrinkAmounts(
		const TSharedRef<PCGExData::FPointIO>& PointIO,
		int32& Start, int32& End) const;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExShrinkPathElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ShrinkPath)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExShrinkPath
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExShrinkPathContext, UPCGExShrinkPathSettings>
	{
		TBitArray<> Mask;
		int32 NumPoints = 0;
		int32 LastPointIndex = 0;

		FPCGPoint NewStart;
		FPCGPoint NewEnd;

		bool bUnaltered = false;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
			DefaultPointFilterValue = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;

	protected:
		bool MaskIndex(const int32 Index);

		void ShrinkByCount();

		void UpdateCut(FPCGPoint& Point, const int32 FromIndex, const int32 ToIndex, const double Dist, const EPCGExPathShrinkDistanceCutType Cut);
		void ShrinkByDistance();
	};
}
