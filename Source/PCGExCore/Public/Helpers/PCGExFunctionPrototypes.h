// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/



#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "PCGExFunctionPrototypes.generated.h"

struct FPCGContext;
/** Holds function prototypes used to match against actor function signatures. */
UCLASS()
class PCGEXCORE_API UPCGExFunctionPrototypes : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UFunction* GetPrototypeWithNoParams() { return FindObject<UFunction>(StaticClass(), TEXT("PrototypeWithNoParams")); }

private:
	UFUNCTION()
	void PrototypeWithNoParams()
	{
	}
};

namespace PCGExHelpers
{
	PCGEXCORE_API TArray<UFunction*> FindUserFunctions(
		const TSubclassOf<AActor>& ActorClass,
		const TArray<FName>& FunctionNames,
		const TArray<const UFunction*>& FunctionPrototypes,
		const FPCGContext* InContext);
}
