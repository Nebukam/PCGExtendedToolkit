// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Core/PCGExProbeFactoryProvider.h"
#include "Core/PCGExProbeOperation.h"
#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExValenceSocketCollection.h"

#include "PCGExProbeValence.generated.h"

UENUM()
enum class EPCGExProbeValencePriorization : uint8
{
	Dot  = 0 UMETA(DisplayName = "Best alignment", ToolTip="Favor the candidates that best align with the direction, as opposed to closest ones."),
	Dist = 1 UMETA(DisplayName = "Closest position", ToolTip="Favor the candidates that are the closest, even if they were not the best aligned."),
};

namespace PCGExProbeValence
{
	class FScopedContainer : public PCGExMT::FScopedContainer
	{
	public:
		FScopedContainer(const PCGExMT::FScope& InScope);

		TArray<double> BestDotsBuffer;
		TArray<double> BestDistsBuffer;
		TArray<int32> BestIdxBuffer;
		TArray<FVector> WorkingDirs;

		void Init(const PCGExValence::FSocketCache& SocketCache, bool bCopyDirs);
		virtual void Reset() override;
	};
}

USTRUCT(BlueprintType)
struct FPCGExProbeConfigValence : public FPCGExProbeConfigBase
{
	GENERATED_BODY()

	FPCGExProbeConfigValence()
		: FPCGExProbeConfigBase()
	{
	}

	/** The socket collection defining sockets and matching parameters */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValenceSocketCollection> SocketCollection;

	/** What matters more? */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExProbeValencePriorization Favor = EPCGExProbeValencePriorization::Dist;
};

/**
 * Probe operation that connects points based on Valence socket directions.
 */
class FPCGExProbeValence : public FPCGExProbeOperation
{
public:
	virtual TSharedPtr<PCGExMT::FScopedContainer> GetScopedContainer(const PCGExMT::FScope& InScope) const override;
	virtual bool RequiresChainProcessing() const override;
	virtual bool Prepare(FPCGExContext* InContext) override;
	virtual void ProcessCandidates(const int32 Index, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container) override;

	FPCGExProbeConfigValence Config;
	PCGExValence::FSocketCache SocketCache;

protected:
	bool bUseBestDot = false;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data", meta=(PCGExNodeLibraryDoc="valence/probe-valence"))
class PCGEXELEMENTSVALENCE_API UPCGExProbeFactoryValence : public UPCGExProbeFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExProbeConfigValence Config;

	PCGExValence::FSocketCache SocketCache;

	virtual TSharedPtr<FPCGExProbeOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class UPCGExProbeValenceProviderSettings : public UPCGExProbeFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(ProbeValence, "Probe : Valence", "Probe using Valence socket collection.", FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExProbeConfigValence Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual void RegisterAssetDependencies(FPCGExContext* InContext) const;
};
