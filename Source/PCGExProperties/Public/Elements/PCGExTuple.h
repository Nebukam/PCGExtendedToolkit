// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCoreMacros.h"
#include "PCGExCoreSettingsCache.h"

#include "PCGSettings.h"
#include "Core/PCGExContext.h"
#include "Core/PCGExElement.h"
#include "Core/PCGExSettings.h"
#include "PCGExPropertyCompiled.h"

#include "PCGExTuple.generated.h"

class FPCGMetadataAttributeBase;

// Tuple now uses the unified property system (FPCGExPropertySchema)
// All value types (Boolean, Float, String, etc.) use FPCGExPropertyCompiled_* from PCGExPropertyCompiled.h

#pragma region Tuple Header and Body

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="quality-of-life/tuple"))
class UPCGExTupleSettings : public UPCGExSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Tuple, "Tuple", "A Simple Tuple attribute.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Constant); }

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Tuple composition - defines the columns (property types and names) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ToolTip="Tuple composition defines the columns (property types and names)."))
	FPCGExPropertySchemaCollection Composition;

	/** Tuple values - each row uses PropertyOverrides to enable/disable columns */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(ToolTip="Tuple values. Toggle 'Enabled' per column to include/exclude values. Rows auto-sync with composition changes.", FullyExpand=true))
	TArray<FPCGExPropertyOverrides> Values;

	/** A list of tags separated by a comma, for easy overrides. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString CommaSeparatedTags;
};

class FPCGExTupleElement final : public IPCGExElement
{
protected:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
