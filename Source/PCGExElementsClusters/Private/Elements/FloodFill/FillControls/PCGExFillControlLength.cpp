// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Elements/FloodFill/FillControls/PCGExFillControlLength.h"


#include "Containers/PCGExManagedObjects.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Elements/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExFillControlConfigLength, MaxLength, double, MaxLengthInput, MaxLengthAttribute, MaxLength)

bool FPCGExFillControlLength::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!FPCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryLength* TypedFactory = Cast<UPCGExFillControlsFactoryLength>(Factory);
	bUsePathLength = TypedFactory->Config.bUsePathLength;

	DistanceLimit = TypedFactory->Config.GetValueSettingMaxLength();
	if (!DistanceLimit->Init(GetSourceFacade())) { return false; }

	return true;
}

bool FPCGExFillControlLength::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return (bUsePathLength ? Candidate.PathDistance : Candidate.Distance) <= DistanceLimit->Read(GetSettingsIndex(Diffusion));
}

bool FPCGExFillControlLength::IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	return (bUsePathLength ? Candidate.PathDistance : Candidate.Distance) <= DistanceLimit->Read(GetSettingsIndex(Diffusion));
}

bool FPCGExFillControlLength::IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate)
{
	return (bUsePathLength ? Candidate.PathDistance : Candidate.Distance) <= DistanceLimit->Read(GetSettingsIndex(Diffusion));
}

TSharedPtr<FPCGExFillControlOperation> UPCGExFillControlsFactoryLength::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(FillControlLength)
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
