// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "PCGSettings.h"
#include "PCGExPointIO.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExPointsProcessor.generated.h"

namespace PCGExMT
{
	enum EState : int
	{
		Setup UMETA(DisplayName = "Setup"),
		ReadyForNextPoints UMETA(DisplayName = "Ready for next points"),
		ReadyForPoints2ndPass UMETA(DisplayName = "Ready for next points - 2nd pass"),
		ReadyForNextParams UMETA(DisplayName = "Ready for next params"),
		ReadyForParams2ndPass UMETA(DisplayName = "Ready for next params - 2nd pass"),
		ProcessingPoints UMETA(DisplayName = "Processing points"),
		ProcessingPoints2ndPass UMETA(DisplayName = "Processing points - 2nd pass"),
		ProcessingParams UMETA(DisplayName = "Processing params"),
		ProcessingParams2ndPass UMETA(DisplayName = "Processing params - 2nd pass"),
		Done UMETA(DisplayName = "Done")
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
	PCGEX_NODE_INFOS(PointsProcessorSettings, "Points Processor Settings", "TOOLTIP_TEXT");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual PCGEx::EIOInit GetPointOutputInitMode() const;

	/** Multithread chunk size, when supported.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay, meta=(Units="Seconds"))
	double DebugDrawLifetime = 10.0f;

	/** Multithread chunk size, when supported.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	int32 ChunkSize = 0;

protected:
	virtual int32 GetPreferredChunkSize() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorContext : public FPCGContext
{
	friend class FPCGExPointsProcessorElementBase;

public:
	UPCGExPointIOGroup* Points = nullptr;

	int32 GetCurrentPointsIndex() const { return CurrentPointsIndex; };
	UPCGExPointIO* CurrentIO = nullptr;

	bool AdvancePointsIO();

	PCGExMT::EState GetState() const { return CurrentState; }
	bool IsState(const PCGExMT::EState OperationId) const { return CurrentState == OperationId; }
	bool IsSetup() const { return IsState(PCGExMT::EState::Setup); }
	bool IsDone() const { return IsState(PCGExMT::EState::Done); }

	virtual void SetState(PCGExMT::EState OperationId);
	virtual void Reset();

	virtual bool ValidatePointDataInput(UPCGPointData* PointData);
	virtual void PostInitPointDataInput(UPCGExPointIO* PointData);

	int32 ChunkSize = 0;

	mutable FRWLock ContextLock;

	void OutputPoints() { Points->OutputTo(this); }

protected:
	PCGExMT::EState CurrentState = PCGExMT::EState::Setup;
	int32 CurrentPointsIndex = -1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorElementBase : public FPCGPointProcessingElementBase
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool Validate(FPCGContext* InContext) const;
	virtual void InitializeContext(FPCGExPointsProcessorContext* InContext, const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const;
	//virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
