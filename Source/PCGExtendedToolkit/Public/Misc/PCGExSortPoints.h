// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"


#include "PCGExSortPoints.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Sort Direction"))
enum class EPCGExSortDirection : uint8
{
	Ascending  = 0 UMETA(DisplayName = "Ascending"),
	Descending = 1 UMETA(DisplayName = "Descending")
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSortRuleConfig : public FPCGExInputConfig
{
	GENERATED_BODY()

	FPCGExSortRuleConfig()
	{
	}

	FPCGExSortRuleConfig(const FPCGExSortRuleConfig& Other)
		: FPCGExInputConfig(Other),
		  Tolerance(Other.Tolerance)
	{
	}

	/** Equality tolerance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double Tolerance = 0.0001f;

	/** Invert sorting direction on that rule. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bInvertRule = false;

	/** Compare absolute value. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	//bool bAbsolute = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSortRule
{
	FPCGExSortRule()
	{
	}

	TSharedPtr<PCGExData::TBuffer<double>> Cache;

	double Tolerance = 0.0001f;
	bool bInvertRule = false;
	bool bAbsolute = false;
};

UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSortPointsBaseSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	//~Begin UPCGSettings
protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Controls the order in which points will be ordered. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	virtual bool GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const;

private:
	friend class FPCGExSortPointsBaseElement;
};

UCLASS(MinimalAPI, BlueprintType, Hidden, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSortPointsSettings : public UPCGExSortPointsBaseSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SortPointsStatic, "Sort Points (Static)", "Sort the source points according to specific rules.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

	//~Begin UObject interface
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Ordered list of attribute to check to sort over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, TitleProperty="{TitlePropertyName}"))
	TArray<FPCGExSortRuleConfig> Rules = {FPCGExSortRuleConfig{}};

	virtual bool GetSortingRules(const FPCGContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const override;

private:
	friend class FPCGExSortPointsBaseElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSortPointsBaseElement final : public FPCGExPointsProcessorElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExSortPoints
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			FPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};
}
