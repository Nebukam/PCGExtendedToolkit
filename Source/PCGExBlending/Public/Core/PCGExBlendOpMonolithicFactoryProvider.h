// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBlendOpFactory.h"
#include "UObject/Object.h"

#include "Details/PCGExBlendingDetails.h"
#include "Factories/PCGExFactoryProvider.h"

#include "PCGExBlendOpMonolithicFactoryProvider.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending")
class PCGEXBLENDING_API UPCGExBlendOpMonolithicFactory : public UPCGExBlendOpFactory
{
	GENERATED_BODY()

public:
	FPCGExBlendingDetails BlendingDetails;

	virtual bool IsMonolithic() const override { return true; }

	virtual bool CreateOperations(
		FPCGExContext* InContext,
		const TSharedPtr<PCGExData::FFacade>& InSourceAFacade,
		const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
		TArray<TSharedPtr<FPCGExBlendOperation>>& OutOperations,
		const TSet<FName>* InSupersedeNames = nullptr) const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual void RegisterBuffersDependenciesForSourceA(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual void RegisterBuffersDependenciesForSourceB(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending", meta=(PCGExNodeLibraryDoc="metadata/modify/blendop-monolithic"))
class PCGEXBLENDING_API UPCGExBlendOpMonolithicProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(BlendOpMonolithic, "BlendOp : Monolithic", "Creates bulk blend operations from monolithic blending settings.", PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(BlendOp); }

	virtual bool CanUserEditTitle() const override { return false; }
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
#endif

	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoBlendOp)
	//~End UPCGSettings

public:
	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

	virtual FName GetMainOutputPin() const override { return PCGExBlending::Labels::OutputBlendingLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Priority.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority = 0;

	/** Blending details. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExBlendingDetails BlendingDetails;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
