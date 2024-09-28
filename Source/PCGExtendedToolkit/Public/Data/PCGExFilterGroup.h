// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExData.h"
#include "PCGExFactoryProvider.h"

#include "PCGExPointFilter.h"

#include "Graph/Filters/PCGExClusterFilter.h"
#include "PCGExFilterGroup.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Filter Group Mode"))
enum class EPCGExFilterGroupMode : uint8
{
	AND = 0 UMETA(DisplayName = "And", ToolTip="All connected filters must pass."),
	OR  = 1 UMETA(DisplayName = "Or", ToolTip="Only a single connected filter must pass.")
};

/**
 * 
 */
UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFilterGroupFactoryBase : public UPCGExClusterFilterFactoryBase
{
	GENERATED_BODY()

public:
	bool bInvert = false;
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> FilterFactories;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterGroup; }
	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const override { return nullptr; }
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFilterGroupFactoryBaseAND : public UPCGExFilterGroupFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterGroup; }
	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const override;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFilterGroupFactoryBaseOR : public UPCGExFilterGroupFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterGroup; }
	virtual TSharedPtr<PCGExPointFilter::TFilter> CreateFilter() const override;
};

namespace PCGExFilterGroup
{
	class /*PCGEXTENDEDTOOLKIT_API*/ TFilterGroup : public PCGExClusterFilter::TFilter
	{
	public:
		explicit TFilterGroup(const UPCGExFilterGroupFactoryBase* InFactory, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFilterFactories):
			TFilter(InFactory), GroupFactory(InFactory), ManagedFactories(InFilterFactories)
		{
		}

		bool bValid = false;
		bool bInvert = false;
		const UPCGExFilterGroupFactoryBase* GroupFactory;
		const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* ManagedFactories;

		TSharedPtr<PCGExCluster::FCluster> Cluster;
		TSharedPtr<PCGExData::FFacade> EdgeDataCache;


		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Group; }

		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		virtual bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade) override;

		virtual void PostInit() override;

		virtual bool Test(const int32 Index) const override = 0;
		virtual bool Test(const PCGExCluster::FNode& Node) const override = 0;
		virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const override = 0;

	protected:
		TArray<TSharedPtr<PCGExPointFilter::TFilter>> ManagedFilters;

		virtual bool InitManaged(const FPCGContext* InContext);
		bool InitManagedFilter(const FPCGContext* InContext, const TSharedPtr<PCGExPointFilter::TFilter>& Filter) const;
		virtual bool PostInitManaged(const FPCGContext* InContext);
		virtual void PostInitManagedFilter(const FPCGContext* InContext, const TSharedPtr<PCGExPointFilter::TFilter>& InFilter);
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TFilterGroupAND : public TFilterGroup
	{
	public:
		explicit TFilterGroupAND(const UPCGExFilterGroupFactoryBase* InFactory, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFilterFactories):
			TFilterGroup(InFactory, InFilterFactories)
		{
		}

		FORCEINLINE virtual bool Test(const int32 Index) const override
		{
			for (const TSharedPtr<PCGExPointFilter::TFilter>& Filter : ManagedFilters) { if (!Filter->Test(Index)) { return bInvert; } }
			return !bInvert;
		}

		FORCEINLINE virtual bool Test(const PCGExCluster::FNode& Node) const override
		{
			for (const TSharedPtr<PCGExPointFilter::TFilter>& Filter : ManagedFilters) { if (!Filter->Test(Node)) { return bInvert; } }
			return !bInvert;
		}

		FORCEINLINE virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const override
		{
			for (const TSharedPtr<PCGExPointFilter::TFilter>& Filter : ManagedFilters) { if (!Filter->Test(Edge)) { return bInvert; } }
			return !bInvert;
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ TFilterGroupOR : public TFilterGroup
	{
	public:
		explicit TFilterGroupOR(const UPCGExFilterGroupFactoryBase* InFactory, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFilterFactories):
			TFilterGroup(InFactory, InFilterFactories)
		{
		}

		FORCEINLINE virtual bool Test(const int32 Index) const override
		{
			for (const TSharedPtr<PCGExPointFilter::TFilter>& Filter : ManagedFilters) { if (Filter->Test(Index)) { return !bInvert; } }
			return bInvert;
		}

		FORCEINLINE virtual bool Test(const PCGExCluster::FNode& Node) const override
		{
			for (const TSharedPtr<PCGExPointFilter::TFilter>& Filter : ManagedFilters) { if (Filter->Test(Node)) { return !bInvert; } }
			return bInvert;
		}

		FORCEINLINE virtual bool Test(const PCGExGraph::FIndexedEdge& Edge) const override
		{
			for (const TSharedPtr<PCGExPointFilter::TFilter>& Filter : ManagedFilters) { if (Filter->Test(Edge)) { return !bInvert; } }
			return bInvert;
		}
	};
}
