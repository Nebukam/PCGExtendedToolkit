// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExConditionalActionFactoryProvider.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExOperation.h"


#include "PCGExConditionalActionResult.generated.h"

class UPCGExConditionalActionResultFactory;

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionResultOperation : public UPCGExConditionalActionOperation
{
	GENERATED_BODY()

public:
	UPCGExConditionalActionResultFactory* TypedFactory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
	FORCEINLINE virtual void OnMatchSuccess(const int32 Index, const FPCGPoint& Point) override { ResultWriter->GetMutable(Index) = true; }
	FORCEINLINE virtual void OnMatchFail(const int32 Index, const FPCGPoint& Point) override { ResultWriter->GetMutable(Index) = false; }

	virtual void Cleanup() override;

protected:
	TSharedPtr<PCGExData::TBuffer<bool>> ResultWriter;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionResultFactory : public UPCGExConditionalActionFactoryBase
{
	friend class UPCGExConditionalActionResultProviderSettings;

	GENERATED_BODY()

public:
	FName ResultAttributeName;

	virtual UPCGExConditionalActionOperation* CreateOperation(FPCGExContext* InContext) const override;
	virtual bool Boot(FPCGContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|ConditionalActionResult")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionResultProviderSettings : public UPCGExConditionalActionProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConditionalActionWriteResult, "Action : Write Result", "Simply writes the filter result to an attribute.")
#endif
	//~End UPCGSettings

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName ResultAttributeName = "Pass";

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
