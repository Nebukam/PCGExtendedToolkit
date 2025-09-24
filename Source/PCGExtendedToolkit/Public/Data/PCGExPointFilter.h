// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFactoryProvider.h"

#include "PCGExPointFilter.generated.h"

namespace PCGExData
{
	class FPointIO;
	struct FProxyPoint;
	class FPointIOCollection;
}

namespace PCGExGraph
{
	struct FEdge;
}

namespace PCGExCluster
{
	struct FNode;
}

class UPCGExFilterProviderSettings;

UENUM()
enum class EPCGExFilterFallback : uint8
{
	Pass = 0 UMETA(DisplayName = "Pass", ToolTip="This item will be considered to successfully pass the filter"),
	Fail = 1 UMETA(DisplayName = "Fail", ToolTip="This item will be considered to failing to pass the filter"),
};

UENUM()
enum class EPCGExFilterResult : uint8
{
	Pass = 0 UMETA(DisplayName = "Pass", ToolTip="Passes the filters"),
	Fail = 1 UMETA(DisplayName = "Fail", ToolTip="Fails the filters"),
};

UENUM()
enum class EPCGExFilterNoDataFallback : uint8
{
	Error = 0 UMETA(DisplayName = "Throw Error", ToolTip="This filter will throw an error if there is no data."),
	Pass  = 1 UMETA(DisplayName = "Pass", ToolTip="This filter will pass if there is no data"),
	Fail  = 2 UMETA(DisplayName = "Fail", ToolTip="This filter will fail if there is no data"),
};

namespace PCGExGraph
{
	struct FEdge;
}

namespace PCGExPointFilter
{
	class IFilter;
}

namespace PCGExFilters
{
	enum class EType : uint8
	{
		None = 0,
		Point,
		Group,
		Node,
		Edge,
		Collection,
	};
}

USTRUCT(/*PCG_DataType*/DisplayName="PCGEx | Filter")
struct FPCGExPointFilterDataTypeInfo : public FPCGExFactoryDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExFilterFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

	friend UPCGExFilterProviderSettings;

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExPointFilterDataTypeInfo)
	
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterPoint; }

	virtual bool DomainCheck();
	virtual bool GetOnlyUseDataDomain() const { return bOnlyUseDataDomain; }

	virtual bool SupportsCollectionEvaluation() const { return bOnlyUseDataDomain; }
	virtual bool SupportsProxyEvaluation() const { return false; }

	virtual bool Init(FPCGExContext* InContext);

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const;

	int32 Priority = 0;
	EPCGExFilterNoDataFallback MissingDataHandling = EPCGExFilterNoDataFallback::Fail;

protected:
	bool bOnlyUseDataDomain = false;
};

namespace PCGExPointFilter
{
	const FName OutputFilterLabel = FName("Filter");
	const FName OutputColFilterLabel = FName("C-Filter");
	const FName OutputFilterLabelNode = FName("Node Filter");
	const FName OutputFilterLabelEdge = FName("Edge Filter");
	const FName SourceFiltersLabel = FName("Filters");

	const FName SourceFiltersConditionLabel = FName("Conditions Filters");
	const FName SourceKeepConditionLabel = FName("Keep Conditions");
	const FName SourceToggleConditionLabel = FName("Toggle Conditions");
	const FName SourceStartConditionLabel = FName("Start Conditions");
	const FName SourceStopConditionLabel = FName("Stop Conditions");
	const FName SourcePinConditionLabel = FName("Pin Conditions");

	const FName SourcePointFiltersLabel = FName("Point Filters");
	const FName SourceVtxFiltersLabel = FName("Vtx Filters");
	const FName SourceEdgeFiltersLabel = FName("Edge Filters");

	const FName OutputInsideFiltersLabel = FName("Inside");
	const FName OutputOutsideFiltersLabel = FName("Outside");

	class PCGEXTENDEDTOOLKIT_API IFilter : public TSharedFromThis<IFilter>
	{
	public:
		explicit IFilter(const TObjectPtr<const UPCGExFilterFactoryData>& InFactory):
			Factory(InFactory)
		{
		}

		bool bUseDataDomainSelectorsOnly = false;
		bool bCollectionTestResult = true;
		bool bUseEdgeAsPrimary = false; // This shouldn't be there but...

		bool DefaultResult = true;
		TSharedPtr<PCGExData::FFacade> PointDataFacade;

		bool bCacheResults = true;
		TObjectPtr<const UPCGExFilterFactoryData> Factory;
		TArray<int8> Results;

		int32 FilterIndex = 0;

		virtual PCGExFilters::EType GetFilterType() const { return PCGExFilters::EType::Point; }

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade);
		virtual void PostInit();

		virtual bool Test(const int32 Index) const;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const; // destined for no-context evaluation only, can't rely on attributes or anything.
		virtual bool Test(const PCGExCluster::FNode& Node) const;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const;

		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const; // destined for collection only, is expected to test internal PointDataFacade directly.

		virtual void SetSupportedTypes(const TSet<PCGExFactories::EType>* InTypes)
		{
		}

