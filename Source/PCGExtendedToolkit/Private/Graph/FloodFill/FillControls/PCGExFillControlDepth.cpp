// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/FloodFill/FillControls/PCGExFillControlDepth.h"




#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

bool PCGExFillControlDepth::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!PCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryDepth* TypedFactory = Cast<UPCGExFillControlsFactoryDepth>(Factory);

	DepthLimit = TypedFactory->Config.GetValueSettingMaxDepth();
	if (!DepthLimit->Init(InContext, GetSourceFacade())) { return false; }

	return true;
}

bool PCGExFillControlDepth::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return Candidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

bool PCGExFillControlDepth::IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return Candidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

bool PCGExFillControlDepth::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	return Candidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

TSharedPtr<PCGExFillControlOperation> UPCGExFillControlsFactoryDepth::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(PCGExFillControlDepth)
	PCGEX_FORWARD_FILLCONTROL_OPERATION
	return NewOperation;
}

void UPCGExFillControlsFactoryDepth::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);

	if (Config.Source == EPCGExFloodFillSettingSource::Vtx)
	{
		FacadePreloader.Register<int32>(InContext, Config.MaxDepthAttribute);
	}
}

UPCGExFactoryData* UPCGExFillControlsDepthProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFillControlsFactoryDepth* NewFactory = InContext->ManagedObjects->New<UPCGExFillControlsFactoryDepth>();
	PCGEX_FORWARD_FILLCONTROL_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExFillControlsDepthProviderSettings::GetDisplayName() const
{
	FString DName = GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Fill Control"), TEXT("FC")) + TEXT(" @ ");

	if (Config.MaxDepthInput == EPCGExInputValueType::Attribute) { DName += Config.MaxDepthAttribute.ToString(); }
	else { DName += FString::Printf(TEXT("%d"), Config.MaxDepth); }

	return DName;
}
#endif
