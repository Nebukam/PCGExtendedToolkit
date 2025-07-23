// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMatching.h"
#include "PCGExOperation.h"
#include "PCGExPointsProcessor.h"


#include "PCGExMatchRuleFactoryProvider.generated.h"

#define PCGEX_MATCH_RULE_BOILERPLATE(_RULE) \
TSharedPtr<FPCGExMatchRuleOperation> UPCGExMatch##_RULE##Factory::CreateOperation(FPCGExContext* InContext) const{ \
	PCGEX_FACTORY_NEW_OPERATION(Match##_RULE)\
	NewOperation->Config = Config; \
	NewOperation->Config.Init(); \
	return NewOperation; } \
UPCGExFactoryData* UPCGExCreateMatch##_RULE##Settings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{ \
	UPCGExMatch##_RULE##Factory* NewFactory = InContext->ManagedObjects->New<UPCGExMatch##_RULE##Factory>(); \
	NewFactory->BaseConfig = Config; \
	NewFactory->Config = Config; \
	return Super::CreateFactory(InContext, NewFactory);}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExMatchRuleConfigBase
{
	GENERATED_BODY()

	FPCGExMatchRuleConfigBase() = default;
	virtual ~FPCGExMatchRuleConfigBase() = default;

	/** Match Strictness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMatchStrictness Strictness = EPCGExMatchStrictness::Required;

	virtual void Init()
	{
	}
};

/**
 * 
 */
class PCGEXTENDEDTOOLKIT_API FPCGExMatchRuleOperation : public FPCGExOperation
{
public:
	virtual bool PrepareForTargets(FPCGExContext* InContext, const TSharedPtr<TArray<PCGExData::FTaggedData>>& InTargets);

	virtual bool Test(const PCGExData::FElement& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO) const
	PCGEX_NOT_IMPLEMENTED_RET(Test, false);

protected:
	TSharedPtr<TArray<PCGExData::FTaggedData>> Targets;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchRuleFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	FPCGExMatchRuleConfigBase BaseConfig;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::MatchRule; }
	virtual TSharedPtr<FPCGExMatchRuleOperation> CreateOperation(FPCGExContext* InContext) const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|DataMatch", meta=(PCGExNodeLibraryDoc="misc/data-matching/match-rule"))
class PCGEXTENDEDTOOLKIT_API UPCGExMatchRuleFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MatchRule, "Match Rule Definition", "Creates a single match rule node, to be used with nodes that support data matching.")
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::ControlFlow; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorMatch); }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExMatching::OutputMatchRuleLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};

namespace PCGExMatching
{
	class FDataMatcher : public TSharedFromThis<FDataMatcher>
	{
	protected:
		const FPCGExMatchingDetails* Details = nullptr;

		TSharedPtr<TArray<PCGExData::FTaggedData>> Targets;
		TSharedPtr<TArray<PCGExData::FElement>> Elements;
		TMap<const UPCGData*, int32> TargetsMap;
		TArray<TSharedPtr<FPCGExMatchRuleOperation>> Operations;

		TArray<TSharedPtr<FPCGExMatchRuleOperation>> RequiredOperations;
		TArray<TSharedPtr<FPCGExMatchRuleOperation>> OptionalOperations;

	public:
		EPCGExMapMatchMode MatchMode = EPCGExMapMatchMode::Disabled;

		FDataMatcher();

		void SetDetails(const FPCGExMatchingDetails* InDetails);

		bool Init(FPCGExContext* InContext, const TArray<const UPCGData*>& InTargetData, const TArray<TSharedPtr<PCGExData::FTags>>& InTags, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<TSharedPtr<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError);

		bool Test(const UPCGData* InTarget, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate) const;
		bool Test(const PCGExData::FElement& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate) const;

		void PopulateIgnoreList(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, TSet<const UPCGData*>& OutIgnoreList) const;
		int32 GetMatchingTargets(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, TArray<int32>& OutMatches) const;

	protected:
		void RegisterTaggedData(FPCGExContext* InContext, const PCGExData::FTaggedData& InTaggedData);
		bool InitInternal(FPCGExContext* InContext, const bool bThrowError);
	};
}
