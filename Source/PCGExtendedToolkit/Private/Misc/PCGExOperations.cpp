// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExOperations.h"

#define LOCTEXT_NAMESPACE "PCGExOperations"

PCGExData::EInit UPCGExOperationsSettings::GetPointOutputInitMode() const { return PCGExData::EInit::NoOutput; }

UPCGExOperationsSettings::UPCGExOperationsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DebugSettings.PointScale = 0.0f;
}

#if WITH_EDITOR
void UPCGExOperationsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	DebugSettings.PointScale = 0.0f;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

FPCGElementPtr UPCGExOperationsSettings::CreateElement() const
{
	return MakeShared<FPCGExOperationsElement>();
}

void FPCGExOperationsContext::PrepareForPoints(const UPCGPointData* PointData)
{
	for (PCGEx::FOperation& Operation : Operations)
	{
		Operation.Validate(PointData);
	}
}

FPCGContext* FPCGExOperationsElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExOperationsContext* Context = new FPCGExOperationsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExOperationsSettings* Settings = Context->GetInputSettings<UPCGExOperationsSettings>();
	check(Settings);

	Context->Operations.Empty();
	for (const FPCGExOperationDescriptor& Descriptor : Settings->Applications)
	{
		if (!Descriptor.bEnabled) { continue; }
		PCGEx::FOperation& Drawer = Context->Operations.Emplace_GetRef();
		Drawer.Descriptor = &(const_cast<FPCGExOperationDescriptor&>(Descriptor));
	}

	return Context;
}

bool FPCGExOperationsElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	const FPCGExOperationsContext* Context = static_cast<FPCGExOperationsContext*>(InContext);

	if (Context->Operations.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MissingDebugInfos", "Debug list is empty."));
	}

	return true;
}

bool FPCGExOperationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOperationsElement::Execute);

#if  WITH_EDITOR

	FPCGExOperationsContext* Context = static_cast<FPCGExOperationsContext*>(InContext);

	const UPCGExOperationsSettings* Settings = Context->GetInputSettings<UPCGExOperationsSettings>();
	check(Settings);

	if (Context->IsSetup())
	{
		if (!Settings->bDebug) { return true; }
		if (!Validate(Context)) { return true; }

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->Done(); //No more points
		}
		else
		{
			Context->SetState(PCGExMT::State_ProcessingPoints);
		}
	}

	auto ProcessPoint = [&](const FPCGPoint& Point, const int32 ReadIndex, const PCGExData::FPointIO* PointIO)
	{
		// FWriteScopeLock WriteLock(Context->ContextLock);
		const FVector Start = Point.Transform.GetLocation();
		DrawDebugPoint(Context->World, Start, 1.0f, FColor::White, true);
		for (const PCGEx::FOperation& Operation : Context->Operations)
		{
			if (!Operation.bValid)
			{
			}
		}
	};

	auto Initialize = [&](const PCGExData::FPointIO* PointIO)
	{
		Context->PrepareForPoints(PointIO->GetIn());
	};

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		Initialize(Context->CurrentIO);
		for (int i = 0; i < Context->CurrentIO->GetNum(); i++) { ProcessPoint(Context->CurrentIO->GetInPoint(i), i, Context->CurrentIO); }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		//Context->OutputPoints();
		return true;
	}

#elif
	return  true;
#endif

	return false;
}

#undef LOCTEXT_NAMESPACE
