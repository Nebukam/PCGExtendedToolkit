// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "UObject/Object.h"
#include "CoreMinimal.h"
#include "Data/PCGExPointStates.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "..\..\Misc\PCGExBitmaskOperation.h"
#include "PCGExClusterStates.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExClusterStateDescriptorBase : public FPCGExStateDescriptorBase
{
	GENERATED_BODY()

	FPCGExClusterStateDescriptorBase()
	{
	}
};

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExClusterStateFactoryBase : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:

	virtual PCGExFactories::EType GetFactoryType() const override;
	
	TArray<UPCGExFilterFactoryBase*> FilterFactories;
	virtual PCGExPointFilter::TFilter* CreateFilter() const override;

	FPCGExClusterStateDescriptorBase Descriptor;

	virtual void BeginDestroy() override;
};

namespace PCGExClusterStates
{
	class PCGEXTENDEDTOOLKIT_API FState : public PCGExClusterFilter::TFilter
	{
	public:
		FPCGExClusterStateDescriptorBase Descriptor;
		FPCGExStateDescriptorBase* BaseDescriptor = nullptr;
		const UPCGExClusterStateFactoryBase* StateFactory = nullptr;

		explicit FState(const UPCGExClusterStateFactoryBase* InFactory):
			TFilter(InFactory), StateFactory(InFactory)
		{
		}

		virtual ~FState() override;

		virtual bool Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExDataCaching::FPool* InPointDataCache, PCGExDataCaching::FPool* InEdgeDataCache) override;
		virtual bool InitInternalManager(const FPCGContext* InContext, const TArray<UPCGExFilterFactoryBase*>& InFactories);

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const override;

		void ProcessFlags(const bool bSuccess, int64& InFlags) const;

	protected:
		PCGExClusterFilter::TManager* Manager = nullptr;
	};
	
	class PCGEXTENDEDTOOLKIT_API FStateManager : public PCGExClusterFilter::TManager
	{
		TArray<FState*> States;
		TArray<int64>* FlagsCache = nullptr;

	public:
		explicit FStateManager(TArray<int64>* InFlags, PCGExCluster::FCluster* InCluster, PCGExDataCaching::FPool* InPointDataCache, PCGExDataCaching::FPool* InEdgeDataCache);
		virtual ~FStateManager() override;

		virtual bool Test(const int32 Index) override;
		virtual bool Test(const PCGExCluster::FNode& Node) override;
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) override;
		
	protected:
		virtual void PostInitFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* InFilter) override;
	};
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExClusterStateFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(NodeFilter, "Graph : Node Flag", "A single, filter-driven node flag.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterState; }
#endif
	//~End UPCGSettings

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	virtual FName GetMainOutputLabel() const override;
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Name = FName("Node State");
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 Priority = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExClusterStateDescriptorBase Descriptor;

	virtual FString GetDisplayName() const override;
	
};
