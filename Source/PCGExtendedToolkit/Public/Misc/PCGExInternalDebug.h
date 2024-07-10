// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExInternalDebug.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Debug")
class PCGEXTENDEDTOOLKIT_API UPCGExInternalDebugSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(InternalDebug, "Internal Debug", "Ignore.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector GHTolerance = FVector(0.001, 0.001, 0.001);
};

struct PCGEXTENDEDTOOLKIT_API FPCGExInternalDebugContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExInternalDebugElement;

	virtual ~FPCGExInternalDebugContext() override;

	FVector GHTolerance = FVector::One();
};

class PCGEXTENDEDTOOLKIT_API FPCGExInternalDebugElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
