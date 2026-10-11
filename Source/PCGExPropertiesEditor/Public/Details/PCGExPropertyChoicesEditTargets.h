// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPropertySchema.h"

#include "PCGExPropertyChoicesEditTargets.generated.h"

// Scratch roots for the choices popover. A structure view renders a root struct's MEMBERS, so the edited struct is
// wrapped as one member: that member row is what the registered type customization renders.

USTRUCT()
struct FPCGExPropertyChoicesEditTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExPropertyChoices Choices;
};

USTRUCT()
struct FPCGExPropertyChoicesPatchEditTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExPropertyChoicesPatch Patch;
};
