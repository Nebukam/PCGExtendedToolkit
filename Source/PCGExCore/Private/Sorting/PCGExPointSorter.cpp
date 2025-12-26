// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sorting/PCGExPointSorter.h"

#include "Utils/PCGExCompare.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExProxyDataHelpers.h"
#include "Sorting/PCGExSortingDetails.h"
#include "Async/ParallelFor.h"

#define LOCTEXT_NAMESPACE "PCGExModularSortPoints"
#define PCGEX_NAMESPACE ModularSortPoints

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

namespace PCGExSorting
{
	FSorter::FSorter(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade, TArray<FPCGExSortRuleConfig> InRuleConfigs)
		: ExecutionContext(InContext), DataFacade(InDataFacade)
	{
		const UPCGData* InData = InDataFacade->Source->GetIn();
		FName Consumable = NAME_None;

		for (const FPCGExSortRuleConfig& RuleConfig : InRuleConfigs)
		{
			PCGEX_MAKE_SHARED(NewRule, FRuleHandler, RuleConfig)
			RuleHandlers.Add(NewRule);

			if (InContext->bCleanupConsumableAttributes && InData) { PCGEX_CONSUMABLE_SELECTOR(RuleConfig.Selector, Consumable) }
		}
	}

	FRuleHandler::FRuleHandler(const FPCGExSortRuleConfig& Config)
		: Selector(Config.Selector), Tolerance(Config.Tolerance), bInvertRule(Config.bInvertRule)
	{
	}

	FSorter::FSorter(TArray<FPCGExSortRuleConfig> InRuleConfigs)
	{
		for (const FPCGExSortRuleConfig& RuleConfig : InRuleConfigs)
		{
			PCGEX_MAKE_SHARED(NewRule, FRuleHandler, RuleConfig)
			RuleHandlers.Add(NewRule);
		}
	}

	bool FSorter::Init(FPCGExContext* InContext)
	{
		for (int i = 0; i < RuleHandlers.Num(); i++)
		{
			const TSharedPtr<FRuleHandler> RuleHandler = RuleHandlers[i];

			TSharedPtr<PCGExData::IBufferProxy> Buffer = nullptr;

			PCGExData::FProxyDescriptor Descriptor(DataFacade);
			Descriptor.bWantsDirect = true;

			if (Descriptor.CaptureStrict(InContext, RuleHandler->Selector, PCGExData::EIOSide::In)) { Buffer = PCGExData::GetProxyBuffer(InContext, Descriptor); }

			if (!Buffer)
			{
				RuleHandlers.RemoveAt(i);
				i--;

				PCGEX_LOG_INVALID_SELECTOR_C(InContext, Sorting Rule, RuleHandler->Selector)
				continue;
			}

			RuleHandler->Buffer = Buffer;
		}

		return !RuleHandlers.IsEmpty();
	}

	bool FSorter::Init(FPCGExContext* InContext, const TArray<TSharedRef<PCGExData::FFacade>>& InDataFacades)
	{
		int32 MaxIndex = 0;
		for (const TSharedRef<PCGExData::FFacade>& Facade : InDataFacades) { MaxIndex = FMath::Max(Facade->Idx, MaxIndex); }
		MaxIndex++;

		for (int i = 0; i < RuleHandlers.Num(); i++)
		{
			const TSharedPtr<FRuleHandler> RuleHandler = RuleHandlers[i];
			RuleHandler->Buffers.SetNum(MaxIndex);

			for (int f = 0; f < InDataFacades.Num(); f++)
			{
				TSharedPtr<PCGExData::FFacade> InFacade = InDataFacades[f];
				PCGExData::FProxyDescriptor Descriptor(InFacade);
				Descriptor.bWantsDirect = true;

				TSharedPtr<PCGExData::IBufferProxy> Buffer = nullptr;

				if (Descriptor.CaptureStrict(InContext, RuleHandler->Selector, PCGExData::EIOSide::In)) { Buffer = PCGExData::GetProxyBuffer(InContext, Descriptor); }

				if (!Buffer)
				{
					RuleHandlers.RemoveAt(i);
					i--;

					PCGEX_LOG_INVALID_SELECTOR_C(InContext, Sorting Rule, RuleHandler->Selector)
					break;
				}
				RuleHandler->Buffers[InFacade->Idx] = Buffer;
			}
		}

		return !RuleHandlers.IsEmpty();
	}

	bool FSorter::Init(FPCGExContext* InContext, const TArray<FPCGTaggedData>& InTaggedDatas)
	{
		IdxMap.Reserve(InTaggedDatas.Num());
		for (int i = 0; i < InTaggedDatas.Num(); i++) { IdxMap.Add(InTaggedDatas[i].Data->GetUniqueID(), i); }

		for (int i = 0; i < RuleHandlers.Num(); i++)
		{
			const TSharedPtr<FRuleHandler> RuleHandler = RuleHandlers[i];
			RuleHandler->DataValues.SetNum(InTaggedDatas.Num());

			for (int f = 0; f < InTaggedDatas.Num(); f++)
			{
				const UPCGData* Data = InTaggedDatas[f].Data;
				const int32 DataIdx = IdxMap[Data->GetUniqueID()];

				TSharedPtr<PCGExData::IDataValue> DataValue = PCGExData::TryGetValueFromData(Data, RuleHandler->Selector);

				if (!DataValue)
				{
					RuleHandlers.RemoveAt(i);
					i--;

					PCGEX_LOG_INVALID_SELECTOR_C(InContext, Sorting Rule, RuleHandler->Selector)
					break;
				}

				RuleHandler->DataValues[DataIdx] = DataValue;
			}
		}

		return !RuleHandlers.IsEmpty();
	}

