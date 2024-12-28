// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGEx.h"
#include "PCGExCompare.h"

#include "PCGExIterations.generated.h"

UENUM()
enum class EPCGExIterationDataType : uint8
{
	Any = 0 UMETA(DisplayName = "Any", Tooltip="Output dummy iteration data of type Attribute set, using an untyped pin."),
	Params = 1 UMETA(DisplayName = "Attribute Set", Tooltip="Output dummy iteration data of type Attribute set."),
	Points = 2 UMETA(DisplayName = "Points", Tooltip="Output dummy iteration data of type Points."),
	Spline = 3 UMETA(DisplayName = "Spline", Tooltip="Output dummy iteration data of type Spline."),
	Texture = 4 UMETA(DisplayName = "Texture", Tooltip="Output dummy iteration data of type Texture."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExIterationsSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend class FPCGExIterationsElement;

public:
	bool bCacheResult = false;
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Iterations, "Iterations", "A Simple Iterations data generator. It create a single instance of a lightweight dummy data object and adds duplicate entries to the node output to be used as individual iterations for a loop node.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorConstant; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	/** Type of data to generate. This is useful if you build subgraphs that are meant to be used as both loops and regular subgraphs, so you can have properly typed pins. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	EPCGExIterationDataType Type = EPCGExIterationDataType::Any;
	
	/** Number of dataset to generate */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	int32 Iterations = 0;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExIterationsElement final : public IPCGElement
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
