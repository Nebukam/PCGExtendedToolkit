// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "Blending/PCGExDataBlending.h"
#include "UObject/Object.h"

#include "PCGExData.h"
#include "PCGExPointFilter.h"


#include "Graph/PCGExCluster.h"

#include "PCGExPointStates.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExStateConfigBase
{
	GENERATED_BODY()

	FPCGExStateConfigBase()
	{
	}

	FName StateName = NAME_None;
	int32 StateId = 0;

	/** Flags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOnTestPass = true;

	/** Operations executed on the flag if all filters pass */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOnTestPass"))
	FPCGExBitmaskWithOperation PassStateFlags;

	/** Flags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOnTestFail = true;

	/** Operations executed on the flag if any filters fail */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOnTestFail"))
	FPCGExBitmaskWithOperation FailStateFlags;

	void Init()
	{
	}
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPointStateFactoryData : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> FilterFactories;
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;

	virtual void BeginDestroy() override;
};

namespace PCGExPointStates
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FState final : public PCGExPointFilter::FFilter
	{
	public:
		FPCGExStateConfigBase* BaseConfig = nullptr;
		const UPCGExPointStateFactoryData* StateFactory = nullptr;

		explicit FState(const TObjectPtr<const UPCGExPointStateFactoryData>& InFactory):
			FFilter(InFactory), StateFactory(InFactory)
		{
		}

		virtual ~FState() override;


		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		bool InitInternalManager(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories);
		virtual bool Test(const int32 Index) const override;

		void ProcessFlags(const bool bSuccess, int64& InFlags) const;

	protected:
		TSharedPtr<PCGExPointFilter::FManager> Manager;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FStateManager final : public PCGExPointFilter::FManager
	{
		TArray<TSharedPtr<FState>> States;
		TSharedPtr<TArray<int64>> FlagsCache;

	public:
		explicit FStateManager(const TSharedPtr<TArray<int64>>& InFlags, const TSharedRef<PCGExData::FFacade>& InPointDataFacade);

		virtual bool Test(const int32 Index) override;

	protected:
		virtual void PostInitFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::FFilter>& InFilter) override;
	};
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPointStateFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PointStateDefinition, "Abstract Point State Definition", "Base class for state factory management.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterState; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExCluster::OutputNodeFlagLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
