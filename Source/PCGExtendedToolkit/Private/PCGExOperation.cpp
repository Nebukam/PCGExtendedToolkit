// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExOperation.h"

#include "PCGExPointsProcessor.h"
#include "PCGParamData.h"

void UPCGExOperation::BindContext(FPCGExPointsProcessorContext* InContext)
{
	Context = InContext;

	TArray<FPCGTaggedData> OverrideParams = Context->InputData.GetParamsByPin(PCGPinConstants::DefaultParamsLabel);

	for (FPCGTaggedData& InTaggedData : OverrideParams)
	{
		const UPCGParamData* ParamData = Cast<UPCGParamData>(InTaggedData.Data);

		if (!ParamData) { continue; }
		PCGEx::FAttributesInfos* Infos = PCGEx::FAttributesInfos::Get(ParamData->Metadata);

		for (PCGEx::FAttributeIdentity& Identity : Infos->Identities)
		{
			PossibleOverrides.Add(Identity.Name, ParamData->Metadata->GetMutableAttribute(Identity.Name));
		}

		PCGEX_DELETE(Infos)
	}

	ApplyOverrides();

	PossibleOverrides.Empty();
}

#if WITH_EDITOR
void UPCGExOperation::UpdateUserFacingInfos()
{
}
#endif

void UPCGExOperation::Cleanup()
{
	Context = nullptr;
}

void UPCGExOperation::Write()
{
}

void UPCGExOperation::BeginDestroy()
{
	Cleanup();
	UObject::BeginDestroy();
}

void UPCGExOperation::ApplyOverrides()
{
}
