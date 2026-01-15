// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMatchAttrToAttr.h"
#include "Core/PCGExMatchRuleFactoryProvider.h"
#include "Details/PCGExInputShorthandsDetails.h"

#include "PCGExMatchOverlap.generated.h"

struct FPCGExTaggedData;

namespace PCGExData
{
	template <typename T>
	class TAttributeBroadcaster;
}

UENUM()
enum class EPCGExMatchOverlapExpansionMode : uint8
{
	None  = 0 UMETA(DisplayName = "None", Tooltip="Don't alter extents"),
	Add   = 1 UMETA(DisplayName = "Add", Tooltip="Add the value to the extents"),
	Scale = 2 UMETA(DisplayName = "Scale", Tooltip="Scale the data bounds"),
};

USTRUCT(BlueprintType)
struct FPCGExMatchOverlapConfig : public FPCGExMatchRuleConfigBase
{
	GENERATED_BODY()

	FPCGExMatchOverlapConfig()
		: FPCGExMatchRuleConfigBase()
	{
	}

	/** Amount but which the bounds should be shrinked */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMatchOverlapExpansionMode ExpansionMode = EPCGExMatchOverlapExpansionMode::None;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ExpansionMode != EPCGExMatchOverlapExpansionMode::None"))
	FPCGExInputShorthandNameVector Expansion = FPCGExInputShorthandNameVector(FName("@Data.Expansion"), FVector(0, 0, 0));
};

/**
 * 
 */
class FPCGExMatchOverlap : public FPCGExMatchRuleOperation
{
public:
	FPCGExMatchOverlapConfig Config;

	virtual bool PrepareForMatchableSources(FPCGExContext* InContext, const TSharedPtr<TArray<FPCGExTaggedData>>& InMatchableSources) override;

	virtual bool Test(const PCGExData::FConstPoint& InTargetElement, const FPCGExTaggedData& InCandidate, const PCGExMatching::FScope& InMatchingScope) const override;

protected:
	TArray<FBox> Bounds;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExMatchOverlapFactory : public UPCGExMatchRuleFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExMatchOverlapConfig Config;

	virtual bool WantsPoints() override;

	virtual TSharedPtr<FPCGExMatchRuleOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="DataMatch", meta=(PCGExNodeLibraryDoc="misc/data-matching/overlap"))
class UPCGExCreateMatchOverlapSettings : public UPCGExMatchRuleFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(MatchOverlap, "Match : Overlap", "Check if there is an overlap with the data bounds (AABB)", FName(GetDisplayName()))

#endif
	//~End UPCGSettings

	/** Rules properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExMatchOverlapConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

protected:
	virtual bool IsCacheable() const override { return true; }

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