		virtual ~IFilter() = default;
	};

	class PCGEXTENDEDTOOLKIT_API ISimpleFilter : public IFilter
	{
	public:
		explicit ISimpleFilter(const TObjectPtr<const UPCGExFilterFactoryData>& InFactory):
			IFilter(InFactory)
		{
		}

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override final;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override final;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;
	};

	class PCGEXTENDEDTOOLKIT_API ICollectionFilter : public IFilter
	{
	public:
		explicit ICollectionFilter(const TObjectPtr<const UPCGExFilterFactoryData>& InFactory):
			IFilter(InFactory)
		{
		}

		virtual PCGExFilters::EType GetFilterType() const override { return PCGExFilters::EType::Collection; }

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;

		virtual bool Test(const int32 Index) const override;
		virtual bool Test(const PCGExData::FProxyPoint& Point) const override;
		virtual bool Test(const PCGExCluster::FNode& Node) const override final;
		virtual bool Test(const PCGExGraph::FEdge& Edge) const override final;
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;
	};

	class PCGEXTENDEDTOOLKIT_API FManager : public TSharedFromThis<FManager>
	{
	public:
		explicit FManager(const TSharedRef<PCGExData::FFacade>& InPointDataFacade);

		bool bUseEdgeAsPrimary = false; // This shouldn't be there...

		bool bCacheResultsPerFilter = false;
		bool bCacheResults = false;
		TArray<int8> Results;

		bool bValid = false;

		TSharedRef<PCGExData::FFacade> PointDataFacade;

		bool Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories);

		virtual bool Test(const int32 Index);
		virtual bool Test(const PCGExData::FProxyPoint& Point);
		virtual bool Test(const PCGExCluster::FNode& Node);
		virtual bool Test(const PCGExGraph::FEdge& Edge);
		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection);

		virtual int32 Test(const PCGExMT::FScope Scope, TArray<int8>& OutResults);
		virtual int32 Test(const PCGExMT::FScope Scope, TBitArray<>& OutResults);

		virtual int32 Test(const TArrayView<PCGExCluster::FNode> Items, const TArrayView<int8> OutResults);
		virtual int32 Test(const TArrayView<PCGExCluster::FNode> Items, const TSharedPtr<TArray<int8>>& OutResults);
		virtual int32 Test(const TArrayView<PCGExGraph::FEdge> Items, const TArrayView<int8> OutResults);

		virtual ~FManager()
		{
		}

		void SetSupportedTypes(const TSet<PCGExFactories::EType>* InTypes);
		const TSet<PCGExFactories::EType>* GetSupportedTypes() const;

	protected:
		const TSet<PCGExFactories::EType>* SupportedFactoriesTypes = nullptr;
		TArray<TSharedPtr<IFilter>> ManagedFilters;

		virtual bool InitFilter(FPCGExContext* InContext, const TSharedPtr<IFilter>& Filter);
		virtual bool PostInit(FPCGExContext* InContext);
		virtual void PostInitFilter(FPCGExContext* InContext, const TSharedPtr<IFilter>& InFilter);

		virtual void InitCache();
	};

	static void RegisterBuffersDependencies(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories, PCGExData::FFacadePreloader& FacadePreloader)
	{
		for (const UPCGExFilterFactoryData* Factory : InFactories)
		{
			Factory->RegisterBuffersDependencies(InContext, FacadePreloader);
		}
	}

	static void PruneForDirectEvaluation(FPCGExContext* InContext, TArray<TObjectPtr<const UPCGExFilterFactoryData>>& InFactories)
	{
		if (InFactories.IsEmpty()) { return; }

		TArray<FString> UnsupportedFilters;
		UnsupportedFilters.Reserve(InFactories.Num());

		int32 WriteIndex = 0;
		for (int32 i = 0; i < InFactories.Num(); i++)
		{
			if (InFactories[i]->SupportsProxyEvaluation()) { InFactories[WriteIndex++] = InFactories[i]; }
			else { UnsupportedFilters.AddUnique(InFactories[i]->GetName()); }
		}

		InFactories.SetNum(WriteIndex);

		if (InFactories.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("None of the filters used supports direct evaluation."));
		}
		else if (!UnsupportedFilters.IsEmpty())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Some filters don't support direct evaluation and will be ignored: \"{0}\"."), FText::FromString(FString::Join(UnsupportedFilters, TEXT(", ")))));
		}
	}
}

USTRUCT(/*PCG_DataType*/DisplayName="PCGEx | Filter (Data)")
struct FPCGExCollectionFilterDataTypeInfo : public FPCGExPointFilterDataTypeInfo
{
	GENERATED_BODY()
	PCG_DECLARE_TYPE_INFO(PCGEXTENDEDTOOLKIT_API)
};

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExFilterCollectionFactoryData : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	PCG_ASSIGN_TYPE_INFO(FPCGExCollectionFilterDataTypeInfo)
	
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FilterCollection; }
	virtual bool DomainCheck() override;
	virtual bool SupportsCollectionEvaluation() const override;
	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override { return nullptr; }
};
