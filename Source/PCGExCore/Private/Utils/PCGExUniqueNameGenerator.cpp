// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Utils/PCGExUniqueNameGenerator.h"
#include "CoreMinimal.h"

FName FPCGExUniqueNameGenerator::Get(const FString& BaseName)
{
	FName OutName = FName(BaseName + "_" + FString::Printf(TEXT("%d"), Idx));
	FPlatformAtomics::InterlockedIncrement(&Idx);
	return OutName;
}

FName FPCGExUniqueNameGenerator::Get(const FName& BaseName)
{
	return Get(BaseName.ToString());
}
