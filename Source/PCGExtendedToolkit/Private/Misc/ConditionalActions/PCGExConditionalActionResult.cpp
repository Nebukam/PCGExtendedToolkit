// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/ConditionalActions/PCGExConditionalActionResult.h"


#define LOCTEXT_NAMESPACE "PCGExWriteConditionalActionResults"
#define PCGEX_NAMESPACE PCGExWriteConditionalActionResults


void UPCGExConditionalActionResultOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExConditionalActionResultOperation* TypedOther = Cast<UPCGExConditionalActionResultOperation>(Other))
	{
	}
}

bool UPCGExConditionalActionResultOperation::PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!Super::PrepareForData(InContext, InPointDataFacade)) { return false; }
	ResultWriter = InPointDataFacade->GetWritable(TypedFactory->ResultAttributeName, false, true, PCGExData::EBufferInit::New);
	return true;
}

void UPCGExConditionalActionResultOperation::Cleanup()
{
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExConditionalActionResultProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(ConditionalActionResult, {})

bool UPCGExConditionalActionResultFactory::Boot(FPCGContext* InContext)
{
	PCGEX_VALIDATE_NAME_C(InContext, ResultAttributeName)
	return true;
}

PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(ConditionalActionResult, { NewFactory->ResultAttributeName = ResultAttributeName; })


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
