// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByPointCount.h"

#include "PCGExPointsProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByPointCountElement"
#define PCGEX_NAMESPACE DiscardByPointCount

PCGExData::EInit UPCGExDiscardByPointCountSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGElementPtr UPCGExDiscardByPointCountSettings::CreateElement() const { return MakeShared<FPCGExDiscardByPointCountElement>(); }

bool FPCGExDiscardByPointCountElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDiscardByPointCountElement::Execute);

	PCGEX_CONTEXT(PointsProcessor)
	PCGEX_SETTINGS(DiscardByPointCount)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
	}

	const int32 Min = Settings->bRemoveBelow ? FMath::Max(1, Settings->MinPointCount) : 1;
	const int32 Max = Settings->bRemoveAbove ? FMath::Max(1, Settings->MaxPointCount) : TNumericLimits<int32>::Max();

	for (PCGExData::FPointIO* PointIO : Context->MainPoints->Pairs)
	{
		if (!FMath::IsWithin(PointIO->GetNum(), Min, Max)) { continue; }
		PointIO->InitializeOutput(PCGExData::EInit::Forward);
	}

	Context->OutputMainPoints();
	Context->Done();

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
