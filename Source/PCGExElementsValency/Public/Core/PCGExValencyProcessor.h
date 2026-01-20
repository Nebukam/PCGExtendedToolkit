// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClustersProcessor.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyOrbitalCache.h"
#include "Core/PCGExValencyOrbitalSet.h"
#include "Core/PCGExValencyBondingRules.h"

#include "PCGExValencyProcessor.generated.h"

/**
 * Base settings for Valency cluster processors.
 * Provides common OrbitalSet and BondingRules properties with optional validation.
 * Override WantsOrbitalSet() and WantsBondingRules() to control requirements.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency")
class PCGEXELEMENTSVALENCY_API UPCGExValencyProcessorSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(MiscAdd); }
#endif

	/** Whether this node requires an OrbitalSet. Override in derived classes. */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Valency")
	virtual bool WantsOrbitalSet() const { return true; }

	/** Whether this node requires BondingRules. Override in derived classes. */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Valency")
	virtual bool WantsBondingRules() const { return false; }

	/** Orbital set - determines which layer's orbital data to use */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="WantsOrbitalSet()", EditConditionHides))
	TSoftObjectPtr<UPCGExValencyOrbitalSet> OrbitalSet;

	/** The bonding rules data asset */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="WantsBondingRules()", EditConditionHides))
	TSoftObjectPtr<UPCGExValencyBondingRules> BondingRules;

	/** Suppress warnings about missing orbital set */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, EditCondition="WantsOrbitalSet()", EditConditionHides))
	bool bQuietMissingOrbitalSet = false;

	/** Suppress warnings about missing bonding rules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, EditCondition="WantsBondingRules()", EditConditionHides))
	bool bQuietMissingBondingRules = false;
};

/**
 * Base context for Valency cluster processors.
 * Holds loaded OrbitalSet, BondingRules, and orbital direction cache.
 */
struct PCGEXELEMENTSVALENCY_API FPCGExValencyProcessorContext : FPCGExClustersProcessorContext
{
	friend class FPCGExValencyProcessorElement;

	virtual void RegisterAssetDependencies() override;

	/** Loaded orbital set */
	TObjectPtr<UPCGExValencyOrbitalSet> OrbitalSet;

	/** Loaded bonding rules */
	TObjectPtr<UPCGExValencyBondingRules> BondingRules;

	/** Orbital direction cache (for computing orbital indices from directions) */
	PCGExValency::FOrbitalDirectionResolver OrbitalResolver;
};

/**
 * Base element for Valency cluster processors.
 * Handles OrbitalSet loading and validation.
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyProcessorElement : public FPCGExClustersProcessorElement
{
protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
};

namespace PCGExValencyMT
{
	class IBatch;

	/**
	 * Base processor for Valency operations on clusters.
	 * Owns orbital cache (per-cluster) and valency state management.
	 */
	class PCGEXELEMENTSVALENCY_API IProcessor : public PCGExClusterMT::IProcessor
	{
		friend class IBatch;

	protected:
		/** Orbital cache (owned by this processor, built after cluster is available) */
		TSharedPtr<PCGExValency::FOrbitalCache> OrbitalCache;

		/** Valency states (node identity + solver output) */
		TArray<PCGExValency::FValencyState> ValencyStates;

		/** Orbital mask reader (forwarded from batch) */
		TSharedPtr<PCGExData::TBuffer<int64>> OrbitalMaskReader;

		/** Edge indices reader (created in PrepareSingle from edge facade) */
		TSharedPtr<PCGExData::TBuffer<int64>> EdgeIndicesReader;

		/** Max orbitals from orbital set */
		int32 MaxOrbitals = 0;

	public:
		IProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade,
		           const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade);

		virtual ~IProcessor() override = default;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		
		/**
		 * Build the orbital cache from the cluster.
		 * Call this AFTER the cluster is built (in Process() or later).
		 * @return True if cache was built successfully
		 */
		bool BuildOrbitalCache();

		/** Initialize valency states from cache (call after BuildOrbitalCache) */
		void InitializeValencyStates();

		/** Get the orbital cache */
		TSharedPtr<PCGExValency::FOrbitalCache> GetOrbitalCache() const { return OrbitalCache; }

		/** Get valency states array */
		TArray<PCGExValency::FValencyState>& GetValencyStates() { return ValencyStates; }
		const TArray<PCGExValency::FValencyState>& GetValencyStates() const { return ValencyStates; }
	};

	/**
	 * Templated processor with typed context/settings access.
	 */
	template <typename TContext, typename TSettings>
	class TProcessor : public IProcessor
	{
		static_assert(std::is_base_of_v<FPCGExValencyProcessorContext, TContext>,
		              "TContext must inherit from FPCGExValencyProcessorContext");
		static_assert(std::is_base_of_v<UPCGExValencyProcessorSettings, TSettings>,
		              "TSettings must inherit from UPCGExValencyProcessorSettings");

	protected:
		TContext* Context = nullptr;
		const TSettings* Settings = nullptr;

	public:
		TProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade,
		           const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: IProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~TProcessor() override = default;

		virtual void SetExecutionContext(FPCGExContext* InContext) override
		{
			IProcessor::SetExecutionContext(InContext);
			Context = static_cast<TContext*>(ExecutionContext);
			Settings = InContext->GetInputSettings<TSettings>();
		}

		TContext* GetContext() { return Context; }
		const TSettings* GetSettings() { return Settings; }
	};

	/**
	 * Base batch for Valency processors.
	 * Creates attribute readers and forwards them to processors.
	 * Orbital cache is built by processors after cluster is available.
	 */
	class PCGEXELEMENTSVALENCY_API IBatch : public PCGExClusterMT::IBatch
	{
	protected:
		/** Orbital mask reader - vertex attribute (shared across processors) */
		TSharedPtr<PCGExData::TBuffer<int64>> OrbitalMaskReader;

		/** Max orbitals from context's orbital set */
		int32 MaxOrbitals = 0;

	public:
		IBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx,
		       TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);

		virtual ~IBatch() override = default;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
	};

	/**
	 * Templated batch for creating specific processor types.
	 */
	template <typename T>
	class TBatch : public IBatch
	{
	protected:
		virtual TSharedPtr<PCGExClusterMT::IProcessor> NewProcessorInstance(
			const TSharedRef<PCGExData::FFacade>& InVtxDataFacade,
			const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade) const override
		{
			return MakeShared<T>(InVtxDataFacade, InEdgeDataFacade);
		}

	public:
		TBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx,
		       TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: IBatch(InContext, InVtx, InEdges)
		{
		}
	};
}
