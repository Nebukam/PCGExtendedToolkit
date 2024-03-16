// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExCreateState.h"

#include "Data/PCGExGraphDefinition.h"

#include "PCGExCreateCustomGraphSocketState.generated.h"

/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateCustomGraphSocketStateSettings : public UPCGExCreateStateSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		GraphSocketState, "Socket State Definition", "Creates a socket state configuration from any number of sockets and attributes.",
		StateName)

#endif
	virtual FName GetMainOutputLabel() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	/** List of tests to perform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{SocketName}", DisplayPriority=-1))
	TArray<FPCGExSocketTestDescriptor> Tests;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCreateCustomGraphSocketStateElement : public FPCGExCreateStateElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
