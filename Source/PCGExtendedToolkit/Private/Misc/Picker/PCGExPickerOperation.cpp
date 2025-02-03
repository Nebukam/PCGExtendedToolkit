// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/Pickers/PCGExPickerOperation.h"
#include "Misc/Pickers/PCGExPickerFactoryProvider.h"

void UPCGExPickerOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	//if (const UPCGExPickerOperation* TypedOther = Cast<UPCGExPickerOperation>(Other))	{	}
}

bool UPCGExPickerOperation::Init(FPCGExContext* InContext, const UPCGExPickerFactoryData* InFactory)
{
	Factory = InFactory;
	return true;
}

void UPCGExPickerOperation::AddPicks(const TSharedRef<PCGExData::FFacade>& InDataFacade, TSet<int32>& OutPicks) const
{
	// TOOD : Pick!
}

void UPCGExPickerPointOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	//if (const UPCGExPickerPointOperation* TypedOther = Cast<UPCGExPickerPointOperation>(Other))	{	}
}

bool UPCGExPickerPointOperation::Init(FPCGExContext* InContext, const UPCGExPickerFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}
