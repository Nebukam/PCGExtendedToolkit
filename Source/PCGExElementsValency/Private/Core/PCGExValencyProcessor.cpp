// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyProcessor.h"

#include "Clusters/PCGExCluster.h"
#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"

void FPCGExValencyProcessorContext::RegisterAssetDependencies()
{
	FPCGExClustersProcessorContext::RegisterAssetDependencies();

	const UPCGExValencyProcessorSettings* Settings = GetInputSettings<UPCGExValencyProcessorSettings>();
	if (Settings && !Settings->OrbitalSet.IsNull())
	{
		AddAssetDependency(Settings->OrbitalSet.ToSoftObjectPath());
	}
}

bool FPCGExValencyProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyProcessor)

	if (Settings->OrbitalSet.IsNull())
	{
		if (!Settings->bQuietMissingOrbitalSet)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("No Valency Orbital Set provided."));
		}
		return false;
	}

	return true;
}

void FPCGExValencyProcessorElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExClustersProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(ValencyProcessor)

	if (!Settings->OrbitalSet.IsNull())
	{
		Context->OrbitalSet = Settings->OrbitalSet.Get();
	}
}

bool FPCGExValencyProcessorElement::PostBoot(FPCGExContext* InContext) const
{
	if (!FPCGExClustersProcessorElement::PostBoot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyProcessor)

	if (!Context->OrbitalSet)
	{
		if (!Settings->bQuietMissingOrbitalSet)
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

	// Settings->OrbitalSet->EDITOR_RegisterTrackingKeys(Context);

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
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Failed to build orbital cache. Run 'Write Valency Orbitals' first."));
			return false;
		}

		// Initialize valency states from cache
		InitializeValencyStates();

		return true;
	}

	bool IProcessor::BuildOrbitalCache()
	{
		if (!Cluster || !OrbitalMaskReader || !EdgeIndicesReader || MaxOrbitals <= 0)
		{
			return false;
		}

		OrbitalCache = MakeShared<PCGExValency::FOrbitalCache>();

		if (!OrbitalCache->BuildFrom(Cluster, OrbitalMaskReader, EdgeIndicesReader, MaxOrbitals))
		{
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
				           FText::Format(FTEXT("Orbital mask attribute '{0}' not found. Run 'Write Valency Orbitals' first."),
					           FText::FromName(MaskAttributeName)));
			}
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
