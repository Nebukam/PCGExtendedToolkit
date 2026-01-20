// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyProcessor.h"

#include "Clusters/PCGExCluster.h"
#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Helpers/PCGExStreamingHelpers.h"

void FPCGExValencyProcessorContext::RegisterAssetDependencies()
{
	FPCGExClustersProcessorContext::RegisterAssetDependencies();

	const UPCGExValencyProcessorSettings* Settings = GetInputSettings<UPCGExValencyProcessorSettings>();
	if (!Settings) { return; }

	// Register OrbitalSet if wanted and provided
	if (Settings->WantsOrbitalSet() && !Settings->OrbitalSet.IsNull())
	{
		AddAssetDependency(Settings->OrbitalSet.ToSoftObjectPath());
	}

	// Register BondingRules if wanted and provided
	if (Settings->WantsBondingRules() && !Settings->BondingRules.IsNull())
	{
		AddAssetDependency(Settings->BondingRules.ToSoftObjectPath());
	}
}

bool FPCGExValencyProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyProcessor)

	// Validate OrbitalSet if wanted
	if (Settings->WantsOrbitalSet() && Settings->OrbitalSet.IsNull())
	{
		// OrbitalSet might come from BondingRules, so only fail if we don't also want BondingRules
		if (!Settings->WantsBondingRules())
		{
			if (!Settings->bQuietMissingOrbitalSet)
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("No Valency Orbital Set provided."));
			}
			return false;
		}
	}

	// Validate BondingRules if wanted
	if (Settings->WantsBondingRules() && Settings->BondingRules.IsNull())
	{
		if (!Settings->bQuietMissingBondingRules)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("No Bonding Rules provided."));
		}
		return false;
	}

	// Start loading assets
	if (!Settings->OrbitalSet.IsNull())
	{
		PCGExHelpers::LoadBlocking_AnyThreadTpl(Settings->OrbitalSet, InContext);
	}
	if (!Settings->BondingRules.IsNull())
	{
		PCGExHelpers::LoadBlocking_AnyThreadTpl(Settings->BondingRules, InContext);
	}

	return true;
}

void FPCGExValencyProcessorElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExClustersProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(ValencyProcessor)

	// Load BondingRules first (OrbitalSet may come from it)
	if (!Settings->BondingRules.IsNull())
	{
		Context->BondingRules = Settings->BondingRules.Get();
	}

	// Load OrbitalSet - prefer explicit setting, fall back to BondingRules
	if (!Settings->OrbitalSet.IsNull())
	{
		Context->OrbitalSet = Settings->OrbitalSet.Get();
	}
	else if (Context->BondingRules && Context->BondingRules->OrbitalSets.Num() > 0)
	{
		// Auto-populate from BondingRules if OrbitalSet not explicitly set
		Context->OrbitalSet = Context->BondingRules->OrbitalSets[0];
	}
}

bool FPCGExValencyProcessorElement::PostBoot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::PostBoot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyProcessor)

	// Validate BondingRules if wanted
	if (Settings->WantsBondingRules())
	{
		if (!Context->BondingRules)
		{
			if (!Settings->bQuietMissingBondingRules)
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Bonding Rules."));
			}
			return false;
		}
	}

	// Validate OrbitalSet if wanted
	if (Settings->WantsOrbitalSet())
	{
		if (!Context->OrbitalSet)
		{
			if (Context->BondingRules && Context->BondingRules->OrbitalSets.Num() == 0)
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Bonding Rules has no OrbitalSets. Rebuild the Bonding Rules asset."));
			}
			else if (!Settings->bQuietMissingOrbitalSet)
			{
				PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to load Valency Orbital Set."));
			}
			return false;
		}

		// Validate orbital set
		TArray<FText> ValidationErrors;
		if (!Context->OrbitalSet->Validate(ValidationErrors))
		{
			for (const FText& Error : ValidationErrors)
			{
				PCGE_LOG(Error, GraphAndLog, Error);
			}
			return false;
		}

		// Build orbital direction cache
		if (!Context->OrbitalResolver.BuildFrom(Context->OrbitalSet))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to build orbital cache from orbital set."));
			return false;
		}
	}

	return true;
}

namespace PCGExValencyMT
{
	//////// IProcessor

	IProcessor::IProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade,
	                       const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
		: PCGExClusterMT::IProcessor(InVtxDataFacade, InEdgeDataFacade)
	{
	}

	bool IProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		if (!PCGExClusterMT::IProcessor::Process(InTaskManager)) { return false; }

