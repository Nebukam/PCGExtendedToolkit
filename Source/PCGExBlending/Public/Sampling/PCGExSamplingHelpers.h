// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExData
{
	class FFacade;
}

class AActor;

struct FPCGContext;
enum class EPCGExAngleRange : uint8;

namespace PCGExSampling::Helpers
{
	PCGEXBLENDING_API
	double GetAngle(const EPCGExAngleRange Mode, const FVector& A, const FVector& B);

	PCGEXBLENDING_API
	bool GetIncludedActors(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InFacade, const FName ActorReferenceName, TMap<AActor*, int32>& OutActorSet);
}
