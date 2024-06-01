// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateCustomGraphSocketState.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExCreateCustomGraphSocketState"
#define PCGEX_NAMESPACE CreateCustomGraphSocketState

FName UPCGExCreateCustomGraphSocketStateSettings::GetMainOutputLabel() const { return PCGExGraph::OutputSocketStateLabel; }

UPCGExParamFactoryBase* UPCGExCreateCustomGraphSocketStateSettings::CreateFactory(FPCGContext* Context, UPCGExParamFactoryBase* InFactory) const
{
	if (!ValidateStateName(Context)) { return nullptr; }

	TArray<FPCGExSocketTestDescriptor> ValidDescriptors;

	for (const FPCGExSocketTestDescriptor& Descriptor : Tests)
	{
		if (!Descriptor.bEnabled) { continue; }
		if (Descriptor.SocketName.IsNone() || !FPCGMetadataAttributeBase::IsValidName(Descriptor.SocketName.ToString()))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("A socket name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
			continue;
		}

		ValidDescriptors.Add(Descriptor);
	}

	if (ValidDescriptors.IsEmpty()) { return nullptr; }

	UPCGExSocketStateFactory* OutState = CreateStateDefinition<UPCGExSocketStateFactory>(Context);
	OutState->Filters.Append(ValidDescriptors);

	return OutState;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
