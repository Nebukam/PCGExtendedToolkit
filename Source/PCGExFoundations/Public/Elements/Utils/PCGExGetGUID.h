// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExPointsProcessor.h"
#include "PCGExWriteGUID.h"
#include "Math/PCGExMath.h"
#include "Misc/Guid.h"

#include "PCGExGetGUID.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="metadata/get-guid"))
class UPCGExGetGUIDSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(GetGUID, "Get GUID", "Get a single GUID from a specific point index, same as GetGUID would compute it given the same set of parameters.", Config.OutputAttributeName);
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Constant); }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual bool GetIsMainTransactional() const override;
	//~End UPCGExPointsProcessorSettings

	/** Point Index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	int32 Index = 0;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Ignore;

	/** Config */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExGUIDDetails Config;
};

struct FPCGExGetGUIDContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExGetGUIDElement;
};

class FPCGExGetGUIDElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(GetGUID)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
