// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterCommon.h"
#include "UObject/Object.h"
#include "Factories/PCGExFactoryProvider.h"
#include "Factories/PCGExFactoryData.h"

#include "PCGExPointFilter.generated.h"

namespace PCGExData
{
	class FPointIO;
	struct FProxyPoint;
	class FPointIOCollection;
}

namespace PCGExGraphs
{
	struct FEdge;
}

namespace PCGExClusters
{
	struct FNode;
}

class UPCGExFilterProviderSettings;

namespace PCGExPointFilter
{
	class IFilter;
}

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Filter"))
struct FPCGExDataTypeInfoFilter : public FPCGExFactoryDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXFILTERS_API)
};

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Filter (Point)"))
struct FPCGExDataTypeInfoFilterPoint : public FPCGExDataTypeInfoFilter
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXFILTERS_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXFILTERS_API UPCGExFilterFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

	friend UPCGExFilterProviderSettings;

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoFilter)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Filter; }

	virtual bool DomainCheck();
	virtual bool GetOnlyUseDataDomain() const { return bOnlyUseDataDomain; }

	virtual bool SupportsCollectionEvaluation() const { return bOnlyUseDataDomain; }
	virtual bool SupportsProxyEvaluation() const { return false; }

	virtual bool Init(FPCGExContext* InContext);

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const;

	int32 Priority = 0;
	EPCGExFilterNoDataFallback InitializationFailurePolicy = EPCGExFilterNoDataFallback::Error;
	EPCGExFilterNoDataFallback MissingDataPolicy = EPCGExFilterNoDataFallback::Fail;

protected:
	bool bOnlyUseDataDomain = false;
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXFILTERS_API UPCGExPointFilterFactoryData : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

	friend UPCGExFilterProviderSettings;

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoFilterPoint)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterPoint; }
};

namespace PCGExPointFilter
{
	class PCGEXFILTERS_API IFilter : public TSharedFromThis<IFilter>
	{
	public:
		explicit IFilter(const TObjectPtr<const UPCGExPointFilterFactoryData>& InFactory)
			: Factory(InFactory)
		{
		}

		bool bWillBeUsedWithCollections = false;
		bool bUseDataDomainSelectorsOnly = false;
		bool bCollectionTestResult = true;
		bool bUseEdgeAsPrimary = false; // This shouldn't be there but...

		bool DefaultResult = true;
		TSharedPtr<PCGExData::FFacade> PointDataFacade;

		bool bCacheResults = true;
		TObjectPtr<const UPCGExPointFilterFactoryData> Factory;
		TArray<int8> Results;

		int32 FilterIndex = 0;

		virtual PCGExFilters::EType GetFilterType() const { return PCGExFilters::EType::Point; }

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade);
		virtual void PostInit();

		virtual bool Test(const int32 Index) const;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const; // destined for no-context evaluation only, can't rely on attributes or anything.
		virtual bool Test(const PCGExClusters::FNode& Node) const;
		virtual bool Test(const PCGExGraphs::FEdge& Edge) const;

		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const; // destined for collection only, is expected to test internal PointDataFacade directly.

		virtual void SetSupportedTypes(const TSet<PCGExFactories::EType>* InTypes)
		{
		}

		virtual ~IFilter() = default;
	};

	class PCGEXFILTERS_API ISimpleFilter : public IFilter
	{
	public:
		explicit ISimpleFilter(const TObjectPtr<const UPCGExPointFilterFactoryData>& InFactory)
			: IFilter(InFactory)
		{
		}

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const override;
		virtual bool Test(const PCGExClusters::FNode& Node) const override final;
		virtual bool Test(const PCGExGraphs::FEdge& Edge) const override final;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;
	};

	class PCGEXFILTERS_API ICollectionFilter : public IFilter
	{
	public:
		explicit ICollectionFilter(const TObjectPtr<const UPCGExPointFilterFactoryData>& InFactory)
			: IFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Collection; }

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const override;
		virtual bool Test(const PCGExClusters::FNode& Node) const override final;
		virtual bool Test(const PCGExGraphs::FEdge& Edge) const override final;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;
	};

	class PCGEXFILTERS_API FManager : public TSharedFromThis<FManager>
	{
	public:
		explicit FManager(const TSharedRef<PCGExData::FFacade>& InPointDataFacade);

		bool bUseEdgeAsPrimary = false; // This shouldn't be there...
		bool bWillBeUsedWithCollections = false;

		bool bCacheResultsPerFilter = false;
		bool bCacheResults = false;
		TArray<int8> Results;

		bool bValid = false;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>& InFactories);

		virtual bool Test(const int32 Index);
		virtual bool Test(const PCGExData::FProxyPoint& Point);
		virtual bool Test(const PCGExClusters::FNode& Node);
		virtual bool Test(const PCGExGraphs::FEdge& Edge);
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection);

		virtual int32 Test(const PCGExMT::FScope Scope, TArray<int8>& OutResults, const bool bParallel = false);
		virtual int32 Test(const PCGExMT::FScope Scope, TBitArray<>& OutResults, const bool bParallel = false);

		virtual int32 Test(const TArrayView<PCGExClusters::FNode> Items, const TArrayView<int8> OutResults, const bool bParallel = false);
		virtual int32 Test(const TArrayView<PCGExClusters::FNode> Items, const TSharedPtr<TArray<int8>>& OutResultsPtr, const bool bParallel = false);
		virtual int32 Test(const TArrayView<PCGExGraphs::FEdge> Items, const TArrayView<int8> OutResults, const bool bParallel = false);

		virtual ~FManager()
		{
		}

		void SetSupportedTypes(const TSet<PCGExFactories::EType>* InTypes);
		const TSet<PCGExFactories::EType>* GetSupportedTypes() const;

	protected:
		const TSet<PCGExFactories::EType>* SupportedFactoriesTypes = nullptr;
		TArray<TSharedPtr<IFilter>> ManagedFilters;
		TArray<const IFilter*> Stack;

		virtual bool InitFilter(FPCGExContext* InContext, const TSharedPtr<IFilter>& Filter);
		virtual bool PostInit(FPCGExContext* InContext);
		virtual void PostInitFilter(FPCGExContext* InContext, const TSharedPtr<IFilter>& InFilter);

		virtual void InitCache();
	};

	PCGEXFILTERS_API
	void RegisterBuffersDependencies(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>& InFactories, PCGExData::FFacadePreloader& FacadePreloader);

	PCGEXFILTERS_API
	void PruneForDirectEvaluation(FPCGExContext* InContext, TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>& InFactories);
}

USTRUCT(meta=(PCG_DataTypeDisplayName="PCGEx | Filter (Data)"))
struct FPCGExDataTypeInfoFilterCollection : public FPCGExDataTypeInfoFilter
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXFILTERS_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXFILTERS_API UPCGExFilterCollectionFactoryData : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExDataTypeInfoFilterCollection)

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterCollection; }
	virtual bool DomainCheck() override;
	virtual bool SupportsCollectionEvaluation() const override;
	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override { return nullptr; }
};