		// Get context for orbital set access
		FPCGExValencyProcessorContext* Context = static_cast<FPCGExValencyProcessorContext*>(ExecutionContext);
		if (!Context->OrbitalSet) { return false; }

		FilterVtxScope(PCGExMT::FScope(0, NumNodes));
		
		// Get edge indices reader for this processor's edge facade
		const FName IdxAttributeName = Context->OrbitalSet->GetOrbitalIdxAttributeName();
		EdgeIndicesReader = EdgeDataFacade->GetReadable<int64>(IdxAttributeName);

		if (!EdgeIndicesReader)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context,
			           FText::Format(FTEXT("Edge indices attribute '{0}' not found. Run 'Write Valency Orbitals' first."),
				           FText::FromName(IdxAttributeName)));
			return false;
		}

		// Build orbital cache now that cluster is available
		// (Readers were forwarded from batch in PrepareSingle)
		if (!BuildOrbitalCache())
		{
			// Specific error already logged by BuildOrbitalCache
			return false;
		}

		// Initialize valency states from cache
		InitializeValencyStates();

		return true;
	}
	

	bool IProcessor::BuildOrbitalCache()
	{
		// Check each requirement and log specific failure reason
		if (!Cluster)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("BuildOrbitalCache: Cluster is null."));
			return false;
		}
		if (!OrbitalMaskReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("BuildOrbitalCache: OrbitalMaskReader is null. Check that orbital mask attribute exists on vertices."));
			return false;
		}
		if (!EdgeIndicesReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("BuildOrbitalCache: EdgeIndicesReader is null. Check that orbital indices attribute exists on edges."));
			return false;
		}
		if (MaxOrbitals <= 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FText::Format(FTEXT("BuildOrbitalCache: MaxOrbitals is {0}, must be > 0. Check OrbitalSet configuration."), FText::AsNumber(MaxOrbitals)));
			return false;
		}

		OrbitalCache = MakeShared<PCGExValency::FOrbitalCache>();

		if (!OrbitalCache->BuildFrom(Cluster, OrbitalMaskReader, EdgeIndicesReader, MaxOrbitals))
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("BuildOrbitalCache: FOrbitalCache::BuildFrom failed."));
			OrbitalCache.Reset();
			return false;
		}

		return true;
	}

	void IProcessor::InitializeValencyStates()
	{
		if (OrbitalCache && OrbitalCache->IsValid())
		{
			OrbitalCache->InitializeStates(ValencyStates);
		}
	}

	//////// IBatch

	IBatch::IBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx,
	               TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: PCGExClusterMT::IBatch(InContext, InVtx, InEdges)
	{
		DefaultVtxFilterValue = true;
	}

	void IBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		PCGExClusterMT::IBatch::RegisterBuffersDependencies(FacadePreloader);

		// Register orbital mask attribute for preloading
		FPCGExValencyProcessorContext* Context = GetContext<FPCGExValencyProcessorContext>();
		if (Context && Context->OrbitalSet)
		{
			FacadePreloader.Register<int64>(ExecutionContext, Context->OrbitalSet->GetOrbitalMaskAttributeName());
		}
	}

	void IBatch::OnProcessingPreparationComplete()
	{
		// Create readers BEFORE calling parent (parent may trigger PrepareSingle)
		FPCGExValencyProcessorContext* Context = GetContext<FPCGExValencyProcessorContext>();

		if (Context && Context->OrbitalSet)
		{
			MaxOrbitals = Context->OrbitalSet->Num();

			// Create orbital mask reader from vertex facade
			const FName MaskAttributeName = Context->OrbitalSet->GetOrbitalMaskAttributeName();
			OrbitalMaskReader = VtxDataFacade->GetReadable<int64>(MaskAttributeName);

			if (!OrbitalMaskReader)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
				           FText::Format(FTEXT("Orbital mask attribute '{0}' not found on vertices. Run 'Write Valency Orbitals' first."),
					           FText::FromName(MaskAttributeName)));
			}
		}
		else if (Context && !Context->OrbitalSet)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("OrbitalSet is null. Ensure BondingRules or OrbitalSet is configured."));
		}

		PCGExClusterMT::IBatch::OnProcessingPreparationComplete();
	}

	bool IBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!PCGExClusterMT::IBatch::PrepareSingle(InProcessor)) { return false; }

		IProcessor* ValencyProcessor = static_cast<IProcessor*>(InProcessor.Get());
		if (!ValencyProcessor) { return false; }

		// Forward readers and config to processor - cache will be built after cluster is available
		ValencyProcessor->OrbitalMaskReader = OrbitalMaskReader;
		ValencyProcessor->MaxOrbitals = MaxOrbitals;

		return true;
	}
}
