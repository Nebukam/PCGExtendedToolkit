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


namespace PCGExMatching
{
	class FMatchingScope;
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExMatchRuleConfigBase
{
	GENERATED_BODY()

	FPCGExMatchRuleConfigBase() = default;
	virtual ~FPCGExMatchRuleConfigBase() = default;

	/** Match Strictness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	EPCGExMatchStrictness Strictness = EPCGExMatchStrictness::Any;

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

	virtual bool Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& PointIO, const PCGExMatching::FMatchingScope& InMatchingScope) const
	PCGEX_NOT_IMPLEMENTED_RET(Test, false);

protected:
	TSharedPtr<TArray<PCGExData::FTaggedData>> Targets;
};

USTRUCT()
struct FPCGExMatchRuleDataTypeInfo : public FPCGExFactoryDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchRuleFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExMatchRuleDataTypeInfo)
	
	FPCGExMatchRuleConfigBase BaseConfig;

	virtual bool WantsPoints() { return false; }

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::MatchRule; }
	virtual TSharedPtr<FPCGExMatchRuleOperation> CreateOperation(FPCGExContext* InContext) const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|DataMatch", meta=(PCGExNodeLibraryDoc="misc/data-matching/match-rule"))
class PCGEXTENDEDTOOLKIT_API UPCGExMatchRuleFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExMatchRuleDataTypeInfo)
	
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
	class FMatchingScope
	{
		int32 NumCandidates = 0;
		int32 Counter = 0;
		int8 Valid = true;

	public:
		FMatchingScope() = default;
		explicit FMatchingScope(const int32 InNumCandidates, const bool bUnlimited = false);

		void RegisterMatch();
		FORCEINLINE int32 GetNumCandidates() const { return NumCandidates; }
		FORCEINLINE int32 GetCounter() const { return FPlatformAtomics::AtomicRead(&Counter); }
		FORCEINLINE bool IsValid() const { return static_cast<bool>(FPlatformAtomics::AtomicRead(&Valid)); }

		void Invalidate();
	};

	class FDataMatcher : public TSharedFromThis<FDataMatcher>
	{
	protected:
		const FPCGExMatchingDetails* Details = nullptr;

		TSharedPtr<TArray<PCGExData::FTaggedData>> Targets;
		TSharedPtr<TArray<PCGExData::FConstPoint>> Elements;
		TMap<const UPCGData*, int32> TargetsMap;
		TArray<TSharedPtr<FPCGExMatchRuleOperation>> Operations;

		TArray<TSharedPtr<FPCGExMatchRuleOperation>> RequiredOperations;
		TArray<TSharedPtr<FPCGExMatchRuleOperation>> OptionalOperations;

	public:
		EPCGExMapMatchMode MatchMode = EPCGExMapMatchMode::Disabled;

		FDataMatcher();

		bool FindIndex(const UPCGData* InData, int32& OutIndex) const;

		void SetDetails(const FPCGExMatchingDetails* InDetails);

		bool Init(FPCGExContext* InContext, const TArray<const UPCGData*>& InTargetData, const TArray<TSharedPtr<PCGExData::FTags>>& InTags, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<TSharedPtr<PCGExData::FFacade>>& InTargetFacades, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TArray<PCGExData::FTaggedData>& InTargetDatas, const bool bThrowError);
		bool Init(FPCGExContext* InContext, const TSharedPtr<FDataMatcher>& InOtherMatcher, const FName InFactoriesLabel, const bool bThrowError);

		bool Test(const UPCGData* InTarget, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope) const;
		bool Test(const PCGExData::FConstPoint& InTargetElement, const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope) const;

		bool PopulateIgnoreList(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope, TSet<const UPCGData*>& OutIgnoreList) const;
		int32 GetMatchingTargets(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate, FMatchingScope& InMatchingScope, TArray<int32>& OutMatches) const;

		bool HandleUnmatchedOutput(const TSharedPtr<PCGExData::FFacade>& InFacade, const bool bForward = true) const;

	protected:
		int32 GetMatchLimitFor(const TSharedPtr<PCGExData::FPointIO>& InDataCandidate) const;
		void RegisterTaggedData(FPCGExContext* InContext, const PCGExData::FTaggedData& InTaggedData);
		bool InitInternal(FPCGExContext* InContext, const FName InFactoriesLabel, const bool bThrowError);
	};
}
