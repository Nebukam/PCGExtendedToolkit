// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExOffsetPath.h"

#define LOCTEXT_NAMESPACE "PCGExOffsetPathElement"
#define PCGEX_NAMESPACE OffsetPath

#if WITH_EDITOR
void UPCGExOffsetPathSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExOffsetPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(OffsetPath)

void UPCGExOffsetPathSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(OffsetPathing, UPCGExMovingAverageOffsetPathing)
}

FPCGExOffsetPathContext::~FPCGExOffsetPathContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExOffsetPathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(OffsetPath)

	return true;
}

bool FPCGExOffsetPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOffsetPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(OffsetPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		int32 Index = 0;
		Context->MainPoints->ForEach(
			[&](PCGExData::FPointIO& PointIO, const int32)
			{
				if (PointIO.GetNum() > 2)
				{
					PointIO.CreateInKeys();
					Context->GetAsyncManager()->Start<FPCGExOffsetPathTask>(Index++, &PointIO);
				}
			});
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->OutputPoints();
		Context->Done();
	}

	return Context->IsDone();
}

bool FPCGExOffsetPathTask::ExecuteTask()
{
	const FPCGExOffsetPathContext* Context = Manager->GetContext<FPCGExOffsetPathContext>();
	PCGEX_SETTINGS(OffsetPath)

	const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = InPoints.Num();
	TArray<FVector> Positions;
	TArray<FVector> Normals;
	TArray<double> OffsetSize;
	TArray<FVector> Up;

	OffsetSize.SetNum(InPoints.Num());
	Up.SetNum(InPoints.Num());

	if (Settings->OffsetType == EPCGExFetchType::Attribute)
	{
		PCGEx::FLocalSingleFieldGetter* OffsetGetter = new PCGEx::FLocalSingleFieldGetter();
		OffsetGetter->Capture(Settings->OffsetAttribute);
		if (!OffsetGetter->Grab(*PointIO))
		{
			PCGEX_DELETE(OffsetGetter)
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing offset size attribute: {0}."), FText::FromName(Settings->OffsetAttribute.GetName())));
			return false;
		}

		for (int i = 0; i < OffsetSize.Num(); i++) { OffsetSize[i] = OffsetGetter->Values[i]; }
		PCGEX_DELETE(OffsetGetter)
	}
	else
	{
		for (int i = 0; i < OffsetSize.Num(); i++) { OffsetSize[i] = Settings->OffsetConstant; }
	}

	if (Settings->UpVectorType == EPCGExFetchType::Attribute)
	{
		PCGEx::FLocalVectorGetter* UpGetter = new PCGEx::FLocalVectorGetter();
		UpGetter->Capture(Settings->UpVectorAttribute);
		if (!UpGetter->Grab(*PointIO))
		{
			PCGEX_DELETE(UpGetter)
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing UpVector attribute: {0}."), FText::FromName(Settings->UpVectorAttribute.GetName())));
			return false;
		}

		for (int i = 0; i < Up.Num(); i++) { Up[i] = UpGetter->Values[i]; }
		PCGEX_DELETE(UpGetter)
	}
	else
	{
		for (int i = 0; i < Up.Num(); i++) { Up[i] = Settings->UpVectorConstant; }
	}


	Positions.SetNum(NumPoints);
	Normals.SetNum(NumPoints);

	for (int i = 0; i < NumPoints; i++) { Positions[i] = InPoints[i].Transform.GetLocation(); }

	auto NRM = [&](const int32 A, const int32 B, const int32 C)-> FVector
	{
		const FVector VA = Positions[A];
		const FVector VB = Positions[B];
		const FVector VC = Positions[C];
		const FVector UpAverage = ((Up[A] + Up[B] + Up[C]) / 3).GetSafeNormal();
		return FMath::Lerp(PCGExMath::GetNormal(VA, VB, VB + UpAverage), PCGExMath::GetNormal(VB, VC, VC + UpAverage), 0.5).GetSafeNormal();
	};

	const int32 LastIndex = NumPoints - 1;

	for (int i = 1; i < LastIndex; i++) { Normals[i] = NRM(i - 1, i, i + 1); }

	if (Settings->bClosedPath)
	{
		Normals[0] = NRM(LastIndex, 0, 1);
		Normals[LastIndex] = NRM(NumPoints - 2, LastIndex, 0);
	}
	else
	{
		Normals[0] = NRM(0, 0, 1);
		Normals[LastIndex] = NRM(NumPoints - 2, LastIndex, LastIndex);
	}

	TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
	for (int i = 0; i < NumPoints; i++)
	{
		MutablePoints[i].Transform.SetLocation(Positions[i] + (Normals[i].GetSafeNormal() * OffsetSize[i]));
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
