﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExReversePointOrder.h"

#include "PCGExMath.h"
#include "Curve/CurveUtil.h"
#include "Data/PCGExDataPreloader.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExReversePointOrderElement"
#define PCGEX_NAMESPACE ReversePointOrder

TArray<FPCGPinProperties> UPCGExReversePointOrderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Method == EPCGExPointReverseMethod::SortingRules) { PCGExSorting::DeclareSortingRulesInputs(PinProperties, EPCGPinStatus::Required); }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ReversePointOrder)
PCGEX_ELEMENT_BATCH_POINT_IMPL(ReversePointOrder)

bool FPCGExReversePointOrderElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ReversePointOrder)

	for (const FPCGExSwapAttributePairDetails& OriginalPair : Settings->SwapAttributesValues)
	{
		if (!OriginalPair.Validate(Context)) { return false; }
	}

	return true;
}

bool FPCGExReversePointOrderElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExReversePointOrderElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ReversePointOrder)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bPrefetchData = Settings->Method != EPCGExPointReverseMethod::None || !Settings->SwapAttributesValues.IsEmpty();
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExReversePointOrder
{
	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TProcessor<FPCGExReversePointOrderContext, UPCGExReversePointOrderSettings>::RegisterBuffersDependencies(FacadePreloader);

		const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(PointDataFacade->GetIn()->Metadata);

		for (const FPCGExSwapAttributePairDetails& OriginalPair : Settings->SwapAttributesValues)
		{
			PCGEx::FAttributeIdentity* FirstIdentity = AttributesInfos->Find(OriginalPair.FirstAttributeName);
			PCGEx::FAttributeIdentity* SecondIdentity = AttributesInfos->Find(OriginalPair.SecondAttributeName);
			if (!FirstIdentity || !SecondIdentity) { continue; }
			if (FirstIdentity->UnderlyingType != SecondIdentity->UnderlyingType) { continue; }

			const int32 AddIndex = SwapPairs.Add(OriginalPair);

			SwapPairs[AddIndex].FirstIdentity = FirstIdentity;
			SwapPairs[AddIndex].SecondIdentity = SecondIdentity;

			FacadePreloader.Register(Context, *FirstIdentity);
			FacadePreloader.Register(Context, *SecondIdentity);
		}

		if (Settings->Method == EPCGExPointReverseMethod::SortingRules)
		{
			Sorter = MakeShared<PCGExSorting::FPointSorter>(Context, PointDataFacade, PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
			Sorter->SortDirection = Settings->SortDirection;
		}
		else if (Settings->Method == EPCGExPointReverseMethod::Winding && Settings->ProjectionDetails.bLocalProjectionNormal)
		{
			FacadePreloader.Register<FVector>(Context, Settings->ProjectionDetails.LocalNormal);
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		ON_SCOPE_EXIT
		{
			if (!bReversed) { PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward); }
		};

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (Sorter)
		{
			if (!Sorter->Init(Context))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some sorting rules could not be processed."));
				bReversed = false;
				return false;
			}

			if (!Sorter->Sort(0, PointDataFacade->GetNum() - 1))
			{
				bReversed = false;
				return true;
			}
		}

		if (Settings->Method == EPCGExPointReverseMethod::Winding)
		{
			FPCGExGeo2DProjectionDetails Proj = Settings->ProjectionDetails;

			if (Proj.Method == EPCGExProjectionMethod::Normal) { if (!Proj.Init(PointDataFacade)) { return false; } }
			else { Proj.Init(PCGExGeo::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange())); }

			TArray<FVector2D> ProjectedPoints;
			Proj.ProjectFlat(PointDataFacade, ProjectedPoints);

			bReversed = !PCGExGeo::IsWinded(Settings->Winding, UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(ProjectedPoints) < 0);
			if (!bReversed) { return true; }
		}

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

#define PCGEX_NATIVE_REVERSE(_NAME, _TYPE, ...) PCGEx::Reverse(PointDataFacade->GetOut()->Get##_NAME##ValueRange());
		PCGEX_FOREACH_POINT_NATIVE_PROPERTY(PCGEX_NATIVE_REVERSE)
#undef PCGEX_NATIVE_REVERSE

		if (SwapPairs.IsEmpty()) { return true; } // Swap pairs are built during data prefetch

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FetchWritersTask)

		FetchWritersTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->StartParallelLoopForPoints();
			};

		FetchWritersTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::FetchWriters);
				PCGEX_ASYNC_THIS
				FPCGExSwapAttributePairDetails& WorkingPair = This->SwapPairs[Scope.Start];

				PCGEx::ExecuteWithRightType(
					WorkingPair.FirstIdentity->UnderlyingType, [&](auto DummyValue)
					{
						using T_REAL = decltype(DummyValue);
						WorkingPair.FirstWriter = This->PointDataFacade->GetWritable<T_REAL>(WorkingPair.FirstAttributeName, PCGExData::EBufferInit::Inherit);
						WorkingPair.SecondWriter = This->PointDataFacade->GetWritable<T_REAL>(WorkingPair.SecondAttributeName, PCGExData::EBufferInit::Inherit);
					});
			};

		FetchWritersTask->StartSubLoops(SwapPairs.Num(), 1);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::ReversePointOrder::ProcessPoints);

		for (const FPCGExSwapAttributePairDetails& WorkingPair : SwapPairs)
		{
			PCGEx::ExecuteWithRightType(
				WorkingPair.FirstIdentity->UnderlyingType, [&](auto DummyValue)
				{
					using T_REAL = decltype(DummyValue);
					TSharedPtr<PCGExData::TBuffer<T_REAL>> FirstWriter = StaticCastSharedPtr<PCGExData::TBuffer<T_REAL>>(WorkingPair.FirstWriter);
					TSharedPtr<PCGExData::TBuffer<T_REAL>> SecondWriter = StaticCastSharedPtr<PCGExData::TBuffer<T_REAL>>(WorkingPair.SecondWriter);

					if (WorkingPair.bMultiplyByMinusOne)
					{
						PCGEX_SCOPE_LOOP(Index)
						{
							const T_REAL FirstValue = FirstWriter->GetValue(Index);
							FirstWriter->SetValue(Index, PCGExMath::DblMult(SecondWriter->GetValue(Index), -1));
							SecondWriter->SetValue(Index, PCGExMath::DblMult(FirstValue, -1));
						}
					}
					else
					{
						PCGEX_SCOPE_LOOP(Index)
						{
							const T_REAL FirstValue = FirstWriter->GetValue(Index);
							FirstWriter->SetValue(Index, SecondWriter->GetValue(Index));
							SecondWriter->SetValue(Index, FirstValue);
						}
					}
				});
		}
	}

	void FProcessor::CompleteWork()
	{
		if (bReversed)
		{
			if (!SwapPairs.IsEmpty()) { PointDataFacade->WriteFastest(AsyncManager); }
			if (Settings->bTagIfReversed) { PointDataFacade->Source->Tags->AddRaw(Settings->IsReversedTag); }
		}
		else
		{
			if (Settings->bTagIfNotReversed) { PointDataFacade->Source->Tags->AddRaw(Settings->IsNotReversedTag); }
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