	bool FSorter::Sort(const int32 A, const int32 B)
	{
		int Result = 0;
		for (const TSharedPtr<FRuleHandler>& RuleHandler : RuleHandlers)
		{
			const double ValueA = RuleHandler->Buffer->ReadAsDouble(A);
			const double ValueB = RuleHandler->Buffer->ReadAsDouble(B);
			Result = FMath::IsNearlyEqual(ValueA, ValueB, RuleHandler->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			if (Result != 0)
			{
				if (RuleHandler->bInvertRule) { Result *= -1; }
				break;
			}
		}

		if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
		return Result < 0;
	}

	bool FSorter::Sort(const PCGExData::FElement A, const PCGExData::FElement B)
	{
		int Result = 0;
		for (const TSharedPtr<FRuleHandler>& RuleHandler : RuleHandlers)
		{
			const double ValueA = RuleHandler->Buffers[A.IO]->ReadAsDouble(A.Index);
			const double ValueB = RuleHandler->Buffers[B.IO]->ReadAsDouble(B.Index);
			Result = FMath::IsNearlyEqual(ValueA, ValueB, RuleHandler->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			if (Result != 0)
			{
				if (RuleHandler->bInvertRule) { Result *= -1; }
				break;
			}
		}

		if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
		return Result < 0;
	}

	bool FSorter::SortData(const int32 A, const int32 B)
	{
		int Result = 0;
		for (const TSharedPtr<FRuleHandler>& RuleHandler : RuleHandlers)
		{
			const TSharedPtr<PCGExData::IDataValue> DataValueA = RuleHandler->DataValues[A];
			const TSharedPtr<PCGExData::IDataValue> DataValueB = RuleHandler->DataValues[B];

			if (!DataValueA || !DataValueB) { continue; }

			if (DataValueA->IsNumeric() || DataValueB->IsNumeric())
			{
				const double ValueA = DataValueA->AsDouble();
				const double ValueB = DataValueB->AsDouble();

				Result = FMath::IsNearlyEqual(ValueA, ValueB, RuleHandler->Tolerance) ? 0 : ValueA < ValueB ? -1 : 1;
			}
			else
			{
				const FString ValueA = DataValueA->AsString();
				const FString ValueB = DataValueB->AsString();

				Result = PCGExCompare::StrictlyEqual(ValueA, ValueB) ? 0 : ValueA < ValueB ? -1 : 1;
			}

			if (Result != 0)
			{
				if (RuleHandler->bInvertRule) { Result *= -1; }
				break;
			}
		}

		if (SortDirection == EPCGExSortDirection::Descending) { Result *= -1; }
		return Result < 0;
	}

	TSharedPtr<FSortCache> FSorter::BuildCache(int32 NumElements) const
	{
		return FSortCache::Build(*this, NumElements);
	}

#pragma region FSortCache

	TSharedPtr<FSortCache> FSortCache::Build(const FSorter& Sorter, int32 InNumElements)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSortCache::Build);
		
		if (InNumElements <= 0 || Sorter.RuleHandlers.IsEmpty())
		{
			return nullptr;
		}

		TSharedPtr<FSortCache> Cache = MakeShared<FSortCache>();
		Cache->NumElements = InNumElements;
		Cache->bDescending = (Sorter.SortDirection == EPCGExSortDirection::Descending);

		const int32 NumRules = Sorter.RuleHandlers.Num();
		Cache->Rules.SetNum(NumRules);

		// Collect buffer pointers for fast access in parallel loop
		TArray<PCGExData::IBufferProxy*> Buffers;
		Buffers.SetNum(NumRules);

		// Initialize rule caches and collect buffers
		for (int32 RuleIdx = 0; RuleIdx < NumRules; RuleIdx++)
		{
			const TSharedPtr<FRuleHandler>& Handler = Sorter.RuleHandlers[RuleIdx];
			FRuleCache& RuleCache = Cache->Rules[RuleIdx];

			RuleCache.Tolerance = Handler->Tolerance;
			RuleCache.bInvertRule = Handler->bInvertRule;
			RuleCache.Values.SetNumUninitialized(InNumElements);

			Buffers[RuleIdx] = Handler->Buffer.Get();
		}

		// Get raw pointers to value arrays for capture
		TArray<double*> ValueArrays;
		ValueArrays.SetNum(NumRules);
		for (int32 RuleIdx = 0; RuleIdx < NumRules; RuleIdx++)
		{
			ValueArrays[RuleIdx] = Cache->Rules[RuleIdx].Values.GetData();
		}

		// Single parallel pass - process all rules for each point
		//const int32 MinBatchSize = FMath::Max(1024, InNumElements / (FPlatformMisc::NumberOfCoresIncludingHyperthreads() * 4));

		PCGEX_PARALLEL_FOR(
			InNumElements,
			for (int32 RuleIdx = 0; RuleIdx < NumRules; RuleIdx++)
			{
			if (PCGExData::IBufferProxy* Buffer = Buffers[RuleIdx]) { ValueArrays[RuleIdx][i] = Buffer->ReadAsDouble(i); }
			else { ValueArrays[RuleIdx][i] = 0.0; }
			}
		)

		return Cache;
	}

#pragma endregion
}
