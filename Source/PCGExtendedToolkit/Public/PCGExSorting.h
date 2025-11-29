// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExFactoryProvider.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExSorting.generated.h"

namespace PCGExData
{
	class IBufferProxy;
}

UENUM()
enum class EPCGExSortDirection : uint8
{
	Ascending  = 0 UMETA(DisplayName = "Ascending", ToolTip = "Ascending", ActionIcon="Ascending"),
	Descending = 1 UMETA(DisplayName = "Descending", ToolTip = "Descending", ActionIcon="Descending")
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSortRuleConfig : public FPCGExInputConfig
{
	GENERATED_BODY()

	FPCGExSortRuleConfig() = default;
	FPCGExSortRuleConfig(const FPCGExSortRuleConfig& Other);


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

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExCollectionSortingDetails
{
	GENERATED_BODY()

	FPCGExCollectionSortingDetails() = default;
	explicit FPCGExCollectionSortingDetails(const bool InEnabled);

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
	void Sort(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InCollection) const;
};

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Sort Rule"))
struct FPCGExDataTypeInfoSortRule : public FPCGExFactoryDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExSortingRule : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoSortRule)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::RuleSort; }

	int32 Priority;
	FPCGExSortRuleConfig Config;

	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExSortingRuleProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoSortRule)

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		SortingRuleFactory, "Sorting Rule", "Creates an single sorting rule to be used with the Sort Points node.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override;
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1), AdvancedDisplay)
	int32 Priority = 0;

	/** Rule Config */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSortRuleConfig Config;
};

namespace PCGExSorting
{
	const FName SourceSortingRules = TEXT("SortRules");

	PCGEXTENDEDTOOLKIT_API
	void DeclareSortingRulesInputs(TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus);

	class PCGEXTENDEDTOOLKIT_API FRuleHandler : public TSharedFromThis<FRuleHandler>
	{
	public:
		FRuleHandler() = default;

		explicit FRuleHandler(const FPCGExSortRuleConfig& Config):
			Selector(Config.Selector),
			Tolerance(Config.Tolerance),
			bInvertRule(Config.bInvertRule)
		{
		}

		TSharedPtr<PCGExData::IBufferProxy> Buffer;
		TArray<TSharedPtr<PCGExData::IBufferProxy>> Buffers;
		TArray<TSharedPtr<PCGExData::IDataValue>> DataValues;

		FPCGAttributePropertyInputSelector Selector;

		double Tolerance = DBL_COMPARE_TOLERANCE;
		bool bInvertRule = false;
		bool bAbsolute = false;
	};

	class PCGEXTENDEDTOOLKIT_API FPointSorter : public TSharedFromThis<FPointSorter>
	{
	protected:
		FPCGExContext* ExecutionContext = nullptr;
		TArray<TSharedPtr<FRuleHandler>> RuleHandlers;
		TMap<uint32, int32> IdxMap;

	public:
		EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;
		TSharedPtr<PCGExData::FFacade> DataFacade;

		FPointSorter(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade, TArray<FPCGExSortRuleConfig> InRuleConfigs);
		explicit FPointSorter(TArray<FPCGExSortRuleConfig> InRuleConfigs);

		bool Init(FPCGExContext* InContext);
		bool Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InDataFacades);
		bool Init(FPCGExContext* InContext, const TArray<FPCGTaggedData>& InTaggedDatas);

		bool Sort(const int32 A, const int32 B);
		bool Sort(const PCGExData::FElement A, const PCGExData::FElement B);
		bool SortData(const int32 A, const int32 B);
	};

	PCGEXTENDEDTOOLKIT_API
	TArray<FPCGExSortRuleConfig> GetSortingRules(FPCGExContext* InContext, const FName InLabel);
}

#undef PCGEX_UNSUPPORTED_STRING_TYPES
#undef PCGEX_UNSUPPORTED_PATH_TYPES
