// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExShrinkPath.h"

#define LOCTEXT_NAMESPACE "PCGExShrinkPathElement"
#define PCGEX_NAMESPACE ShrinkPath

PCGExData::EInit UPCGExShrinkPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FName UPCGExShrinkPathSettings::GetPointFilterLabel() const { return FName("StopConditions"); }

PCGEX_INITIALIZE_ELEMENT(ShrinkPath)

bool FPCGExShrinkPathContext::PrepareFiltersWithAdvance() const { return false; }

FPCGExShrinkPathContext::~FPCGExShrinkPathContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExShrinkPathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ShrinkPath)

	if (Settings->ValueSource == EPCGExFetchType::Attribute) { PCGEX_VALIDATE_NAME(Settings->ShrinkAmount.GetName()) }

	return true;
}

bool FPCGExShrinkPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExShrinkPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ShrinkPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO())
		{
			Context->GetAsyncManager()->Start<FPCGExShrinkPathTask>(Context->CurrentIO->IOIndex, Context->CurrentIO);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

bool FPCGExShrinkPathTask::ExecuteTask()
{
	const FPCGExShrinkPathContext* Context = Manager->GetContext<FPCGExShrinkPathContext>();
	PCGEX_SETTINGS(ShrinkPath)

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	const int32 LastPointIndex = InPoints.Num() - 1;
	const int32 NumPoints = InPoints.Num();

	int32 StartOffset = 0;
	int32 EndOffset = 0;

	PCGExDataFilter::TEarlyExitFilterManager* Stops = Context->CreatePointFilterManagerInstance(PointIO);

	auto WrapUp = [&]()
	{
		PCGEX_DELETE(Stops)
	};

	if (Stops->bValid)
	{
		Stops->PrepareForTesting();
		if (Stops->RequiresPerPointPreparation())
		{
			for (int i = 0; i < NumPoints; i++) { Stops->PrepareSingle(i); }
			Stops->PreparationComplete();
		}
	}
	else
	{
		for (bool& Result : Stops->Results) { Result = false; }
	}

	if (Settings->bEndpointsIgnoreStopConditions)
	{
		switch (Settings->ShrinkEndpoint)
		{
		default: ;
		case EPCGExShrinkEndpoint::Both:
			Stops->Results[0] = false;
			Stops->Results[LastPointIndex] = false;
			break;
		case EPCGExShrinkEndpoint::Start:
			Stops->Results[0] = false;
			break;
		case EPCGExShrinkEndpoint::End:
			Stops->Results[LastPointIndex] = false;
			break;
		}
	}

	if (Settings->ShrinkMode == EPCGExPathShrinkMode::Count)
	{
		uint32 StartAmount = 0;
		uint32 EndAmount = 0;

		if (Settings->ValueSource == EPCGExFetchType::Attribute)
		{
			PCGEx::FLocalIntegerGetter* Getter = new PCGEx::FLocalIntegerGetter();
			if (!Getter->SoftGrab(*PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Could not read Shrink value attribute on some inputs.")); }
			StartAmount = Getter->SoftGet(InPoints[0], 0);
			EndAmount = Getter->SoftGet(InPoints.Last(), 0);
			PCGEX_DELETE(Getter);
		}
		else
		{
			StartAmount = EndAmount = Settings->CountConstant;
		}

		if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::Start || Stops->Results[LastPointIndex]) { EndAmount = 0; }
		if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::End || Stops->Results[0]) { StartAmount = 0; }

		if (StartAmount == 0 && EndAmount == 0)
		{
			PointIO->InitializeOutput(PCGExData::EInit::Forward);
			WrapUp();
			return false;
		}

		PointIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

		auto ShrinkBy = [&](int32 Amount)
		{
			if (Amount == 0 || MutablePoints.IsEmpty() || Stops->Results[StartOffset]) { return; }

			if (FMath::Abs(Amount) > MutablePoints.Num())
			{
				MutablePoints.Empty();
				return;
			}

			if (Amount > 0)
			{
				for (int i = 0; i < Amount; i++)
				{
					if (Stops->Results[StartOffset]) { break; }
					MutablePoints.RemoveAt(0);
					StartOffset++;
				}
			}
			else
			{
				for (int i = 0; i < FMath::Abs(Amount); i++)
				{
					if (Stops->Results.Last(EndOffset)) { break; }
					MutablePoints.RemoveAt(MutablePoints.Num() - (EndOffset++));
				}
			}
		};

		switch (Settings->ShrinkFirst)
		{
		default: ;
		case EPCGExShrinkEndpoint::Both:
			while ((StartAmount + EndAmount) != 0)
			{
				if (StartAmount > 0) { ShrinkBy(1); }
				if (EndAmount > 0) { ShrinkBy(-1); }
				StartAmount--;
				EndAmount--;
			}
			break;
		case EPCGExShrinkEndpoint::Start:
			for (int i = 0; i < StartAmount; i++) { ShrinkBy(1); }
			for (int i = 0; i < EndAmount; i++) { ShrinkBy(-1); }
			break;
		case EPCGExShrinkEndpoint::End:
			for (int i = 0; i < EndAmount; i++) { ShrinkBy(-1); }
			for (int i = 0; i < StartAmount; i++) { ShrinkBy(1); }
			break;
		}
	}
	else
	{
		double StartAmount = 0;
		double EndAmount = 0;

		if (Settings->ValueSource == EPCGExFetchType::Attribute)
		{
			PCGEx::FLocalSingleFieldGetter* Getter = new PCGEx::FLocalSingleFieldGetter();
			if (!Getter->SoftGrab(*PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Could not read Shrink value attribute on some inputs.")); }
			StartAmount = Getter->SoftGet(InPoints[0], 0);
			EndAmount = Getter->SoftGet(InPoints.Last(), 0);
			PCGEX_DELETE(Getter);
		}
		else
		{
			StartAmount = EndAmount = Settings->DistanceConstant;
		}

		if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::Start || Stops->Results[LastPointIndex]) { EndAmount = 0; }
		if (Settings->ShrinkEndpoint == EPCGExShrinkEndpoint::End || Stops->Results[0]) { StartAmount = 0; }

		if (StartAmount == 0 && EndAmount == 0)
		{
			PointIO->InitializeOutput(PCGExData::EInit::Forward);
			WrapUp();
			return false;
		}

		PointIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

		if ((StartAmount < 0 || EndAmount < 0) && NumPoints >= 2)
		{
			if (StartAmount < 0)
			{
				const FVector Pos = MutablePoints[0].Transform.GetLocation();
				const FVector Direction = (Pos - MutablePoints[1].Transform.GetLocation()).GetSafeNormal() * StartAmount;
				MutablePoints[0].Transform.SetLocation(Pos + Direction);
				StartAmount = 0;
			}

			if (EndAmount < 0)
			{
				const FVector Pos = MutablePoints.Last().Transform.GetLocation();
				const FVector Direction = (Pos - MutablePoints[LastPointIndex - 1].Transform.GetLocation()).GetSafeNormal() * EndAmount;
				MutablePoints[0].Transform.SetLocation(Pos + Direction);
				EndAmount = 0;
			}

			auto ShrinkBy = [&](double Distance)
			{
				if (Distance == 0 || MutablePoints.IsEmpty() || Stops->Results[StartOffset]) { return 0; }

				if (MutablePoints.Num() <= 1)
				{
					MutablePoints.Empty();
					return 0;
				}

				FVector From;
				FVector To;
				int32 Index;

				if (Distance > 0)
				{
					Index = 0;
					From = MutablePoints[Index].Transform.GetLocation();
					To = MutablePoints[Index + 1].Transform.GetLocation();
				}
				else
				{
					Index = MutablePoints.Num() - 1;
					From = MutablePoints[Index].Transform.GetLocation();
					To = MutablePoints[Index - 1].Transform.GetLocation();
				}

				const double AvailableDistance = FVector::Dist(From, To);
				if(Distance > AvailableDistance)
				{
					MutablePoints.RemoveAt(Index);
					return Distance - AvailableDistance;
				}

				
				double Remainder = 0;

				return Distance;
			};

			switch (Settings->ShrinkFirst)
			{
			default: ;
			case EPCGExShrinkEndpoint::Both:
				while ((StartAmount + EndAmount) != 0)
				{
					StartAmount = ShrinkBy(StartAmount);
					EndAmount = ShrinkBy(-EndAmount);
				}
				break;
			case EPCGExShrinkEndpoint::Start:
				while (StartAmount > 0) { StartAmount = ShrinkBy(StartAmount); }
				while (EndAmount > 0) { EndAmount = ShrinkBy(-EndAmount); }
				break;
			case EPCGExShrinkEndpoint::End:
				while (EndAmount > 0) { EndAmount = ShrinkBy(-EndAmount); }
				while (StartAmount > 0) { StartAmount = ShrinkBy(StartAmount); }
				break;
			}
		}
	}

	WrapUp();
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
