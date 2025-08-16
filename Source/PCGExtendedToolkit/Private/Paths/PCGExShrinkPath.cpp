// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExShrinkPath.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExShrinkPathElement"
#define PCGEX_NAMESPACE ShrinkPath

UPCGExShrinkPathSettings::UPCGExShrinkPathSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportClosedLoops = false;
}

PCGEX_INITIALIZE_ELEMENT(ShrinkPath)
PCGEX_ELEMENT_BATCH_POINT_IMPL(ShrinkPath)

void FPCGExShrinkPathContext::GetShrinkAmounts(const TSharedRef<PCGExData::FPointIO>& PointIO, double& Start, double& End, EPCGExPathShrinkDistanceCutType& StartCut, EPCGExPathShrinkDistanceCutType& EndCut) const
{
	PCGEX_SETTINGS_LOCAL(ShrinkPath)

	StartCut = Settings->PrimaryDistanceDetails.CutType;
	EndCut = Settings->PrimaryDistanceDetails.CutType;

	constexpr int32 StartIndex = 0;
	const int32 EndIndex = PointIO->GetNum() - 1;

	if (Settings->PrimaryDistanceDetails.AmountInput == EPCGExInputValueType::Attribute)
	{
		const TUniquePtr<PCGEx::TAttributeBroadcaster<double>> Getter = MakeUnique<PCGEx::TAttributeBroadcaster<double>>();
		if (!Getter->Prepare(Settings->PrimaryDistanceDetails.DistanceAttribute, PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Could not read primary Distance value attribute on some inputs.")); }

		Start = Getter->FetchSingle(PointIO->GetInPoint(StartIndex), 0);
		End = Getter->FetchSingle(PointIO->GetInPoint(EndIndex), 0);
	}
	else
	{
		Start = End = Settings->PrimaryDistanceDetails.Distance;
	}

	if (Settings->SettingsMode == EPCGExShrinkConstantMode::Separate)
	{
		EndCut = Settings->SecondaryDistanceDetails.CutType;

		if (Settings->SecondaryDistanceDetails.AmountInput == EPCGExInputValueType::Attribute)
		{
			const TUniquePtr<PCGEx::TAttributeBroadcaster<double>> Getter = MakeUnique<PCGEx::TAttributeBroadcaster<double>>();
			if (!Getter->Prepare(Settings->SecondaryDistanceDetails.DistanceAttribute, PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Could not read secondary Distance attribute on some inputs.")); }
			End = Getter->FetchSingle(PointIO->GetInPoint(EndIndex), 0);
		}
		else
		{
			End = Settings->SecondaryDistanceDetails.Distance;
		}
	}
}

void FPCGExShrinkPathContext::GetShrinkAmounts(const TSharedRef<PCGExData::FPointIO>& PointIO, uint32& Start, uint32& End) const
{
	PCGEX_SETTINGS_LOCAL(ShrinkPath)

	constexpr int32 StartIndex = 0;
	const int32 EndIndex = PointIO->GetNum() - 1;

	if (Settings->PrimaryCountDetails.ValueSource == EPCGExInputValueType::Attribute)
	{
		const TUniquePtr<PCGEx::TAttributeBroadcaster<int32>> Getter = MakeUnique<PCGEx::TAttributeBroadcaster<int32>>();
		if (!Getter->Prepare(Settings->PrimaryCountDetails.CountAttribute, PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Could not read primary Distance value attribute on some inputs.")); }
		Start = Getter->FetchSingle(PointIO->GetInPoint(StartIndex), 0);
		End = Getter->FetchSingle(PointIO->GetInPoint(EndIndex), 0);
	}
	else
	{
		Start = End = Settings->PrimaryCountDetails.Count;
	}

	if (Settings->SettingsMode == EPCGExShrinkConstantMode::Separate)
	{
		if (Settings->SecondaryCountDetails.ValueSource == EPCGExInputValueType::Attribute)
		{
			const TUniquePtr<PCGEx::TAttributeBroadcaster<int32>> Getter = MakeUnique<PCGEx::TAttributeBroadcaster<int32>>();
			if (!Getter->Prepare(Settings->PrimaryCountDetails.CountAttribute, PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, this, FTEXT("Could not read secondary Distance attribute on some inputs.")); }
			End = Getter->FetchSingle(PointIO->GetInPoint(EndIndex), 0);
		}
		else
		{
			End = Settings->SecondaryCountDetails.Count;
		}
	}
}

bool FPCGExShrinkPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ShrinkPath)

	if (Settings->ShrinkMode == EPCGExPathShrinkMode::Count)
	{
		if (!Settings->PrimaryCountDetails.SanityCheck(Context)) { return false; }
		if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::Both && Settings->SettingsMode == EPCGExShrinkConstantMode::Separate)
		{
			if (!Settings->SecondaryCountDetails.SanityCheck(Context)) { return false; }
		}
	}
	else
	{
		if (!Settings->PrimaryDistanceDetails.SanityCheck(Context)) { return false; }
		if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::Both && Settings->SettingsMode == EPCGExShrinkConstantMode::Separate)
		{
			if (!Settings->SecondaryDistanceDetails.SanityCheck(Context)) { return false; }
		}
	}

	return true;
}

bool FPCGExShrinkPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExShrinkPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ShrinkPath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to shrink."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExShrinkPath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExShrinkPath::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		const TSharedRef<PCGExData::FPointIO> PointIO = PointDataFacade->Source;

		ON_SCOPE_EXIT
		{
			PCGEX_ASYNC_CHKD_VOID(AsyncManager)

			if (PointIO->GetOut() && PointIO->GetIn() != PointIO->GetOut() && PointIO->GetNum(PCGExData::EIOSide::Out) <= 1)
			{
				PointIO->InitializeOutput(PCGExData::EIOInit::NoInit);
			}
		};

		const UPCGBasePointData* InPoints = PointIO->GetIn();
		const int32 NumPoints = InPoints->GetNumPoints();
		const int32 LastPointIndex = NumPoints - 1;

		FilterScope(PCGExMT::FScope(0, NumPoints));

		int32 StartOffset = 0;
		int32 EndOffset = 1;

		EPCGExShrinkEndpoint SafeShrinkFirst = Settings->ShrinkFirst;

		if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::Start) { SafeShrinkFirst = EPCGExShrinkEndpoint::Start; }
		else if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::End) { SafeShrinkFirst = EPCGExShrinkEndpoint::End; }

		if (Settings->bEndpointsIgnoreStopConditions)
		{
			switch (SafeShrinkFirst)
			{
			default: ;
			case EPCGExShrinkEndpoint::Both:
				PointFilterCache[0] = false;
				PointFilterCache[LastPointIndex] = false;
				break;
			case EPCGExShrinkEndpoint::Start:
				PointFilterCache[0] = false;
				break;
			case EPCGExShrinkEndpoint::End:
				PointFilterCache[LastPointIndex] = false;
				break;
			}
		}

		if (Settings->ShrinkMode == EPCGExPathShrinkMode::Count)
		{
			uint32 StartAmount = 0;
			uint32 EndAmount = 0;

			Context->GetShrinkAmounts(PointIO, StartAmount, EndAmount);

			if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::Start || PointFilterCache[LastPointIndex]) { EndAmount = 0; }
			if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::End || PointFilterCache[0]) { StartAmount = 0; }

			// Just so we avoid wasting cycles... Not that this whole code is that efficient anyway
			StartAmount = FMath::Min(StartAmount, static_cast<uint32>(NumPoints));
			EndAmount = FMath::Min(EndAmount, static_cast<uint32>(NumPoints));

			if (StartAmount == 0 && EndAmount == 0)
			{
				PointIO->InitializeOutput(PCGExData::EIOInit::Forward);
				return false;
			}

			TArray<int32> KeptIndices;
			PCGEx::ArrayOfIndices(KeptIndices, NumPoints);

			auto ShrinkOnce = [&](const int32 Direction)
			{
				if (Direction == 0 || KeptIndices.IsEmpty()) { return; }

				int32 RemoveIndex = -1;

				if (Direction > 0)
				{
					if (PointFilterCache[StartOffset]) { return; }

					RemoveIndex = 0;
					StartOffset++;
				}
				else
				{
					if (PointFilterCache[PointFilterCache.Num() - EndOffset]) { return; }

					const int32 LastIndex = KeptIndices.Num() - 1;
					if (KeptIndices.IsValidIndex(LastIndex)) { RemoveIndex = LastIndex; }
					EndOffset++;
				}

				if (RemoveIndex != -1) { KeptIndices.RemoveAt(RemoveIndex); }
			};

			switch (SafeShrinkFirst)
			{
			default: ;
			case EPCGExShrinkEndpoint::Both:
				while (StartAmount > 0 || EndAmount > 0)
				{
					if (StartAmount > 0) { ShrinkOnce(1); }
					if (EndAmount > 0) { ShrinkOnce(-1); }
					StartAmount--;
					EndAmount--;
				}
				break;
			case EPCGExShrinkEndpoint::Start:
				for (uint32 i = 0; i < StartAmount; i++) { ShrinkOnce(1); }
				if (!KeptIndices.IsEmpty() && EndAmount > 0) { for (uint32 i = 0; i < EndAmount; i++) { ShrinkOnce(-1); } }
				break;
			case EPCGExShrinkEndpoint::End:
				for (uint32 i = 0; i < EndAmount; i++) { ShrinkOnce(-1); }
				if (!KeptIndices.IsEmpty() && StartAmount > 0) { for (uint32 i = 0; i < StartAmount; i++) { ShrinkOnce(1); } }
				break;
			}

			if (KeptIndices.Num() >= 2)
			{
				if (KeptIndices.Num() == NumPoints)
				{
					PCGEX_INIT_IO(PointIO, PCGExData::EIOInit::Forward)
				}
				else
				{
					if (KeptIndices[0] == 0)
					{
						// Only removed point from the end, no need to copy any data
						// just update the points count
						PCGEX_INIT_IO(PointIO, PCGExData::EIOInit::Duplicate)
						PointIO->GetOut()->SetNumPoints(KeptIndices.Num());
					}
					else
					{
						PCGEX_INIT_IO(PointIO, PCGExData::EIOInit::New)
						PCGEx::SetNumPointsAllocated(PointIO->GetOut(), KeptIndices.Num(), PointIO->GetAllocations());
						PointIO->InheritPoints(KeptIndices, 0);
					}
				}
			}
		}
		else
		{
			// BUG : With path of exactly 3 points, something goes wrong

			double StartAmount = 0;
			double EndAmount = 0;

			EPCGExPathShrinkDistanceCutType StartCut;
			EPCGExPathShrinkDistanceCutType EndCut;

			Context->GetShrinkAmounts(PointIO, StartAmount, EndAmount, StartCut, EndCut);

			if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::Start || PointFilterCache[LastPointIndex]) { EndAmount = 0; }
			if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::End || PointFilterCache[0]) { StartAmount = 0; }

			if (StartAmount == 0 && EndAmount == 0)
			{
				PointIO->InitializeOutput(PCGExData::EIOInit::Forward);
				return false;
			}

			TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

			FVector StartPosition = InTransforms[0].GetLocation();
			FVector EndPosition = InTransforms[InTransforms.Num() - 1].GetLocation();

			TArray<int32> KeptIndices;
			PCGEx::ArrayOfIndices(KeptIndices, NumPoints);

			if ((StartAmount < 0 || EndAmount < 0) && NumPoints >= 2)
			{
				if (StartAmount < 0)
				{
					const FVector Pos = InTransforms[0].GetLocation();
					const FVector Direction = (InTransforms[1].GetLocation() - Pos).GetSafeNormal() * StartAmount;
					StartPosition = Pos + Direction;
					StartAmount = 0;
				}

				if (EndAmount < 0)
				{
					const FVector Pos = InTransforms[InTransforms.Num() - 1].GetLocation();
					const FVector Direction = (InTransforms[InTransforms.Num() - 2].GetLocation() - Pos).GetSafeNormal() * EndAmount;
					EndPosition = Pos + Direction;
					EndAmount = 0;
				}
			}

			if (StartAmount != 0 || EndAmount != 0)
			{
				auto ShrinkBy = [&](double Distance, const EPCGExPathShrinkDistanceCutType CutType, FVector& OutPosition)-> double
				{
					if (Distance == 0 || KeptIndices.IsEmpty()) { return 0; }

					if (KeptIndices.Num() <= 1)
					{
						KeptIndices.Empty();
						return 0;
					}

					FVector From;
					FVector To;
					int32 Index;

					if (Distance > 0)
					{
						Index = 0;
						if (PointFilterCache[Index]) { return 0; }

						From = InTransforms[KeptIndices[Index]].GetLocation();
						To = InTransforms[KeptIndices[Index + 1]].GetLocation();
					}
					else
					{
						Index = KeptIndices.Num() - 1;
						if (PointFilterCache[Index]) { return 0; }

						From = InTransforms[KeptIndices[Index]].GetLocation();
						To = InTransforms[KeptIndices[Index - 1]].GetLocation();
						Distance = FMath::Abs(Distance);
					}

					const double AvailableDistance = FVector::Dist(From, To);
					if (Distance >= AvailableDistance)
					{
						KeptIndices.RemoveAt(Index);
						return Distance - AvailableDistance;
					}

					if (Distance < AvailableDistance)
					{
						switch (CutType)
						{
						default: ;
						case EPCGExPathShrinkDistanceCutType::NewPoint:
							OutPosition = FMath::Lerp(From, To, Distance / AvailableDistance);
							break;
						case EPCGExPathShrinkDistanceCutType::Previous:
							// Do nothing
							break;
						case EPCGExPathShrinkDistanceCutType::Next:
							KeptIndices.RemoveAt(Index);
							break;
						case EPCGExPathShrinkDistanceCutType::Closest:
							if (AvailableDistance / Distance > 0.5) { KeptIndices.RemoveAt(Index); }
							break;
						}
					}

					return 0;
				};

				switch (SafeShrinkFirst)
				{
				default: ;
				case EPCGExShrinkEndpoint::Both:
					while ((StartAmount + EndAmount) != 0)
					{
						if (StartAmount > 0) { StartAmount = ShrinkBy(StartAmount, StartCut, StartPosition); }
						if (EndAmount > 0) { EndAmount = ShrinkBy(-EndAmount, EndCut, EndPosition); }
					}
					break;
				case EPCGExShrinkEndpoint::Start:
					while (StartAmount > 0) { StartAmount = ShrinkBy(StartAmount, StartCut, StartPosition); }
					if (!KeptIndices.IsEmpty()) { while (EndAmount > 0) { EndAmount = ShrinkBy(-EndAmount, EndCut, EndPosition); } }
					break;
				case EPCGExShrinkEndpoint::End:
					while (EndAmount > 0) { EndAmount = ShrinkBy(-EndAmount, StartCut, StartPosition); }
					if (!KeptIndices.IsEmpty()) { while (StartAmount > 0) { StartAmount = ShrinkBy(StartAmount, EndCut, EndPosition); } }
					break;
				}
			}

			if (KeptIndices.Num() >= 2)
			{
				if (NumPoints == KeptIndices.Num())
				{
					PCGEX_INIT_IO(PointIO, PCGExData::EIOInit::Duplicate)
					PointIO->InheritProperties(KeptIndices, EPCGPointNativeProperties::Transform);

					TPCGValueRange<FTransform> OutTransforms = PointIO->GetOut()->GetTransformValueRange(false);

					OutTransforms[0].SetLocation(StartPosition);
					OutTransforms[OutTransforms.Num() - 1].SetLocation(EndPosition);
				}
				else
				{
					PCGEX_INIT_IO(PointIO, PCGExData::EIOInit::New)
					PCGEx::SetNumPointsAllocated(PointIO->GetOut(), KeptIndices.Num(), PointIO->GetAllocations());
					PointIO->InheritPoints(KeptIndices, 0);
				}
			}
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
