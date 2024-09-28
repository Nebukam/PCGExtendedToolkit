// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "UObject/Object.h"
#include "CoreMinimal.h"
#include "Data/PCGExPointStates.h"


#include "Graph/Filters/PCGExClusterFilter.h"
#include "Misc/PCGExBitmaskMerge.h"
#include "Graph/PCGExCluster.h"
#include "PCGExClusterStates.generated.h"

namespace PCGExNodeFlags
{
	const FName OutputOnPassBitmaskLabel = TEXT("BitmaskPass");
	const FName OutputOnFailBitmaskLabel = TEXT("BitmaskFail");
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExClusterStateConfigBase : public FPCGExStateConfigBase
{
	GENERATED_BODY()

	FPCGExClusterStateConfigBase()
	{
	}
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExClusterStateFactoryBase : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::StateNode; }

	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> FilterFactories;
	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const override;

	FPCGExClusterStateConfigBase Config;

	virtual void BeginDestroy() override;
};

namespace PCGExClusterStates
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FState : public PCGExClusterFilter::TFilter
	{
	public:
		FPCGExClusterStateConfigBase Config;
		FPCGExStateConfigBase* BaseConfig = nullptr;
		const UPCGExClusterStateFactoryBase* StateFactory = nullptr;

		explicit FState(const UPCGExClusterStateFactoryBase* InFactory):
			TFilter(InFactory), StateFactory(InFactory)
		{
		}

		virtual ~FState() override;

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade) override;
		virtual bool InitInternalManager(const FPCGContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>& InFactories);

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const override;

		void ProcessFlags(const bool bSuccess, int64& InFlags) const;

	protected:
		TUniquePtr<PCGExClusterFilter::TManager> Manager;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FStateManager : public PCGExClusterFilter::TManager
	{
		TArray<TSharedPtr<FState>> States;
		TSharedPtr<TArray<int64>> FlagsCache;

	public:
		explicit FStateManager(
			const TSharedPtr<TArray<int64>>& InFlags,
			const TSharedPtr<PCGExCluster::FCluster>& InCluster,
			const TSharedPtr<PCGExData::FFacade>& InPointDataCache,
			const TSharedPtr<PCGExData::FFacade>& InEdgeDataCache);

		FORCEINLINE virtual bool Test(const int32 Index) override
		{
			int64& Flags = *(FlagsCache->GetData() + Index);
			for (const TSharedPtr<FState>& State : States) { State->ProcessFlags(State->Test(Index), Flags); }
			return true;
		}

		FORCEINLINE virtual bool Test(const PCGExCluster::FNode& Node) override
		{
			int64& Flags = *(FlagsCache->GetData() + Node.PointIndex);
			for (const TSharedPtr<FState>& State : States) { State->ProcessFlags(State->Test(Node), Flags); }
			return true;
		}

		FORCEINLINE virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) override
		{
			int64& Flags = *(FlagsCache->GetData() + Edge.PointIndex);
			for (const TSharedPtr<FState>& State : States) { State->ProcessFlags(State->Test(Edge), Flags); }
			return true;
		}

	protected:
		virtual void PostInitFilter(const FPCGContext* InContext, const TSharedPtr<PCGExPointFilter::TFilter>& InFilter) override;
	};
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExClusterStateFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NodeFilter, "Cluster : Node Flag", "A single, filter-driven node flag.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterState; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual FName GetMainOutputLabel() const override { return PCGExCluster::OutputNodeFlagLabel; }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Name = FName("Node Flag");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 Priority = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExClusterStateConfigBase Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
