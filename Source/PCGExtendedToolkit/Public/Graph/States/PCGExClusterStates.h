// Copyright 2025 Timothé Lapetite and contributors
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
struct PCGEXTENDEDTOOLKIT_API FPCGExClusterStateConfigBase : public FPCGExStateConfigBase
{
	GENERATED_BODY()

	FPCGExClusterStateConfigBase()
	{
	}
};

USTRUCT(/*PCG_DataType*/DisplayName="PCGEx | Cluster State")
struct FPCGExClusterStateDataTypeInfo : public FPCGExFactoryDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExClusterStateFactoryData : public UPCGExClusterFilterFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExClusterStateDataTypeInfo)
	
	UPROPERTY()
	FPCGExClusterStateConfigBase Config;

	UPROPERTY()
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> FilterFactories;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::NodeState; }
	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual void BeginDestroy() override;
};

namespace PCGExClusterStates
{
	class PCGEXTENDEDTOOLKIT_API FState final : public PCGExClusterFilter::IFilter
	{
	public:
		FPCGExClusterStateConfigBase Config;
		FPCGExStateConfigBase* BaseConfig = nullptr;
		const UPCGExClusterStateFactoryData* StateFactory = nullptr;

		explicit FState(const UPCGExClusterStateFactoryData* InFactory):
			IFilter(InFactory), StateFactory(InFactory)
		{
		}

		virtual ~FState() override;

		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;
		bool InitInternalManager(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories);

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override;

		void ProcessFlags(const bool bSuccess, int64& InFlags) const;

	protected:
		TSharedPtr<PCGExClusterFilter::FManager> Manager;
	};

	class PCGEXTENDEDTOOLKIT_API FStateManager final : public PCGExClusterFilter::FManager
	{
		TArray<TSharedPtr<FState>> States;
		TSharedPtr<TArray<int64>> FlagsCache;

	public:
		explicit FStateManager(
			const TSharedPtr<TArray<int64>>& InFlags,
			const TSharedRef<PCGExCluster::FCluster>& InCluster,
			const TSharedRef<PCGExData::FFacade>& InPointDataCache,
			const TSharedRef<PCGExData::FFacade>& InEdgeDataCache);

		virtual bool Test(const int32 Index) override
		{
			int64& Flags = *(FlagsCache->GetData() + Index);
			for (const TSharedPtr<FState>& State : States) { State->ProcessFlags(State->Test(Index), Flags); }
			return true;
		}

		virtual bool Test(const PCGExCluster::FNode& Node) override
		{
			int64& Flags = *(FlagsCache->GetData() + Node.PointIndex);
			for (const TSharedPtr<FState>& State : States) { State->ProcessFlags(State->Test(Node), Flags); }
			return true;
		}

		virtual bool Test(const PCGExGraph::FEdge& Edge) override
		{
			int64& Flags = *(FlagsCache->GetData() + Edge.PointIndex);
			for (const TSharedPtr<FState>& State : States) { State->ProcessFlags(State->Test(Edge), Flags); }
			return true;
		}

	protected:
		virtual void PostInitFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::IFilter>& InFilter) override;
	};
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="clusters/metadata/flag-nodes/node-flag"))
class PCGEXTENDEDTOOLKIT_API UPCGExClusterStateFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExClusterStateDataTypeInfo)
	
public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		ClusterNodeFlag, "Cluster : Node Flag", "A single, filter-driven node flag.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterState; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual FName GetMainOutputPin() const override { return PCGExCluster::OutputNodeFlagLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Name = FName("Node Flag");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable), AdvancedDisplay)
	int32 Priority = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExClusterStateConfigBase Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
