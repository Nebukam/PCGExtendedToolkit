// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/FloodFill/FillControls/PCGExFillControlOperation.h"





#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

bool UPCGExFillControlOperation::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	Handler = InHandler;
	Cluster = InHandler->Cluster;
	return true;
}

bool UPCGExFillControlOperation::ChecksCapture() const
{
	return (Factory->ConfigBase.Steps & static_cast<uint8>(EPCGExFloodFillControlStepsFlags::Capture)) != 0;
}

bool UPCGExFillControlOperation::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& InCandidate)
{
	return true;
}

bool UPCGExFillControlOperation::ChecksProbe() const
{
	return (Factory->ConfigBase.Steps & static_cast<uint8>(EPCGExFloodFillControlStepsFlags::Probing)) != 0;
}

bool UPCGExFillControlOperation::IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& InCandidate)
{
	return true;
}

bool UPCGExFillControlOperation::ChecksCandidate() const
{
	return (Factory->ConfigBase.Steps & static_cast<uint8>(EPCGExFloodFillControlStepsFlags::Candidate)) != 0;
}

bool UPCGExFillControlOperation::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	return false;
}

void UPCGExFillControlOperation::Cleanup()
{
	Factory = nullptr;
	Cluster.Reset();
	SettingsIndex.Reset();
	Super::Cleanup();
}

int32 UPCGExFillControlOperation::GetSettingsIndex(const PCGExFloodFill::FDiffusion* Diffusion) const
{
	return *(SettingsIndex->GetData() + Diffusion->Index);
}

TSharedPtr<PCGExData::FFacade> UPCGExFillControlOperation::GetSourceFacade() const
{
	return Factory->ConfigBase.Source == EPCGExFloodFillSettingSource::Seed ? Handler->SeedsDataFacade : Handler->VtxDataFacade;
}
