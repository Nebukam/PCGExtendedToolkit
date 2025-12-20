// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Metadata/PCGMetadataCommon.h"

namespace PCGExPath
{
	namespace Labels
	{
		PCGEX_CTX_STATE(State_BuildingPaths)

		const FName SourcePathsLabel = TEXT("Paths");
		const FName OutputPathsLabel = TEXT("Paths");

		const FName SourceCanCutFilters = TEXT("Can Cut Conditions");
		const FName SourceCanBeCutFilters = TEXT("Can Be Cut Conditions");
		const FName SourceTriggerFilters = TEXT("Trigger Conditions");
		const FName SourceShiftFilters = TEXT("Shift Conditions");

		const FPCGAttributeIdentifier ClosedLoopIdentifier = FPCGAttributeIdentifier(FName("IsClosed"), PCGMetadataDomainID::Data);
		const FPCGAttributeIdentifier HoleIdentifier = FPCGAttributeIdentifier(FName("IsHole"), PCGMetadataDomainID::Data);
	}
}
