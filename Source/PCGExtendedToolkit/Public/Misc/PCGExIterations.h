﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCompare.h"
#include "PCGExPointsProcessor.h"
#include "PCGSettings.h"

#include "PCGExIterations.generated.h"

UENUM()
enum class EPCGExIterationDataType : uint8
{
	Any     = 0 UMETA(DisplayName = "Any", Tooltip="Output dummy iteration data of type Attribute set, using an untyped pin."),
	Params  = 1 UMETA(DisplayName = "Attribute Set", Tooltip="Output dummy iteration data of type Attribute set."),
	Points  = 2 UMETA(DisplayName = "Points", Tooltip="Output dummy iteration data of type Points."),
	Spline  = 3 UMETA(DisplayName = "Spline", Tooltip="Output dummy iteration data of type Spline."),
	Texture = 4 UMETA(DisplayName = "Texture", Tooltip="Output dummy iteration data of type Texture."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), meta=(PCGExNodeLibraryDoc="quality-of-life/iterations"))
class UPCGExIterationsSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExIterationsElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_DUMMY_SETTINGS_MEMBERS
	PCGEX_NODE_INFOS(Iterations, "Iterations", "A Simple Iterations data generator. It create a single instance of a lightweight dummy data object and adds duplicate entries to the node output to be used as individual iterations for a loop node.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorConstant; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** Type of data to generate. This is useful if you build subgraphs that are meant to be used as both loops and regular subgraphs, so you can have properly typed pins. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	EPCGExIterationDataType Type = EPCGExIterationDataType::Params;

	/** Number of dataset to generate */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	int32 Iterations = 0;

	/** Output per-iteration params with useful values. Less optimized than the non-value version */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Type = EPCGExIterationDataType::Params"))
	bool bOutputUtils = false;
};

class FPCGExIterationsElement final : public IPCGElement
{
public:
	PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }
};
