// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExInternalDebug.h"

#define LOCTEXT_NAMESPACE "PCGExInternalDebugElement"
#define PCGEX_NAMESPACE InternalDebug

PCGExData::EInit UPCGExInternalDebugSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(InternalDebug)

FPCGExInternalDebugContext::~FPCGExInternalDebugContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExInternalDebugElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(InternalDebug)

	Context->GHTolerance = FVector(1 / Settings->GHTolerance.X, 1 / Settings->GHTolerance.Y, 1 / Settings->GHTolerance.Z);
	return true;
}

bool FPCGExInternalDebugElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExInternalDebugElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(InternalDebug)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
#define PCGEX_W(_NAME, _TYPE) FName _NAME##Name = FName(#_NAME); PCGEx:: TAttributeWriter<_TYPE>* _NAME##Writer = new PCGEx:: TAttributeWriter<_TYPE>(_NAME##Name); _NAME##Writer->BindAndSetNumUninitialized(Context->CurrentIO);
#define PCGEX_D(_NAME) _NAME##Writer->Write(); PCGEX_DELETE(_NAME##Writer)

		while (Context->AdvancePointsIO())
		{
			TArray<FPCGPoint>& OutPoints = Context->CurrentIO->GetOut()->GetMutablePoints();

			PCGEX_W(GH64, int32)

			for (int i = 0; i < OutPoints.Num(); ++i)
			{
				FPCGPoint& Point = OutPoints[i];
				GH64Writer->Values[i] = PCGEx::GH(Point.Transform.GetLocation(), Context->GHTolerance);
			}

			PCGEX_D(GH64)
		}

#undef PCGEX_W
#undef PCGEX_D

		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->MainPoints->OutputToContext();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
