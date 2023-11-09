// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExRelationsHelpers.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExRelationsParamsProcessor.generated.h"

class UPCGExRelationsParamsData;

namespace PCGExRelational
{
	extern const FName SourcePointsLabel;
	extern const FName SourceRelationalParamsLabel;
	extern const FName OutputPointsLabel;
}

/**
 * A Base node to process a set of point using RelationalParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExRelationsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("RelationsProcessorSettings")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExRelationsProcessorSettings", "NodeTitle", "Relations Processor Settings"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

private:
	friend class FPCGExRelationsProcessorElement;
	friend class UPCGExRelationsData;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExRelationsProcessorContext : public FPCGContext
{
	friend class FPCGExRelationsProcessorElement;

public:
	PCGExRelational::FParamsInputs Params;
	FPCGExPointIOMap<FPCGExIndexedPointDataIO> Points;

	int32 GetCurrentParamsIndex() const { return CurrentParamsIndex; };
	UPCGExRelationsParamsData* CurrentParams = nullptr;

	bool AdvanceParams(bool bResetPointsIndex = false)
	{
		if (bResetPointsIndex) { CurrentPointsIndex = -1; }
		CurrentParamsIndex++;
		if (Params.Params.IsValidIndex(CurrentParamsIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("AdvanceParams to %d"), CurrentParamsIndex);
			CurrentParams = Params.Params[CurrentParamsIndex];
			return true;
		}

		CurrentParams = nullptr;
		return false;
	}

	int32 GetCurrentPointsIndex() const { return CurrentPointsIndex; };
	FPCGExIndexedPointDataIO* CurrentIO = nullptr;

	bool AdvancePointsIO(bool bResetParamsIndex = false)
	{
		if (bResetParamsIndex) { CurrentParamsIndex = -1; }
		CurrentPointsIndex++;
		if (Points.Pairs.IsValidIndex(CurrentPointsIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("AdvancePointsIO to %d"), CurrentPointsIndex);
			CurrentIO = &(Points.Pairs[CurrentPointsIndex]);
			return true;
		}
		CurrentIO = nullptr;
		return false;
	}

	int32 GetOperation() const { return CurrentOperation; }
	bool IsCurrentOperation(int32 OperationId) const { return CurrentOperation == OperationId; }

	void SetOperation(int32 OperationId)
	{
		int32 PreviousOperation = CurrentOperation;
		CurrentOperation = OperationId;
		UE_LOG(LogTemp, Warning, TEXT("SetOperation = %d (was:%d)"), CurrentOperation, PreviousOperation);
	}

	void Reset()
	{
		CurrentOperation = -1;
		CurrentParamsIndex = -1;
		CurrentParamsIndex = -1;
	}

protected:
	int32 CurrentOperation = -1;
	int32 CurrentParamsIndex = -1;
	int32 CurrentPointsIndex = -1;
	virtual bool InitializePointsOutput() const { return true; }
};

class PCGEXTENDEDTOOLKIT_API FPCGExRelationsProcessorElement : public FPCGPointProcessingElementBase
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	template <typename T>
	T* InitializeRelationsContext(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const
	{
		T* Context = new T();

		Context->InputData = InputData;
		Context->SourceComponent = SourceComponent;
		Context->Node = Node;

		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalParamsLabel);
		Context->Params.Initialize(Context, Sources);

		Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourcePointsLabel);
		Context->Points.Initialize(Context, Sources, Context->InitializePointsOutput());

		return Context;
	}

	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	friend class UPCGExRelationsData;
};
