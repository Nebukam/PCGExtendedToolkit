// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExSortingCommon.h"
#include "Metadata/PCGAttributePropertySelector.h"

struct FPCGExContext;
struct FPCGExSortRuleConfig;

namespace PCGExData
{
	struct FElement;
	class FFacade;
	class IDataValue;
	class IBufferProxy;
}

namespace PCGExSorting
{
	class PCGEXCORE_API FRuleHandler : public TSharedFromThis<FRuleHandler>
	{
	public:
		FRuleHandler() = default;

		explicit FRuleHandler(const FPCGExSortRuleConfig& Config);

		TSharedPtr<PCGExData::IBufferProxy> Buffer;
		TArray<TSharedPtr<PCGExData::IBufferProxy>> Buffers;
		TArray<TSharedPtr<PCGExData::IDataValue>> DataValues;

		FPCGAttributePropertyInputSelector Selector;

		double Tolerance = DBL_COMPARE_TOLERANCE;
		bool bInvertRule = false;
		bool bAbsolute = false;
	};

	class PCGEXCORE_API FSorter : public TSharedFromThis<FSorter>
	{
	protected:
		FPCGExContext* ExecutionContext = nullptr;
		TArray<TSharedPtr<FRuleHandler>> RuleHandlers;
		TMap<uint32, int32> IdxMap;

	public:
		EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;
		TSharedPtr<PCGExData::FFacade> DataFacade;

		FSorter(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade, TArray<FPCGExSortRuleConfig> InRuleConfigs);
		explicit FSorter(TArray<FPCGExSortRuleConfig> InRuleConfigs);

		bool Init(FPCGExContext* InContext);
		bool Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InDataFacades);
		bool Init(FPCGExContext* InContext, const TArray<FPCGTaggedData>& InTaggedDatas);

		bool Sort(const int32 A, const int32 B);
		bool Sort(const PCGExData::FElement A, const PCGExData::FElement B);
		bool SortData(const int32 A, const int32 B);
	};
}

#undef PCGEX_UNSUPPORTED_STRING_TYPES
#undef PCGEX_UNSUPPORTED_PATH_TYPES
