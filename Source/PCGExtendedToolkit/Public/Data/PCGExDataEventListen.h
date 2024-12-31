// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGEx.h"
#include "PCGExPointsProcessor.h"
#include "UPCGExSubSystem.h"

#include "PCGExDataEventListen.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDataEventListenSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExDataEventListenElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_DUMMY_SETTINGS_MEMBERS
	PCGEX_NODE_INFOS(DataEventListen, "Event : Listen", "Listen to and wait for an Event Dispatch");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorDebug;; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExEventScope Scope = EPCGExEventScope::Owner;
	
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName Event = NAME_None;
	
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDataEventListenContext final : FPCGExContext
{
	friend class FPCGExDataEventListenElement;
	bool bListening = false;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDataEventListenElement final : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(true)

	//virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
