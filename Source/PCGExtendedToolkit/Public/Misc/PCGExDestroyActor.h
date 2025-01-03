// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExBufferHelper.h"

#include "PCGExDestroyActor.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDestroyActorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExDestroyActorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DestroyActor, "Destroy Actor", "Destroy target actor references that have been previously spawned by the PCG component this note is currently executing on.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings

public:
	virtual FName GetMainInputPin() const override;
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Actor reference */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ActorReferenceAttribute = FName(TEXT("ActorReference"));
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDestroyActorContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExDestroyActorElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDestroyActorElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExDestroyActors
{
	const FName SourceOverridesPacker = TEXT("Overrides : Packer");

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExDestroyActorContext, UPCGExDestroyActorSettings>
	{
		TSet<TSoftObjectPtr<AActor>> ActorsToDelete;
		TWeakPtr<PCGExMT::FAsyncToken> MainThreadToken;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;
		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
	};
}
