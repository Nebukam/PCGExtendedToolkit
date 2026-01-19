// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Core/PCGExProbeFactoryProvider.h"
#include "Core/PCGExProbeOperation.h"
#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExValencyOrbitalSet.h"

#include "PCGExProbeValency.generated.h"

UENUM()
enum class EPCGExProbeValencyPriorization : uint8
{
	Dot  = 0 UMETA(DisplayName = "Best alignment", ToolTip="Favor the candidates that best align with the direction, as opposed to closest ones."),
	Dist = 1 UMETA(DisplayName = "Closest position", ToolTip="Favor the candidates that are the closest, even if they were not the best aligned."),
};

namespace PCGExProbeValency
{
	class FScopedContainer : public PCGExMT::FScopedContainer
	{
	public:
		FScopedContainer(const PCGExMT::FScope& InScope);

		TArray<double> BestDotsBuffer;
		TArray<double> BestDistsBuffer;
		TArray<int32> BestIdxBuffer;
		TArray<FVector> WorkingDirs;

		void Init(const PCGExValency::FOrbitalDirectionResolver& OrbitalResolver, bool bCopyDirs);
		virtual void Reset() override;
	};
}

USTRUCT(BlueprintType)
struct FPCGExProbeConfigValency : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigValency()
		: FPCGExProbeConfigBase()
	{
	}

	/** The orbital set defining orbitals and matching parameters */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValencyOrbitalSet> OrbitalSet;

	/** What matters more? */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExProbeValencyPriorization Favor = EPCGExProbeValencyPriorization::Dist;
};

/**
 * Probe operation that connects points based on Valency orbital directions.
 */
class FPCGExProbeValency : public FPCGExProbeOperation
{
public:
	virtual TSharedPtr<PCGExMT::FScopedContainer> GetScopedContainer(const PCGExMT::FScope& InScope) const override;
	virtual bool RequiresChainProcessing() const override;
	virtual bool Prepare(FPCGExContext* InContext) override;
	virtual void ProcessCandidates(const int32 Index, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container) override;

	FPCGExProbeConfigValency Config;
	PCGExValency::FOrbitalDirectionResolver OrbitalResolver;

protected:
	bool bUseBestDot = false;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data", meta=(PCGExNodeLibraryDoc="valency/probe-valency"))
class PCGEXELEMENTSVALENCY_API UPCGExProbeFactoryValency : public UPCGExProbeFactoryData
{
	GENERATED_BODY()

public:
	TSharedPtr<FStreamableHandle> OrbitalSetHandle;
	
	UPROPERTY()
	FPCGExProbeConfigValency Config;

	PCGExValency::FOrbitalDirectionResolver OrbitalResolver;

	virtual TSharedPtr<FPCGExProbeOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class UPCGExProbeValencyProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ProbeValency, "Probe : Orbital (Valency)", "Probe using Valency orbital set.", FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigValency Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const;
};
