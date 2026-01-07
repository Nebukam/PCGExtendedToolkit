// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExContext.h"
#include "Core/PCGExElement.h"

#include "Core/PCGExSettings.h"
#include "PCGExCoreMacros.h"

#include "PCGExFlushDebug.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGExDebugSettings : public UPCGExSettings
{
	GENERATED_BODY()

	friend class FPCGExDebugElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FlushDebug, "Flush Debug", "Flush persistent debug lines.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return CustomColor; }
#endif

	virtual bool HasDynamicPins() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** Debug drawing toggle. Exposed to have more control on debug draw in sub-graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug, meta=(PCG_Overridable))
	bool bPCGExDebug = true;

	/** Debug drawing toggle. Exposed to have more control on debug draw in sub-graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Debug, meta=(PCG_Overridable))
	FLinearColor CustomColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
};

class FPCGExDebugElement final : public IPCGExElement
{
protected:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT

	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
};
