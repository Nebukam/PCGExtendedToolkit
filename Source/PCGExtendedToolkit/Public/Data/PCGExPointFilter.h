// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExData.h"
#include "PCGExDataCaching.h"
#include "PCGExFactoryProvider.h"

#include "PCGExPointFilter.generated.h"

namespace PCGExPointFilter
{
	class TFilter;
}

namespace PCGExFilters
{
	enum class EType : uint8
	{
		None = 0,
		Point,
		Node,
		ClusterEdge,
	};
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExFilterFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	FORCEINLINE virtual PCGExFactories::EType GetFactoryType() const override;

	virtual void Init();

	int32 Priority = 0;
	virtual PCGExPointFilter::TFilter* CreateFilter() const;
};

namespace PCGExPointFilter
{
	PCGEX_ASYNC_STATE(State_FilteringPoints)

	const FName OutputFilterLabel = TEXT("Filter");
	const FName SourceFiltersLabel = TEXT("Filters");
	const FName OutputInsideFiltersLabel = TEXT("Inside");
	const FName OutputOutsideFiltersLabel = TEXT("Outside");

	class PCGEXTENDEDTOOLKIT_API TFilter
	{
	public:
		explicit TFilter(const UPCGExFilterFactoryBase* InFactory):
			Factory(InFactory)
		{
		}

		PCGExDataCaching::FPool* PointDataCache = nullptr;

		bool bCacheResults = true;
		const UPCGExFilterFactoryBase* Factory;
		TArray<bool> Results;

		int32 FilterIndex = 0;

		FORCEINLINE virtual PCGExFilters::EType GetFilterType() const;

		virtual bool Init(const FPCGContext* InContext, PCGExDataCaching::FPool* InPointDataCache);

		virtual void PostInit();
		
		FORCEINLINE virtual bool Test(const int32 Index) const = 0;

		virtual ~TFilter()
		{
			Results.Empty();
		}
	};

	class PCGEXTENDEDTOOLKIT_API TManager
	{
	public:
		explicit TManager(PCGExDataCaching::FPool* InPointDataCache);

		bool bCacheResultsPerFilter = false;
		bool bCacheResults = false;
		TArray<bool> Results;

		bool bValid = false;

		PCGExDataCaching::FPool* PointDataCache = nullptr;

		bool Init(const FPCGContext* InContext, const TArray<UPCGExFilterFactoryBase*>& InFactories);
		virtual bool TestPoint(const int32 Index);

		virtual ~TManager()
		{
			PCGEX_DELETE_TARRAY(PointFilters)
		}

	protected:
		TArray<TFilter*> PointFilters;

		virtual bool InitFilter(const FPCGContext* InContext, TFilter* Filter);
		virtual bool PostInit(const FPCGContext* InContext);
		virtual void PostInitFilter(const FPCGContext* InContext, TFilter* InFilter);

		virtual void InitCache();
	};

}
