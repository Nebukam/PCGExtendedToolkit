// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExData.h"
#include "PCGExFactoryProvider.h"

#include "PCGExSorting.generated.h"

UENUM()
enum class EPCGExSortDirection : uint8
{
	Ascending  = 0 UMETA(DisplayName = "Ascending"),
	Descending = 1 UMETA(DisplayName = "Descending")
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSortRuleConfig : public FPCGExInputConfig
{
	GENERATED_BODY()

	FPCGExSortRuleConfig()
	{
	}

	FPCGExSortRuleConfig(const FPCGExSortRuleConfig& Other)
		: FPCGExInputConfig(Other),
		  Tolerance(Other.Tolerance),
		  bInvertRule(Other.bInvertRule)
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

struct PCGEXTENDEDTOOLKIT_API FPCGExSortRule
{
	FPCGExSortRule()
	{
	}

	explicit FPCGExSortRule(const FPCGExSortRuleConfig& Config):
		Selector(Config.Selector),
		Tolerance(Config.Tolerance),
		bInvertRule(Config.bInvertRule)
	{
	}

	TSharedPtr<PCGExData::TBuffer<double>> Cache;
	TSharedPtr<PCGEx::TAttributeBroadcaster<double>> SoftCache;

	FPCGAttributePropertyInputSelector Selector;
	double Tolerance = DBL_COMPARE_TOLERANCE;
	bool bInvertRule = false;
	bool bAbsolute = false;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExCollectionSortingDetails
{
	GENERATED_BODY()

	FPCGExCollectionSortingDetails()
	{
	}

	explicit FPCGExCollectionSortingDetails(const bool InEnabled)
	{
		bEnabled = InEnabled;
	}

	/** Whether this collection sorting is enabled or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bEnabled = false;

	/** Sorting direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSortDirection Direction = EPCGExSortDirection::Ascending;

	/** Tag which value will be used for sorting; i.e MyTag:0, MyTag:1, MyTag:3 etc. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName TagName = FName("Tag");

	/** Multiplied applied to original order when tag is missing. Use -1/1 to choose whether these data should be put before or after the valid ones. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallback", meta=(PCG_Overridable))
	double FallbackOrderOffset = 0;

	/** Multiplied applied to original order when tag is missing. Use -1/1 to choose whether these data should be put before or after the valid ones. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallback", meta=(PCG_Overridable))
	double FallbackOrderMultiplier = 1;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallback")
	bool bQuietMissingTagWarning = false;

	bool Init(const FPCGContext* InContext);
	void Sort(const FPCGContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InCollection) const;
};


/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExSortingRule : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::RuleSort; }

	int32 Priority;
	FPCGExSortRuleConfig Config;

	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExSortingRuleProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		SortingRuleFactory, "Sorting Rule", "Creates an single sorting rule to be used with the Sort Points node.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif
	//~End UPCGSettings

	//~Begin UPCGExFactoryProviderSettings
	virtual FName GetMainOutputPin() const override { return FName("SortingRule"); }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
	//~End UPCGExFactoryProviderSettings

	/** Filter Priority.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority;

	/** Rule Config */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSortRuleConfig Config;
};

namespace PCGExSorting
{
	const FName SourceSortingRules = TEXT("SortRules");

	template <bool bUsePointIndices = false, bool bSoftMode = false>
	class PCGEXTENDEDTOOLKIT_API PointSorter : public TSharedFromThis<PointSorter<bUsePointIndices, bSoftMode>>
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
				InDataFacade->Source->PrintOutInKeysMap(PointIndices);
			}

			const UPCGData* InData = InDataFacade->Source->GetIn();
			FName Consumable = NAME_None;

			for (const FPCGExSortRuleConfig& RuleConfig : InRuleConfigs)
			{
				PCGEX_MAKE_SHARED(NewRule, FPCGExSortRule, RuleConfig)
				Rules.Add(NewRule.ToSharedRef());

				if (InContext->bCleanupConsumableAttributes && InData) { PCGEX_CONSUMABLE_SELECTOR(RuleConfig.Selector, Consumable) }
			}
		}

		void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
		{
			if constexpr (!bSoftMode) { for (const TSharedRef<FPCGExSortRule>& Rule : Rules) { FacadePreloader.Register<double>(ExecutionContext, Rule->Selector); } }
		}

		bool Init()
		{
			if constexpr (bSoftMode)
			{
				for (int i = 0; i < Rules.Num(); i++)
				{
					const TSharedPtr<FPCGExSortRule> Rule = Rules[i];
					PCGEX_MAKE_SHARED(SoftCache, PCGEx::TAttributeBroadcaster<double>)

					if (!SoftCache->Prepare(Rule->Selector, DataFacade->Source))
					{
						Rules.RemoveAt(i);
						i--;

						PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some points are missing attributes used for sorting."));
						continue;
					}

					Rule->SoftCache = SoftCache;
				}
			}
			else
			{
				for (int i = 0; i < Rules.Num(); i++)
				{
					const TSharedPtr<FPCGExSortRule> Rule = Rules[i];
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
			}


			return !Rules.IsEmpty();
		}

		bool Sort(const int32 A, const int32 B)
		{
			if constexpr (bSoftMode)
			{
				int Result = 0;
				for (const TSharedRef<FPCGExSortRule>& Rule : Rules)
				{
					const double ValueA = Rule->SoftCache->SoftGet(DataFacade->Source->GetInPointRef(A), 0);
					const double ValueB = Rule->SoftCache->SoftGet(DataFacade->Source->GetInPointRef(B), 0);
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
			else
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
		}

		bool Sort(const FPCGPoint& A, const FPCGPoint& B)
		{
			return Sort(PointIndices[A.MetadataEntry], PointIndices[B.MetadataEntry]);
		}
	};

	PCGEXTENDEDTOOLKIT_API
	TArray<FPCGExSortRuleConfig> GetSortingRules(FPCGExContext* InContext, const FName InLabel);

	PCGEXTENDEDTOOLKIT_API
	void PrepareRulesAttributeBuffers(FPCGExContext* InContext, const FName InLabel, PCGExData::FFacadePreloader& FacadePreloader);

	PCGEXTENDEDTOOLKIT_API
	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<FPCGExSortRuleConfig>& InRuleConfigs);
}

#undef PCGEX_UNSUPPORTED_STRING_TYPES
#undef PCGEX_UNSUPPORTED_PATH_TYPES
