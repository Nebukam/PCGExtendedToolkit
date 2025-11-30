// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFactoryProvider.h"
#include "Data/PCGExPointFilter.h"
#include "Misc/Filters/PCGExFilterFactoryProvider.h"

#include "PCGExClusterFilter.generated.h"

namespace PCGExCluster
{
	class FCluster;
}

namespace PCGExGraph
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
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExClusterFilterFactoryData : public UPCGExPointFilterFactoryData
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
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExNodeFilterFactoryData : public UPCGExClusterFilterFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoFilterVtx)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterNode; }
};

UCLASS(Abstract)
class UPCGExVtxFilterProviderSettings : public UPCGExClusterFilterProviderSettings
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
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeFilterFactoryData : public UPCGExClusterFilterFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoFilterEdge)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterEdge; }
};

UCLASS(Abstract)
class UPCGExEdgeFilterProviderSettings : public UPCGExClusterFilterProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoFilterEdge)

	virtual FName GetMainOutputPin() const override;
};

#pragma endregion

namespace PCGExClusterFilter
{
	class PCGEXTENDEDTOOLKIT_API IFilter : public PCGExPointFilter::IFilter
	{
	public:
		explicit IFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory):
			PCGExPointFilter::IFilter(InFactory)
		{
		}

		bool bInitForCluster = false;
		TSharedPtr<PCGExCluster::FCluster> Cluster;
		TSharedPtr<PCGExData::FFacade> EdgeDataFacade;

		virtual PCGExFilters::EType GetFilterType() const override;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);
		virtual void PostInit() override;
	};

	class PCGEXTENDEDTOOLKIT_API IVtxFilter : public IFilter
	{
	public:
		explicit IVtxFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory):
			IFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Node; }
		virtual bool Test(const int32 Index) const override final;
		virtual bool Test(const PCGExCluster::FNode& Node) const override;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override final;
	};

	class PCGEXTENDEDTOOLKIT_API IEdgeFilter : public IFilter
	{
	public:
		explicit IEdgeFilter(const TObjectPtr<const UPCGExClusterFilterFactoryData>& InFactory):
			IFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Edge; }
		virtual bool Test(const int32 Index) const override final;
		virtual bool Test(const PCGExCluster::FNode& Node) const override final;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override;
	};

	class PCGEXTENDEDTOOLKIT_API FManager : public PCGExPointFilter::FManager
	{
	public:
		FManager(const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);

		TSharedRef<PCGExCluster::FCluster> Cluster;
		TSharedRef<PCGExData::FFacade> EdgeDataFacade;

		virtual ~FManager() override
		{
		}

	protected:
		virtual bool InitFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::IFilter>& Filter) override;
		virtual void InitCache() override;
	};
}
