// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExData.h"
#include "PCGExFactoryProvider.h"

#include "PCGExPointFilter.h"


#include "Graph/Filters/PCGExClusterFilter.h"
#include "PCGExFilterGroup.generated.h"

UENUM()
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
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override { return nullptr; }
	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const override;
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
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
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
	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
};

namespace PCGExFilterGroup
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FFilterGroup : public PCGExClusterFilter::FFilter
	{
	public:
		explicit FFilterGroup(const UPCGExFilterGroupFactoryBase* InFactory, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFilterFactories):
			FFilter(InFactory), GroupFactory(InFactory), ManagedFactories(InFilterFactories)
		{
		}

		bool bValid = false;
		bool bInvert = false;
		const UPCGExFilterGroupFactoryBase* GroupFactory;
		const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* ManagedFactories;

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Group; }

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		virtual bool Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) override;

		virtual void PostInit() override;

		virtual bool Test(const int32 Index) const override = 0;
		virtual bool Test(const PCGExCluster::FNode& Node) const override = 0;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override = 0;

	protected:
		TArray<TSharedPtr<PCGExPointFilter::FFilter>> ManagedFilters;

		virtual bool InitManaged(FPCGExContext* InContext);
		bool InitManagedFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::FFilter>& Filter) const;
		virtual bool PostInitManaged(FPCGExContext* InContext);
		virtual void PostInitManagedFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::FFilter>& InFilter);
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FFilterGroupAND final : public FFilterGroup
	{
	public:
		explicit FFilterGroupAND(const UPCGExFilterGroupFactoryBase* InFactory, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFilterFactories):
			FFilterGroup(InFactory, InFilterFactories)
		{
		}

		FORCEINLINE virtual bool Test(const int32 Index) const override
		{
			for (const TSharedPtr<PCGExPointFilter::FFilter>& Filter : ManagedFilters) { if (!Filter->Test(Index)) { return bInvert; } }
			return !bInvert;
		}

		FORCEINLINE virtual bool Test(const PCGExCluster::FNode& Node) const override
		{
			for (const TSharedPtr<PCGExPointFilter::FFilter>& Filter : ManagedFilters) { if (!Filter->Test(Node)) { return bInvert; } }
			return !bInvert;
		}

		FORCEINLINE virtual bool Test(const PCGExGraph::FEdge& Edge) const override
		{
			for (const TSharedPtr<PCGExPointFilter::FFilter>& Filter : ManagedFilters) { if (!Filter->Test(Edge)) { return bInvert; } }
			return !bInvert;
		}
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FFilterGroupOR final : public FFilterGroup
	{
	public:
		explicit FFilterGroupOR(const UPCGExFilterGroupFactoryBase* InFactory, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>* InFilterFactories):
			FFilterGroup(InFactory, InFilterFactories)
		{
		}

		FORCEINLINE virtual bool Test(const int32 Index) const override
		{
			for (const TSharedPtr<PCGExPointFilter::FFilter>& Filter : ManagedFilters) { if (Filter->Test(Index)) { return !bInvert; } }
			return bInvert;
		}

		FORCEINLINE virtual bool Test(const PCGExCluster::FNode& Node) const override
		{
			for (const TSharedPtr<PCGExPointFilter::FFilter>& Filter : ManagedFilters) { if (Filter->Test(Node)) { return !bInvert; } }
			return bInvert;
		}

		FORCEINLINE virtual bool Test(const PCGExGraph::FEdge& Edge) const override
		{
			for (const TSharedPtr<PCGExPointFilter::FFilter>& Filter : ManagedFilters) { if (Filter->Test(Edge)) { return !bInvert; } }
			return bInvert;
		}
	};
}
