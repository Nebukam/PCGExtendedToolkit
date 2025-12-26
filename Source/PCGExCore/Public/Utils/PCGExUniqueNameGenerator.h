// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/



#pragma once

#include "CoreMinimal.h"

class PCGEXCORE_API FPCGExUniqueNameGenerator final : public TSharedFromThis<FPCGExUniqueNameGenerator>
{
	int32 Idx = 0;

public:
	FPCGExUniqueNameGenerator() = default;
	~FPCGExUniqueNameGenerator() = default;

	FName Get(const FString& BaseName);
	FName Get(const FName& BaseName);
};
