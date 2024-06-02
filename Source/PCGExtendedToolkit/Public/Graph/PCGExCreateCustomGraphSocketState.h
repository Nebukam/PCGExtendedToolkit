// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataStateFactoryProvider.h"

#include "Data/PCGExGraphDefinition.h"

#include "PCGExCreateCustomGraphSocketState.generated.h"

/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateCustomGraphSocketStateSettings : public UPCGExStateFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		GraphSocketState, "Socket State Definition", "Creates a socket state configuration from any number of sockets and attributes.",
		StateName)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorSocketState; }

#endif
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGSettings

public:
	/** List of tests to perform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, TitleProperty="{SocketName}", DisplayPriority=-1))
	TArray<FPCGExSocketTestDescriptor> Tests;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* Context, UPCGExParamFactoryBase* InFactory = nullptr) const override;
};
