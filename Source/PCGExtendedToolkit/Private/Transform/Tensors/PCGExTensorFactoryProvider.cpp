// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorFactoryProvider.h"

#include "Paths/PCGExPaths.h"
#include "Paths/PCGExSplineToPath.h"
#include "Transform/Tensors/PCGExTensorOperation.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTensor"
#define PCGEX_NAMESPACE CreateTensor

TSharedPtr<PCGExTensorOperation> UPCGExTensorFactoryData::CreateOperation(FPCGExContext* InContext) const
{
	return nullptr; // Create shape builder operation
}

bool UPCGExTensorFactoryData::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }
	return InitInternalData(InContext);
}

bool UPCGExTensorFactoryData::InitInternalData(FPCGExContext* InContext)
{
	return true;
}

void UPCGExTensorFactoryData::InheritFromOtherTensor(const UPCGExTensorFactoryData* InOtherTensor)
{
	const TSet<FString> Exclusions = {TEXT("Points"), TEXT("Splines"), TEXT("ManagedSplines")};
	PCGExHelpers::CopyProperties(this, InOtherTensor, &Exclusions);
	if (InOtherTensor->GetClass() == GetClass())
	{
		// Same type let's automate
	}
	else
	{
		// We can only attempt to inherit a few settings
	}
}

TArray<FPCGPinProperties> UPCGExTensorFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORY(PCGExTensor::SourceTensorConfigSourceLabel, "A tensor that already exist which settings will be used to override the settings of this one. This is to streamline re-using params between tensors, or to 'fake' the ability to transform tensors.", Advanced, {})
	return PinProperties;
}

UPCGExFactoryData* UPCGExTensorFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	TArray<FPCGTaggedData> Collection = InContext->InputData.GetInputsByPin(PCGExTensor::SourceTensorConfigSourceLabel);
	const UPCGExTensorFactoryData* InTensorReference = Collection.IsEmpty() ? nullptr : Cast<UPCGExTensorFactoryData>(Collection[0].Data);
	if (InTensorReference) { Cast<UPCGExTensorFactoryData>(InFactory)->InheritFromOtherTensor(InTensorReference); }

	return Super::CreateFactory(InContext, InFactory);
}

bool UPCGExTensorPointFactoryData::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }
	if (!InitInternalFacade(InContext)) { return false; }

	EffectorsArray = GetEffectorsArray();

	// Bulk of the work happens here
	EffectorsArray->Init(InContext, this);

	InputDataFacade->Flush(); // Flush cached buffers
	InputDataFacade.Reset();

	return true;
}

TSharedPtr<PCGExTensor::FEffectorsArray> UPCGExTensorPointFactoryData::GetEffectorsArray() const
{
	return MakeShared<PCGExTensor::FEffectorsArray>();
}

bool UPCGExTensorPointFactoryData::InitInternalFacade(FPCGExContext* InContext)
{
	InputDataFacade = PCGExData::TryGetSingleFacade(InContext, PCGExTensor::SourceEffectorsLabel, false, true);
	if (!InputDataFacade) { return false; }
	return true;
}

void UPCGExTensorPointFactoryData::PrepareSinglePoint(const int32 Index) const
{
}

TArray<FPCGPinProperties> UPCGExTensorPointFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGExTensor::SourceEffectorsLabel, "Single point collection that represent individual effectors within that tensor", Required, {})
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
