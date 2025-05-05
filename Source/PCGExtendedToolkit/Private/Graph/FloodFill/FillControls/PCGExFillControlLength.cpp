// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/FloodFill/FillControls/PCGExFillControlLength.h"




#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

bool UPCGExFillControlLength::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!UPCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryLength* TypedFactory = Cast<UPCGExFillControlsFactoryLength>(Factory);
	bUsePathLength = TypedFactory->Config.bUsePathLength;

	DistanceLimit = TypedFactory->Config.GetValueSettingMaxLength();
	if (!DistanceLimit->Init(InContext, GetSourceFacade())) { return false; }

	return true;
}

bool UPCGExFillControlLength::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return (bUsePathLength ? Candidate.PathDistance : Candidate.Distance) <= DistanceLimit->Read(GetSettingsIndex(Diffusion));
}

bool UPCGExFillControlLength::IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return (bUsePathLength ? Candidate.PathDistance : Candidate.Distance) <= DistanceLimit->Read(GetSettingsIndex(Diffusion));
}

bool UPCGExFillControlLength::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	return (bUsePathLength ? Candidate.PathDistance : Candidate.Distance) <= DistanceLimit->Read(GetSettingsIndex(Diffusion));
}

TSharedPtr<UPCGExFillControlOperation> UPCGExFillControlsFactoryLength::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(UPCGExFillControlLength)
	PCGEX_FORWARD_FILLCONTROL_OPERATION
	return NewOperation;
}

void UPCGExFillControlsFactoryLength::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);

	if (Config.Source == EPCGExFloodFillSettingSource::Vtx)
	{
		FacadePreloader.Register<double>(InContext, Config.MaxLengthAttribute);
	}
}

UPCGExFactoryData* UPCGExFillControlsLengthProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFillControlsFactoryLength* NewFactory = InContext->ManagedObjects->New<UPCGExFillControlsFactoryLength>();
	PCGEX_FORWARD_FILLCONTROL_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExFillControlsLengthProviderSettings::GetDisplayName() const
{
	FString DName = GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Fill Control"), TEXT("FC")) + TEXT(" @ ");

	if (Config.MaxLengthInput == EPCGExInputValueType::Attribute) { DName += Config.MaxLengthAttribute.ToString(); }
	else { DName += FString::Printf(TEXT("%.1f"), Config.MaxLength); }

	return DName;
}
#endif
