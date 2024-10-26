// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExConstants.h"
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
	double Tolerance = DBL_COMPARE_TOLERANCE;

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

	FPCGAttributePropertyInputSelector Selector;
	double Tolerance = DBL_COMPARE_TOLERANCE;
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

	virtual bool GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const;

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

	virtual bool GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const override;

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
	const FName SourceSortingRules = TEXT("SortRules");

	template <bool bUsePointIndices = false>
	class PointSorter : public TSharedFromThis<PointSorter<bUsePointIndices>>
	{
	protected:
		FPCGExContext* ExecutionContext = nullptr;
		TArray<TSharedRef<FPCGExSortRule>> Rules;
		TMap<PCGMetadataEntryKey, int32> PointIndices;

	public:
		EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;
		TSharedRef<PCGExData::FFacade> DataFacade;

		explicit PointSorter(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade, TArray<FPCGExSortRuleConfig> InRuleConfigs)
			: ExecutionContext(InContext), DataFacade(InDataFacade)
		{
			if constexpr (bUsePointIndices)
			{
				InDataFacade->Source->PrintOutKeysMap(PointIndices);
			}

			for (const FPCGExSortRuleConfig& RuleConfig : InRuleConfigs)
			{
				TSharedPtr<FPCGExSortRule> NewRule = MakeShared<FPCGExSortRule>();
				NewRule->Selector = RuleConfig.Selector;
				NewRule->Tolerance = RuleConfig.Tolerance;
				NewRule->bInvertRule = RuleConfig.bInvertRule;
				Rules.Add(NewRule.ToSharedRef());
			}
		}

		void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
		{
			for (const TSharedRef<FPCGExSortRule> Rule : Rules) { FacadePreloader.Register<double>(ExecutionContext, Rule->Selector); }
		}

		bool Init()
		{
			for (int i = 0; i < Rules.Num(); i++)
			{
				TSharedPtr<FPCGExSortRule> Rule = Rules[i];
				const TSharedPtr<PCGExData::TBuffer<double>> Cache = DataFacade->GetBroadcaster<double>(Rule->Selector);

				if (!Cache)
				{
					Rules.RemoveAt(i);
					i--;

					PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some points are missing attributes used for sorting."));
					continue;
				}

				Rule->Cache = Cache;
			}

			return !Rules.IsEmpty();
		}

		FORCEINLINE bool Sort(const int32 A, const int32 B)
		{
			int Result = 0;
			for (const TSharedRef<FPCGExSortRule>& Rule : Rules)
			{
				const double ValueA = Rule->Cache->Read(A);
				const double ValueB = Rule->Cache->Read(B);
				Result = FMath::IsNearlyEqual(ValueA, ValueB, Rule->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
				if (Result != 0)
				{
					if (Rule->bInvertRule) { Result *= -1; }
					break;
				}
			}

			if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
			return Result < 0;
		}

		FORCEINLINE bool Sort(const FPCGPoint& A, const FPCGPoint& B)
		{
			return Sort(PointIndices[A.MetadataEntry], PointIndices[B.MetadataEntry]);
		}
	};

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPointsProcessorContext, UPCGExSortPointsBaseSettings>
	{
		TSharedPtr<PointSorter<true>> Sorter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};
}
