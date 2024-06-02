// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSmooth.h"

#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#define LOCTEXT_NAMESPACE "PCGExSmoothElement"
#define PCGEX_NAMESPACE Smooth

#if WITH_EDITOR
void UPCGExSmoothSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (SmoothingMethod) { SmoothingMethod->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExSmoothSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(Smooth)

void UPCGExSmoothSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(Smoothing, UPCGExMovingAverageSmoothing)
}

FPCGExSmoothContext::~FPCGExSmoothContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSmoothElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)
	PCGEX_OPERATION_BIND(SmoothingMethod, UPCGExMovingAverageSmoothing)

	return true;
}

bool FPCGExSmoothElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSmoothElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)

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
					PointIO.CreateOutKeys();
					Context->GetAsyncManager()->Start<FPCGExSmoothTask>(Index++, &PointIO);
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

bool FPCGExSmoothTask::ExecuteTask()
{
	const FPCGExSmoothContext* Context = Manager->GetContext<FPCGExSmoothContext>();
	PCGEX_SETTINGS(Smooth)

	TArray<double> Influence;
	TArray<double> SmoothingAmount;
	Influence.SetNum(PointIO->GetNum());
	SmoothingAmount.SetNum(PointIO->GetNum());

	if (Settings->InfluenceType == EPCGExFetchType::Attribute)
	{
		PCGEx::FLocalSingleFieldGetter* InfluenceGetter = new PCGEx::FLocalSingleFieldGetter();
		InfluenceGetter->Capture(Settings->InfluenceAttribute);
		if (!InfluenceGetter->Grab(*PointIO))
		{
			PCGEX_DELETE(InfluenceGetter)
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing influence attribute: {0}."), FText::FromName(Settings->InfluenceAttribute.GetName())));
			return false;
		}

		for (int i = 0; i < Influence.Num(); i++) { Influence[i] = InfluenceGetter->Values[i]; }
		PCGEX_DELETE(InfluenceGetter)
	}
	else
	{
		for (int i = 0; i < Influence.Num(); i++) { Influence[i] = Settings->InfluenceConstant; }
	}

	if (Settings->SmoothingAmountType == EPCGExFetchType::Attribute)
	{
		PCGEx::FLocalSingleFieldGetter* SmoothingAmountGetter = new PCGEx::FLocalSingleFieldGetter();
		SmoothingAmountGetter->Capture(Settings->InfluenceAttribute);
		if (!SmoothingAmountGetter->Grab(*PointIO))
		{
			PCGEX_DELETE(SmoothingAmountGetter)
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing smoothing amount attribute: {0}."), FText::FromName(Settings->InfluenceAttribute.GetName())));
			return false;
		}

		for (int i = 0; i < SmoothingAmount.Num(); i++) { SmoothingAmount[i] = FMath::Clamp(SmoothingAmountGetter->Values[i], 0, TNumericLimits<double>::Max()) * Settings->ScaleSmoothingAmountAttribute; }
		PCGEX_DELETE(SmoothingAmountGetter)
	}
	else
	{
		for (int i = 0; i < SmoothingAmount.Num(); i++) { SmoothingAmount[i] = Settings->SmoothingAmountConstant; }
	}

	if (Settings->bPreserveStart) { Influence[0] = 0; }
	if (Settings->bPreserveEnd) { Influence[Influence.Num() - 1] = 0; }

	Context->SmoothingMethod->DoSmooth(*PointIO, &SmoothingAmount, &Influence, Settings->bClosedPath, &Settings->BlendingSettings);
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
