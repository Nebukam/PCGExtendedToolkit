// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/FloodFill/FillControls/PCGExFillControlDepth.h"


#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

bool UPCGExFillControlDepth::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!Super::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryDepth* TypedFactory = Cast<UPCGExFillControlsFactoryDepth>(Factory);

	DepthLimit = PCGExDetails::MakeSettingValue<int32>(TypedFactory->Config.MaxDepthInput, TypedFactory->Config.MaxDepthAttribute, TypedFactory->Config.MaxDepth);
	if (!DepthLimit->Init(InContext, GetSourceFacade())) { return false; }

	return true;
}

bool UPCGExFillControlDepth::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& InCandidate)
{
	return InCandidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

bool UPCGExFillControlDepth::IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& InCandidate)
{
	return InCandidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

bool UPCGExFillControlDepth::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	return Candidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

void UPCGExFillControlDepth::Cleanup()
{
	DepthLimit.Reset();
	Super::Cleanup();
}

UPCGExFillControlOperation* UPCGExFillControlsFactoryDepth::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExFillControlDepth* NewOperation = InContext->ManagedObjects->New<UPCGExFillControlDepth>();
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
