// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExInstruction.h"

void UPCGExInstruction::BindContext(FPCGExPointsProcessorContext* InContext)
{
	Context = InContext;
}

void UPCGExInstruction::UpdateUserFacingInfos()
{
}
