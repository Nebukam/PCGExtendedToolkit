// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Probes/PCGExProbeValence.h"

#include "PCGExH.h"
#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExProbingCandidates.h"

UPCGExFactoryData* UPCGExProbeValenceProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExProbeFactoryValence* NewFactory = InContext->ManagedObjects->New<UPCGExProbeFactoryValence>();
	NewFactory->Config = Config;

	// Load and cache socket collection
	if (!Config.SocketCollection.IsNull())
	{
		UPCGExValenceSocketCollection* Collection = Config.SocketCollection.LoadSynchronous();
		if (Collection)
		{
			if (!NewFactory->SocketCache.BuildFrom(Collection))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Failed to build socket cache from Valence Socket Collection."));
				return nullptr;
			}
		}
		else
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Failed to load Valence Socket Collection."));
			return nullptr;
		}
	}
	else
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No Valence Socket Collection provided."));
		return nullptr;
	}

	return NewFactory;
}

void UPCGExProbeValenceProviderSettings::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	if (!Config.SocketCollection.IsNull())
	{
		InContext->AddAssetDependency(Config.SocketCollection.ToSoftObjectPath());
	}
}

TSharedPtr<FPCGExProbeOperation> UPCGExProbeFactoryValence::CreateOperation(FPCGExContext* InContext) const
{
	TSharedPtr<FPCGExProbeValence> NewOperation = MakeShared<FPCGExProbeValence>();
	NewOperation->Config = Config;
	NewOperation->SocketCache = SocketCache;
	return NewOperation;
}

namespace PCGExProbeValence
{
	FScopedContainer::FScopedContainer(const PCGExMT::FScope& InScope)
		: PCGExMT::FScopedContainer(InScope)
	{
	}

	void FScopedContainer::Init(const PCGExValence::FSocketCache& SocketCache, const bool bCopyDirs)
	{
		const int32 NumDirs = SocketCache.Num();
		BestDotsBuffer.SetNumUninitialized(NumDirs);
		BestDistsBuffer.SetNumUninitialized(NumDirs);
		BestIdxBuffer.SetNumUninitialized(NumDirs);

		if (bCopyDirs) { WorkingDirs = SocketCache.Directions; }
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

TSharedPtr<PCGExMT::FScopedContainer> FPCGExProbeValence::GetScopedContainer(const PCGExMT::FScope& InScope) const
{
	TSharedPtr<PCGExProbeValence::FScopedContainer> ScopedContainer = MakeShared<PCGExProbeValence::FScopedContainer>(InScope);
	ScopedContainer->Init(SocketCache, SocketCache.bTransformDirection);
	return ScopedContainer;
}

bool FPCGExProbeValence::RequiresChainProcessing() const { return false; }

bool FPCGExProbeValence::Prepare(FPCGExContext* InContext)
{
	if (!FPCGExProbeOperation::Prepare(InContext)) { return false; }

	bUseBestDot = Config.Favor == EPCGExProbeValencePriorization::Dot;

	return true;
}

void FPCGExProbeValence::ProcessCandidates(const int32 Index, TArray<PCGExProbing::FCandidate>& Candidates, TSet<uint64>* Coincidence, const FVector& ST, TSet<uint64>* OutEdges, PCGExMT::FScopedContainer* Container)
{
	const int32 DirCount = SocketCache.Num();
	if (DirCount == 0) { return; }

	PCGExProbeValence::FScopedContainer* LocalContainer = static_cast<PCGExProbeValence::FScopedContainer*>(Container);
	LocalContainer->Reset();

	TArray<double>& BestDotsBuffer = LocalContainer->BestDotsBuffer;
	TArray<double>& BestDistsBuffer = LocalContainer->BestDistsBuffer;
	TArray<int32>& BestIdxBuffer = LocalContainer->BestIdxBuffer;
	TArray<FVector>& WorkingDirs = LocalContainer->WorkingDirs;

	const double DotThreshold = SocketCache.DotThreshold;

	// Precompute world dirs if using transform
	if (SocketCache.bTransformDirection)
	{
		const FTransform& WorkingTransform = *(WorkingTransforms->GetData() + Index);
		for (int32 d = 0; d < DirCount; ++d)
		{
			WorkingDirs[d] = WorkingTransform.TransformVectorNoScale(SocketCache.Directions[d]);
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

		// Inner loop - check each socket direction
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
FString UPCGExProbeValenceProviderSettings::GetDisplayName() const
{
	return TEXT("");
}
#endif
