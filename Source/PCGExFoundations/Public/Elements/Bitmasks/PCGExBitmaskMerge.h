// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Bitmasks/PCGExBitmaskDetails.h"
#include "Core/PCGExContext.h"
#include "Core/PCGExElement.h"
#include "Core/PCGExSettings.h"
#include "PCGExCoreMacros.h"
#include "PCGExCoreSettingsCache.h"
#include "PCGExBitmaskMerge.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="metadata/bitmasks/bitmask-merge"))
class UPCGExBitmaskMergeSettings : public UPCGExSettings
{
	GENERATED_BODY()

	friend class FPCGExBitmaskMergeElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BitmaskMerge, "Bitmask Merge", "A Simple bitmask merge node.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(MiscWrite); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExBitOp Operation = EPCGExBitOp::OR;
};

class FPCGExBitmaskMergeElement final : public IPCGExElement
{
public:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT

protected:
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
