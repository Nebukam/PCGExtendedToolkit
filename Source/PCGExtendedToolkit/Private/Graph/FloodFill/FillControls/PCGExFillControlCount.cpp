// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/FloodFill/FillControls/PCGExFillControlCount.h"


#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"

bool PCGExFillControlCount::PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler)
{
	if (!PCGExFillControlOperation::PrepareForDiffusions(InContext, InHandler)) { return false; }

	const UPCGExFillControlsFactoryCount* TypedFactory = Cast<UPCGExFillControlsFactoryCount>(Factory);

	CountLimit = TypedFactory->Config.GetValueSettingMaxCount();
	if (!CountLimit->Init(InContext, GetSourceFacade())) { return false; }

	return true;
}

bool PCGExFillControlCount::IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate)
{
	const int32 Limit = CountLimit->Read(GetSettingsIndex(Diffusion));
	return Diffusion->Captured.Num() < Limit;
}

TSharedPtr<PCGExFillControlOperation> UPCGExFillControlsFactoryCount::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(PCGExFillControlCount)
	PCGEX_FORWARD_FILLCONTROL_OPERATION
	return NewOperation;
}

void UPCGExFillControlsFactoryCount::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);

	if (Config.Source == EPCGExFloodFillSettingSource::Vtx)
	{
		FacadePreloader.Register<int32>(InContext, Config.MaxCountAttribute);
	}
}

UPCGExFactoryData* UPCGExFillControlsCountProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExFillControlsFactoryCount* NewFactory = InContext->ManagedObjects->New<UPCGExFillControlsFactoryCount>();
	PCGEX_FORWARD_FILLCONTROL_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExFillControlsCountProviderSettings::GetDisplayName() const
{
	FString DName = GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Fill Control"), TEXT("FC")) + TEXT(" @ ");

	if (Config.MaxCountInput == EPCGExInputValueType::Attribute) { DName += Config.MaxCountAttribute.ToString(); }
	else { DName += FString::Printf(TEXT("%d"), Config.MaxCount); }

	return DName;
}
#endif
