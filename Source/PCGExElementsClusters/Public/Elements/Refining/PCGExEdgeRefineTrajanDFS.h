// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineTrajanDFS.generated.h"

/**
 * 
 */
class FPCGExEdgeRefineTrajanDFS : public FPCGExEdgeRefineOperation
{
public:
	virtual void Process() override
	{
		const int32 NumNodes = Cluster->Nodes->Num();

		TArray<int32> Disc;   // discovery time
		TArray<int32> Low;    // lowest reachable ancestor
		TArray<int32> Parent; // DFS parent

		Disc.Init(-1, NumNodes);
		Low.Init(-1, NumNodes);
		Parent.Init(-1, NumNodes);

		int32 Time = 0;

		TArray<int32> Bridges;
		Bridges.Reserve(Cluster->Edges->Num());


		TFunction<void(int32)> DFS = [&](const int32 Index)
		{
			const PCGExClusters::FNode& Current = *Cluster->GetNode(Index);

			Disc[Index] = Low[Index] = Time++;

			for (const PCGExGraphs::FLink Lk : Current.Links)
			{
				if (Disc[Lk.Node] == -1)
				{
					Parent[Lk.Node] = Index;

					DFS(Lk.Node);

					Low[Index] = FMath::Min(Low[Index], Low[Lk.Node]);

					if (Low[Lk.Node] > Disc[Index])
					{
						Bridges.Add(Lk.Edge);
					}
				}
				else if (Lk.Node != Parent[Index])
				{
					Low[Index] = FMath::Min(Low[Index], Disc[Lk.Node]);
				}
			}
		};

		for (int32 i = 0; i < NumNodes; ++i) { if (Disc[i] == -1) { DFS(i); } }
		for (const int32 BridgeIndex : Bridges) { Cluster->GetEdge(BridgeIndex)->bValid = bInvert; }
	}

	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : DFS (Trajan)", PCGExNodeLibraryDoc="clusters/refine-cluster/dfs-trajan"))
class UPCGExEdgeRefineTrajanDFS : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return !bInvert; }

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineTrajanDFS* TypedOther = Cast<UPCGExEdgeRefineTrajanDFS>(Other))
		{
			bInvert = TypedOther->bInvert;
		}
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	PCGEX_CREATE_REFINE_OPERATION(EdgeRefineTrajanDFS, { Operation->bInvert = bInvert; })
};
