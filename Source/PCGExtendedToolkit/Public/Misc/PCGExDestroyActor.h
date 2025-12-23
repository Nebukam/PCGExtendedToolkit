// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExPointsProcessor.h"
#include "PCGExDestroyActor.generated.h"

namespace PCGExMT
{
	class FAsyncToken;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling", meta=(PCGExNodeLibraryDoc="misc/destroy-actor"))
class UPCGExDestroyActorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExDestroyActorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DestroyActor, "Destroy Actor", "Destroy target actor references that have been previously spawned by the PCG component this note is currently executing on.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(MiscRemove); }
#endif

protected:
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

struct FPCGExDestroyActorContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExDestroyActorElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExDestroyActorElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(DestroyActor)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExDestroyActor
{
	const FName SourceOverridesPacker = TEXT("Overrides : Packer");

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExDestroyActorContext, UPCGExDestroyActorSettings>
	{
		TSet<TSoftObjectPtr<AActor>> ActorsToDelete;
		TWeakPtr<PCGExMT::FAsyncToken> MainThreadToken;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;
		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
	};
}
