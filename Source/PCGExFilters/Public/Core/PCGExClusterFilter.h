// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExPointFilter.h"
#include "Core/PCGExFilterFactoryProvider.h"

#include "PCGExClusterFilter.generated.h"

namespace PCGExClusters
{
	class FCluster;
}

namespace PCGExGraphs
{
	struct FEdge;
}

namespace PCGExPointFilter
{
	class IFilter;
}

#pragma region Cluster Filter

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Filter (Cluster)"))
struct FPCGExDataTypeInfoFilterCluster : public FPCGExDataTypeInfoFilterPoint
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXFILTERS_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXFILTERS_API UPCGExClusterFilterFactoryData : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoFilterCluster)

	virtual bool SupportsCollectionEvaluation() const override { return false; }
};

UCLASS(Abstract)
class UPCGExClusterFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoFilterCluster)
};

#pragma endregion

#pragma region Vtx/Node Filter

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Filter (Cluster Vtx)"))
struct FPCGExDataTypeInfoFilterVtx : public FPCGExDataTypeInfoFilterCluster
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXFILTERS_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXFILTERS_API UPCGExNodeFilterFactoryData : public UPCGExClusterFilterFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoFilterVtx)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterNode; }
};

UCLASS(Abstract)
class PCGEXFILTERS_API UPCGExVtxFilterProviderSettings : public UPCGExClusterFilterProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoFilterVtx)

	virtual FName GetMainOutputPin() const override;
};

#pragma endregion

#pragma region Edge Filter

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Filter (Cluster Edges)"))
struct FPCGExDataTypeInfoFilterEdge : public FPCGExDataTypeInfoFilterCluster
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXFILTERS_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXFILTERS_API UPCGExEdgeFilterFactoryData : public UPCGExClusterFilterFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoFilterEdge)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterEdge; }
};

UCLASS(Abstract)
class PCGEXFILTERS_API UPCGExEdgeFilterProviderSettings : public UPCGExClusterFilterProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoFilterEdge)

	virtual FName GetMainOutputPin() const override;
};

#pragma endregion

namespace PCGExClusterFilter
{
	class PCGEXFILTERS_API IFilter : public PCGExPointFilter::IFilter
	{
	public:
		explicit IFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory)
			: PCGExPointFilter::IFilter(InFactory)
		{
		}

		bool bInitForCluster = false;
		TSharedPtr<PCGExClusters::FCluster> Cluster;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		virtual PCGExFilters::EType GetFilterType() const override;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExClusters::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);
		virtual void PostInit() override;
	};

	class PCGEXFILTERS_API IVtxFilter : public IFilter
	{
	public:
		explicit IVtxFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory)
			: IFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Node; }
		virtual bool Test(const int32 Index) const override final;
		virtual bool Test(const PCGExClusters::FNode& Node) const override;
		virtual bool Test(const PCGExGraphs::FEdge& Edge) const override final;
	};

	class PCGEXFILTERS_API IEdgeFilter : public IFilter
	{
	public:
		explicit IEdgeFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory)
			: IFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Edge; }
		virtual bool Test(const int32 Index) const override final;
		virtual bool Test(const PCGExClusters::FNode& Node) const override final;
		virtual bool Test(const PCGExGraphs::FEdge& Edge) const override;
	};

	class PCGEXFILTERS_API FManager : public PCGExPointFilter::FManager
	{
	public:
		FManager(const TSharedRef<PCGExClusters::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);

		TSharedRef<PCGExClusters::FCluster> Cluster;
		TSharedRef<PCGExData::FFacade> EdgeDataFacade;

		virtual ~FManager() override
		{
		}

	protected:
		virtual bool InitFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::IFilter>& Filter) override;
		virtual void InitCache() override;
	};
}
