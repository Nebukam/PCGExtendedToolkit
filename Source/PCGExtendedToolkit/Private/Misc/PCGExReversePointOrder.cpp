// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExReversePointOrder.h"

#include "Curve/CurveUtil.h"


#define LOCTEXT_NAMESPACE "PCGExReversePointOrderElement"
#define PCGEX_NAMESPACE ReversePointOrder

PCGExData::EIOInit UPCGExReversePointOrderSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

TArray<FPCGPinProperties> UPCGExReversePointOrderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Method == EPCGExPointReverseMethod::SortingRules)
	{
		PCGEX_PIN_PARAMS(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	}
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ReversePointOrder)

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
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExReversePointOrder::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExReversePointOrder::FProcessor>>& NewBatch)
			{
				NewBatch->bPrefetchData = Settings->Method != EPCGExPointReverseMethod::None || !Settings->SwapAttributesValues.IsEmpty();
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExReversePointOrder
{
	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TPointsProcessor<FPCGExReversePointOrderContext, UPCGExReversePointOrderSettings>::RegisterBuffersDependencies(FacadePreloader);

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
			Sorter = MakeShared<PCGExSorting::PointSorter<false, true>>(Context, PointDataFacade, PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
			Sorter->SortDirection = Settings->SortDirection;
			Sorter->RegisterBuffersDependencies(FacadePreloader);
		}
		else if (Settings->Method == EPCGExPointReverseMethod::Winding && Settings->ProjectionDetails.bLocalProjectionNormal)
		{
			FacadePreloader.Register<FVector>(Context, Settings->ProjectionDetails.LocalNormal);
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		ON_SCOPE_EXIT
		{
			if (!bReversed) { PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward); }
		};

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		if (Sorter)
		{
			if (!Sorter->Init())
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
			FPCGExGeo2DProjectionDetails Proj = FPCGExGeo2DProjectionDetails(Settings->ProjectionDetails);
			if (!Proj.Init(Context, PointDataFacade)) { return false; }

			TArray<FVector2D> ProjectedPoints;
			Proj.ProjectFlat(PointDataFacade, ProjectedPoints);

			bReversed = !PCGExGeo::IsWinded(Settings->Winding, UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(ProjectedPoints) < 0);
			if (!bReversed) { return true; }
		}

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();
		Algo::Reverse(MutablePoints);

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
						using RawT = decltype(DummyValue);
						WorkingPair.FirstWriter = This->PointDataFacade->GetWritable<RawT>(WorkingPair.FirstAttributeName, PCGExData::EBufferInit::Inherit);
						WorkingPair.SecondWriter = This->PointDataFacade->GetWritable<RawT>(WorkingPair.SecondAttributeName, PCGExData::EBufferInit::Inherit);
					});
			};

		FetchWritersTask->StartSubLoops(SwapPairs.Num(), 1);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		FPointsProcessor::PrepareSingleLoopScopeForPoints(Scope);
		for (const FPCGExSwapAttributePairDetails& WorkingPair : SwapPairs)
		{
			PCGEx::ExecuteWithRightType(
				WorkingPair.FirstIdentity->UnderlyingType, [&](auto DummyValue)
				{
					using RawT = decltype(DummyValue);
					TSharedPtr<PCGExData::TBuffer<RawT>> FirstWriter = StaticCastSharedPtr<PCGExData::TBuffer<RawT>>(WorkingPair.FirstWriter);
					TSharedPtr<PCGExData::TBuffer<RawT>> SecondWriter = StaticCastSharedPtr<PCGExData::TBuffer<RawT>>(WorkingPair.SecondWriter);

					if (WorkingPair.bMultiplyByMinusOne)
					{
						for (int i = Scope.Start; i < Scope.End; i++)
						{
							const RawT FirstValue = FirstWriter->Read(i);
							FirstWriter->GetMutable(i) = PCGExMath::DblMult(SecondWriter->GetConst(i), -1);
							SecondWriter->GetMutable(i) = PCGExMath::DblMult(FirstValue, -1);
						}
					}
					else
					{
						for (int i = Scope.Start; i < Scope.End; i++)
						{
							const RawT FirstValue = FirstWriter->Read(i);
							FirstWriter->GetMutable(i) = SecondWriter->GetConst(i);
							SecondWriter->GetMutable(i) = FirstValue;
						}
					}
				});
		}
	}

	void FProcessor::CompleteWork()
	{
		if (bReversed)
		{
			if (!SwapPairs.IsEmpty()) { PointDataFacade->Write(AsyncManager); }
			if (Settings->bTagIfReversed) { PointDataFacade->Source->Tags->Add(Settings->IsReversedTag); }
		}
		else
		{
			if (Settings->bTagIfNotReversed) { PointDataFacade->Source->Tags->Add(Settings->IsNotReversedTag); }
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
