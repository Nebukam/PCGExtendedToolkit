// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPointsProcessor.h"

#include "PCGExPathProcessor.generated.h"

class UPCGExFilterFactoryBase;

class UPCGExNodeStateFactory;

namespace PCGExCluster
{
	class FNodeStateHandler;
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPath; }
#endif
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputLabel() const override { return PCGExGraph::SourcePathsLabel; }
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputPathsLabel; }
	virtual FString GetPointFilterTooltip() const override { return TEXT("Path points processing filters"); }
	//~End UPCGExPointsProcessorSettings
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPathProcessorElement;

	virtual ~FPCGExPathProcessorContext() override;

	PCGExData::FPointIOCollection* MainPaths = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorElement : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
};
