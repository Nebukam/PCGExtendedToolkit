// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateCustomGraphSocketState.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExCreateCustomGraphSocketState"
#define PCGEX_NAMESPACE CreateCustomGraphSocketState

FName UPCGExCreateCustomGraphSocketStateSettings::GetMainOutputLabel() const { return PCGExGraph::OutputSocketStateLabel; }

FPCGElementPtr UPCGExCreateCustomGraphSocketStateSettings::CreateElement() const { return MakeShared<FPCGExCreateCustomGraphSocketStateElement>(); }

#if WITH_EDITOR
void UPCGExCreateCustomGraphSocketStateSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExCreateCustomGraphSocketStateElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateCustomGraphSocketStateElement::Execute);

	PCGEX_SETTINGS(CreateCustomGraphSocketState)

	if (!Boot(Context)) { return true; }

	UPCGExSocketStateDefinition* OutState = CreateStateDefinition<UPCGExSocketStateDefinition>(Context);

	for (const FPCGExSocketTestDescriptor& Descriptor : Settings->Tests)
	{
		if (!Descriptor.bEnabled) { continue; }

		if (Descriptor.SocketName.IsNone() || !FPCGMetadataAttributeBase::IsValidName(Descriptor.SocketName.ToString()))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("A socket name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
			continue;
		}

		OutState->Tests.Add(Descriptor);
	}

	if (OutState->Tests.IsEmpty())
	{
		OutState->ConditionalBeginDestroy();
		return true;
	}

	OutputState(Context, OutState);

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
