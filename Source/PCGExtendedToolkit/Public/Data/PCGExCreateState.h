// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"

#include "PCGExCreateState.generated.h"

/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateStateSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorFilter; }

#endif
	virtual FName GetMainOutputLabel() const;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	//~End UPCGSettings

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	/** State name.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName StateName = NAME_Default;

	/** State ID.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 StateId = 0;

	/** State priority for conflict resolution. Higher values are applied last as to override lower-priority states.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 Priority = 0;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCreateStateElement : public IPCGElement
{
public:
#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

protected:
	virtual bool Boot(FPCGContext* Context) const;

	template <typename T>
	T* CreateStateDefinition(const FPCGContext* Context) const
	{
		PCGEX_SETTINGS(CreateState)

		T* OutState = NewObject<T>();

		OutState->StateName = Settings->StateName;
		OutState->StateId = Settings->StateId;
		OutState->Priority = Settings->Priority;

		const TArray<FPCGTaggedData>& IfPin = Context->InputData.GetInputsByPin(PCGExDataState::SourceValidStateAttributesLabel);
		for (const FPCGTaggedData& TaggedData : IfPin)
		{
			if (const UPCGParamData* ValidStateData = Cast<UPCGParamData>(TaggedData.Data))
			{
				OutState->ValidStateAttributes.Add(const_cast<UPCGParamData*>(ValidStateData));
				OutState->ValidStateAttributesInfos.Add(PCGEx::FAttributesInfos::Get(ValidStateData->Metadata));
			}
		}

		const TArray<FPCGTaggedData>& ElsePin = Context->InputData.GetInputsByPin(PCGExDataState::SourceInvalidStateAttributesLabel);
		for (const FPCGTaggedData& TaggedData : ElsePin)
		{
			if (const UPCGParamData* InvalidStateData = Cast<UPCGParamData>(TaggedData.Data))
			{
				OutState->InvalidStateAttributes.Add(const_cast<UPCGParamData*>(InvalidStateData));
				OutState->InvalidStateAttributesInfos.Add(PCGEx::FAttributesInfos::Get(InvalidStateData->Metadata));
			}
		}

		return OutState;
	}

	template <typename T>
	static void OutputState(FPCGContext* Context, T* State)
	{
		PCGEX_SETTINGS(CreateState)

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = State;
		Output.Pin = Settings->GetMainOutputLabel();
	}

public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
};
