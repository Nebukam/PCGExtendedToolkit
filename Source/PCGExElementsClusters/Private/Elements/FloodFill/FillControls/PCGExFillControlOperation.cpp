// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Elements/FloodFill/FillControls/PCGExFillControlOperation.h"


#include "Elements/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

bool FPCGExFillControlOperation::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	Handler = InHandler;
	Cluster = InHandler->Cluster;
	return true;
}

bool FPCGExFillControlOperation::ChecksCapture() const
{
	return EnumHasAnyFlags(static_cast<EPCGExFloodFillControlStepsFlags>(Factory->ConfigBase.Steps), EPCGExFloodFillControlStepsFlags::Capture);
}

bool FPCGExFillControlOperation::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return true;
}

bool FPCGExFillControlOperation::ChecksProbe() const
{
	return EnumHasAnyFlags(static_cast<EPCGExFloodFillControlStepsFlags>(Factory->ConfigBase.Steps), EPCGExFloodFillControlStepsFlags::Probing);
}

bool FPCGExFillControlOperation::IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return true;
}

bool FPCGExFillControlOperation::ChecksCandidate() const
{
	return EnumHasAnyFlags(static_cast<EPCGExFloodFillControlStepsFlags>(Factory->ConfigBase.Steps), EPCGExFloodFillControlStepsFlags::Candidate);
}

bool FPCGExFillControlOperation::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	return false;
}

int32 FPCGExFillControlOperation::GetSettingsIndex(const PCGExFloodFill::FDiffusion* Diffusion) const
{
	return *(SettingsIndex->GetData() + Diffusion->Index);
}

TSharedPtr<PCGExData::FFacade> FPCGExFillControlOperation::GetSourceFacade() const
{
	return Factory->ConfigBase.Source == EPCGExFloodFillSettingSource::Seed ? Handler->SeedsDataFacade : Handler->VtxDataFacade;
}
