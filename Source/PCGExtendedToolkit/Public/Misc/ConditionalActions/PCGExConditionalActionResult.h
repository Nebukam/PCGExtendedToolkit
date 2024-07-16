// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExConditionalActionFactoryProvider.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExOperation.h"

#include "PCGExConditionalActionResult.generated.h"

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExConditionalActionResultOperation : public UPCGExConditionalActionOperation
{
	GENERATED_BODY()

public:
	UPCGExConditionalActionResultFactory* TypedFactory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade) override;
	FORCEINLINE virtual void OnMatchSuccess(int32 Index, const FPCGPoint& Point) override { ResultWriter->Values[Index] = true; }
	FORCEINLINE virtual void OnMatchFail(int32 Index, const FPCGPoint& Point) override { ResultWriter->Values[Index] = false; }

	virtual void Cleanup() override;

protected:
	PCGEx::TFAttributeWriter<bool>* ResultWriter = nullptr;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExConditionalActionResultFactory : public UPCGExConditionalActionFactoryBase
{
	friend class UPCGExConditionalActionResultProviderSettings;

	GENERATED_BODY()

public:
	FName ResultAttributeName;

	virtual UPCGExConditionalActionOperation* CreateOperation() const override;
	virtual bool Boot(FPCGContext* InContext) override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|ConditionalActionResult")
class PCGEXTENDEDTOOLKIT_API UPCGExConditionalActionResultProviderSettings : public UPCGExConditionalActionProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConditionalActionWriteAttributes, "Action : Write Result", "Simply writes the filter result to an attribute.")
#endif
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ResultAttributeName = "Pass";

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
