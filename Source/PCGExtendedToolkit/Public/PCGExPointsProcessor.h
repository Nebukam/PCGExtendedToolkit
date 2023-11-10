// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExPointsProcessor.generated.h"

namespace PCGEx
{
	extern const FName SourcePointsLabel;
	extern const FName OutputPointsLabel;

	enum EOperation : int
	{
		Setup              = -1 UMETA(DisplayName = "Setup"),
		ReadyForNextPoints = 0 UMETA(DisplayName = "Ready for next points"),
		ReadyForNextParams = 1 UMETA(DisplayName = "Ready for next params"),
		ProcessingPoints   = 2 UMETA(DisplayName = "Processing points"),
		ProcessingParams   = 3 UMETA(DisplayName = "Processing params"),
		Done               = 77 UMETA(DisplayName = "Done")
	};
}

/**
 * A Base node to process a set of point using RelationalParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPointsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGExPointsProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PointsProcessorSettings")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExPointsProcessorSettings", "NodeTitle", "Points Processor Settings"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual PCGEx::EIOInit GetPointOutputInitMode() const;

	/** Multithread chunk size, when supported.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (AdvancedDisplay))
	int32 ChunkSize = 0;

protected:
	virtual int32 GetPreferredChunkSize() const;

private:
	friend class UPCGExRelationsData;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorContext : public FPCGContext
{
	friend class FPCGExPointsProcessorElementBase;

public:
	PCGEx::FPointIOGroup Points;

	int32 GetCurrentPointsIndex() const { return CurrentPointsIndex; };
	PCGEx::FPointIO* CurrentIO = nullptr;

	bool AdvancePointsIO();

	int32 GetOperation() const { return CurrentOperation; }
	bool IsCurrentOperation(int32 OperationId) const { return CurrentOperation == OperationId; }

	virtual void SetOperation(int32 OperationId);
	virtual void Reset();
	virtual bool IsValid();

	int32 ChunkSize = 0;

protected:
	int32 CurrentOperation = -1;
	int32 CurrentPointsIndex = -1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorElementBase : public FPCGPointProcessingElementBase
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual void InitializeContext(FPCGExPointsProcessorContext* InContext, const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const;
	//virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	friend class UPCGExRelationsData;
};
