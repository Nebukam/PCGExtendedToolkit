// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "PCGExMatchSharedTag.generated.h"

struct FPCGExTaggedData;

namespace PCGExData
{
	class FTags;
	template <typename T>
	class TAttributeBroadcaster;
}

UENUM()
enum class EPCGExTagMatchMode : uint8
{
	Specific  = 0 UMETA(DisplayName = "Specific Tag", ToolTip="Match a specific tag by name"),
	AnyShared = 1 UMETA(DisplayName = "Any Shared", ToolTip="Match if ANY tag is shared between data"),
	AllShared = 2 UMETA(DisplayName = "All Shared", ToolTip="Match if ALL tags from candidate exist in target"),
};

USTRUCT(BlueprintType)
struct FPCGExMatchSharedTagConfig : public FPCGExMatchRuleConfigBase
{
	GENERATED_BODY()

	FPCGExMatchSharedTagConfig()
		: FPCGExMatchRuleConfigBase()
	{
	}

	/** Tag matching mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExTagMatchMode Mode = EPCGExTagMatchMode::Specific;

	/** Type of Tag Name value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExTagMatchMode::Specific", EditConditionHides))
	EPCGExInputValueType TagNameInput = EPCGExInputValueType::Constant;

	/** Attribute to read tag name value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Tag Name (Attr)", EditCondition="Mode == EPCGExTagMatchMode::Specific && TagNameInput != EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FName TagNameAttribute = FName("ReadTagFrom");

	/** Constant tag name value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Tag Name", EditCondition="Mode == EPCGExTagMatchMode::Specific && TagNameInput == EPCGExInputValueType::Constant", EditConditionHides, HideEditConditionToggle))
	FString TagName = TEXT("Tag");

	/** Whether to do a tag value match or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Mode == EPCGExTagMatchMode::Specific", EditConditionHides))
	bool bDoValueMatch = false;

	/** Whether to also match tag values when checking shared tags. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Mode != EPCGExTagMatchMode::Specific", EditConditionHides))
	bool bMatchTagValues = false;

	virtual void Init() override;
};

/**
 * 
 */
class FPCGExMatchSharedTag : public FPCGExMatchRuleOperation
{
public:
	FPCGExMatchSharedTagConfig Config;

	virtual bool PrepareForMatchableSources(FPCGExContext* InContext, const TSharedPtr<TArray<FPCGExTaggedData>>& InMatchableSources) override;

	virtual bool Test(const PCGExData::FConstPoint& InTargetElement, const FPCGExTaggedData& InCandidate, const PCGExMatching::FScope& InMatchingScope) const override;

protected:
	TArray<TSharedPtr<PCGExData::TAttributeBroadcaster<FString>>> TagNameGetters;
	TArray<TWeakPtr<PCGExData::FTags>> Tags;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExMatchSharedTagFactory : public UPCGExMatchRuleFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExMatchSharedTagConfig Config;

	virtual bool WantsPoints() override;

	virtual TSharedPtr<FPCGExMatchRuleOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="DataMatch", meta=(PCGExNodeLibraryDoc="misc/data-matching/tags-attributes"))
class UPCGExCreateMatchSharedTagSettings : public UPCGExMatchRuleFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(MatchSharedTag, "Match : Shared Tag", "Match data that share common tags", FName(GetDisplayName()))

#endif
	//~End UPCGSettings

	/** Rules properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExMatchSharedTagConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
