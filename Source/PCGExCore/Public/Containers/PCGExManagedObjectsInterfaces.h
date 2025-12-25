// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/



#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "UObject/UObjectGlobals.h"

#include "PCGExManagedObjectsInterfaces.generated.h"

class UPCGManagedComponent;

UINTERFACE()
class PCGEXCORE_API UPCGExManagedObjectInterface : public UInterface
{
	GENERATED_BODY()
};

class PCGEXCORE_API IPCGExManagedObjectInterface
{
	GENERATED_BODY()

public:
	virtual void Cleanup() = 0;
};

UINTERFACE()
class PCGEXCORE_API UPCGExManagedComponentInterface : public UInterface
{
	GENERATED_BODY()
};

class PCGEXCORE_API IPCGExManagedComponentInterface
{
	GENERATED_BODY()

public:
	virtual void SetManagedComponent(UPCGManagedComponent* InManagedComponent) = 0;
	virtual UPCGManagedComponent* GetManagedComponent() = 0;
};
