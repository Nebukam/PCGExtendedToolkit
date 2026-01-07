// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Elements/FloodFill/FillControls/PCGExFillControlDepth.h"


#include "Containers/PCGExManagedObjects.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Elements/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExFillControlConfigDepth, MaxDepth, int32, MaxDepthInput, MaxDepthAttribute, MaxDepth)

bool FPCGExFillControlDepth::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!FPCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryDepth* TypedFactory = Cast<UPCGExFillControlsFactoryDepth>(Factory);

	DepthLimit = TypedFactory->Config.GetValueSettingMaxDepth();
	if (!DepthLimit->Init(GetSourceFacade())) { return false; }

	return true;
}

bool FPCGExFillControlDepth::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return Candidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

bool FPCGExFillControlDepth::IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return Candidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

bool FPCGExFillControlDepth::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	return Candidate.Depth <= DepthLimit->Read(GetSettingsIndex(Diffusion));
}

TSharedPtr<FPCGExFillControlOperation> UPCGExFillControlsFactoryDepth::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(FillControlDepth)
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
