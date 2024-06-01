// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCreateCustomGraphSocket.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExCreateCustomGraphSocket"
#define PCGEX_NAMESPACE CreateCustomGraphSocket

FName UPCGExCreateCustomGraphSocketSettings::GetMainOutputLabel() const { return PCGExGraph::OutputSocketParamsLabel; }

UPCGExParamFactoryBase* UPCGExCreateCustomGraphSocketSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	if (Socket.SocketName.IsNone() || !FPCGMetadataAttributeBase::IsValidName(Socket.SocketName.ToString()))
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Output name is invalid; Cannot be 'None' and can only contain the following special characters:[ ],[_],[-],[/]"));
		return nullptr;
	}

	UPCGExSocketFactory* OutParams = NewObject<UPCGExSocketFactory>();
	OutParams->Descriptor = FPCGExSocketDescriptor(Socket);

	return OutParams;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
