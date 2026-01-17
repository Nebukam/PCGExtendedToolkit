// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Probes/PCGExProbeValency.h"

#include "PCGExH.h"
#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExProbingCandidates.h"
#include "Helpers/PCGExStreamingHelpers.h"

PCGEX_CREATE_PROBE_FACTORY(
	Valency,
	{
		// Load and cache orbital set
		if (!Config.OrbitalSet.IsNull())
		{
			NewFactory->OrbitalSetHandle = PCGExHelpers::LoadBlocking_AnyThreadTpl(Config.OrbitalSet, InContext);
			if (UPCGExValencyOrbitalSet* OrbitalSet = Config.OrbitalSet.Get())
			{
				if (!NewFactory->OrbitalCache.BuildFrom(OrbitalSet))
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Failed to build orbital cache from Valency Orbital Set."));
					return nullptr;
				}
			}
			else
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Failed to load Valency Orbital Set."));
				return nullptr;
			}
		}
		else
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No Valency Orbital Set provided."));
			return nullptr;
		}
	},
	{
		NewOperation->OrbitalCache = OrbitalCache;
	})

void UPCGExProbeValencyProviderSettings::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	if (!Config.OrbitalSet.IsNull())
	{
		InContext->AddAssetDependency(Config.OrbitalSet.ToSoftObjectPath());
	}
}

namespace PCGExProbeValency
{
	FScopedContainer::FScopedContainer(const PCGExMT::FScope& InScope)
		: PCGExMT::FScopedContainer(InScope)
	{
	}

	void FScopedContainer::Init(const PCGExValency::FOrbitalCache& OrbitalCache, const bool bCopyDirs)
	{
		const int32 NumDirs = OrbitalCache.Num();
		BestDotsBuffer.SetNumUninitialized(NumDirs);
		BestDistsBuffer.SetNumUninitialized(NumDirs);
		BestIdxBuffer.SetNumUninitialized(NumDirs);

		if (bCopyDirs) { WorkingDirs = OrbitalCache.Directions; }
		else { WorkingDirs.SetNumUninitialized(NumDirs); }
	}

	void FScopedContainer::Reset()
	{
		for (int i = 0; i < BestDotsBuffer.Num(); i++)
		{
			BestDotsBuffer[i] = -1;
			BestDistsBuffer[i] = MAX_dbl;
			BestIdxBuffer[i] = -1;
		}
	}
}

TSharedPtr<PCGExMT::FScopedContainer> FPCGExProbeValency::GetScopedContainer(const PCGExMT::FScope& InScope) const
{
	TSharedPtr<PCGExProbeValency::FScopedContainer> ScopedContainer = MakeShared<PCGExProbeValency::FScopedContainer>(InScope);
	ScopedContainer->Init(OrbitalCache, OrbitalCache.bTransformOrbital);
	return ScopedContainer;
}

bool FPCGExProbeValency::RequiresChainProcessing() const { return false; }

bool FPCGExProbeValency::Prepare(FPCGExContext* InContext)
{
	if (!FPCGExProbeOperation::Prepare(InContext)) { return false; }

	bUseBestDot = Config.Favor == EPCGExProbeValencyPriorization::Dot;

	return true;
}

void FPCGExProbeValency::ProcessCandidates(const int32 Index, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
	const int32 DirCount = OrbitalCache.Num();
	if (DirCount == 0) { return; }

	PCGExProbeValency::FScopedContainer* LocalContainer = static_cast<PCGExProbeValency::FScopedContainer*>(Container);
	LocalContainer->Reset();

	TArray<double>& BestDotsBuffer = LocalContainer->BestDotsBuffer;
	TArray<double>& BestDistsBuffer = LocalContainer->BestDistsBuffer;
	TArray<int32>& BestIdxBuffer = LocalContainer->BestIdxBuffer;
	TArray<FVector>& WorkingDirs = LocalContainer->WorkingDirs;

	const double DotThreshold = OrbitalCache.DotThreshold;

	// Precompute world dirs if using transform
	if (OrbitalCache.bTransformOrbital)
	{
		const FTransform& WorkingTransform = *(WorkingTransforms->GetData() + Index);
		for (int32 d = 0; d < DirCount; ++d)
		{
			WorkingDirs[d] = WorkingTransform.TransformVectorNoScale(OrbitalCache.Directions[d]);
		}
	}

	const double R = GetSearchRadius(Index);
	const int32 MaxIndex = Candidates.Num() - 1;
	PCGExProbing::FCandidate* CandData = Candidates.GetData();

	for (int i = 0; i <= MaxIndex; ++i)
	{
		const int32 LocalIndex = bUseBestDot ? (MaxIndex - i) : i;
		const PCGExProbing::FCandidate& C = CandData[LocalIndex];

		if (bUseBestDot)
		{
			if (C.Distance > R) { continue; }
		}
		else
		{
			if (C.Distance > R) { break; }
		}

		if (Coincidence && Coincidence->Contains(C.GH)) { continue; }

		const double CandDist = C.Distance;
		const FVector& CandDir = C.Direction;

		// Inner loop - check each orbital direction
		for (int32 d = 0; d < DirCount; ++d)
		{
			const double dot = FVector::DotProduct(WorkingDirs[d], CandDir);
			if (dot < DotThreshold) { continue; }

			if (dot >= BestDotsBuffer[d] && CandDist < BestDistsBuffer[d])
			{
				BestDotsBuffer[d] = dot;
				BestDistsBuffer[d] = CandDist;
				BestIdxBuffer[d] = LocalIndex;
			}
		}
	}

	for (int32 d = 0; d < DirCount; ++d)
	{
		const int32 CI = BestIdxBuffer[d];
		if (CI < 0) { continue; }

		const PCGExProbing::FCandidate& C = CandData[CI];

		if (Coincidence)
		{
			bool bAlready;
			Coincidence->Add(C.GH, &bAlready);
			if (bAlready) { continue; }
		}

		OutEdges->Add(PCGEx::H64U(Index, C.PointIndex));
	}
}

#if WITH_EDITOR
FString UPCGExProbeValencyProviderSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif
