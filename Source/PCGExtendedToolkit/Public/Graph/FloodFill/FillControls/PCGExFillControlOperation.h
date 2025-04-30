// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"


#include "Graph/PCGExCluster.h"
#include "Graph/FloodFill/PCGExFloodFill.h"
#include "UObject/Object.h"

#include "PCGExFillControlOperation.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExFillControlOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPROPERTY()
	const UPCGExFillControlsFactoryData* Factory = nullptr;

	TSharedPtr<TArray<int32>> SettingsIndex;

	virtual bool PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler);

	virtual bool ChecksCapture() const;
	virtual bool IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate);

	virtual bool ChecksProbe() const;
	virtual bool IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate);

	virtual bool ChecksCandidate() const;
	virtual bool IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate);

	virtual void Cleanup() override;

	int32 GetSettingsIndex(const PCGExFloodFill::FDiffusion* Diffusion) const;

protected:
	TSharedPtr<PCGExData::FFacade> GetSourceFacade() const;

	TSharedPtr<PCGExCluster::FCluster> Cluster;
	TSharedPtr<PCGExFloodFill::FFillControlsHandler> Handler;
};
