// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExCreateAttributeBlend"
#define PCGEX_NAMESPACE CreateAttributeBlend

void FPCGExAttributeBlendConfig::Init()
{
	bRequiresWeight =
		BlendMode == EPCGExDataBlendingType::Lerp ||
		BlendMode == EPCGExDataBlendingType::Weight ||
		BlendMode == EPCGExDataBlendingType::WeightedSubtract ||
		BlendMode == EPCGExDataBlendingType::WeightedSum;

	if (!bUseLocalCurve) { LocalWeightCurve.ExternalCurve = WeightCurve.Get(); }
	ScoreCurveObj = LocalWeightCurve.GetRichCurveConst();
}

void UPCGExAttributeBlendOperation::PrepareForData(const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
}


UPCGExAttributeBlendOperation* UPCGExAttributeBlendFactory::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExAttributeBlendOperation* NewOperation = InContext->ManagedObjects->New<UPCGExAttributeBlendOperation>();
	return NewOperation;
}

void UPCGExAttributeBlendFactory::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	Super::RegisterAssetDependencies(InContext);
	if (Config.bRequiresWeight && !Config.bUseLocalCurve) { InContext->AddAssetDependency(Config.WeightCurve.ToSoftObjectPath()); }
}

#if WITH_EDITOR
void UPCGExAttributeBlendFactoryProviderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Config.bRequiresWeight =
		Config.BlendMode == EPCGExDataBlendingType::Lerp ||
		Config.BlendMode == EPCGExDataBlendingType::Weight ||
		Config.BlendMode == EPCGExDataBlendingType::WeightedSubtract ||
		Config.BlendMode == EPCGExDataBlendingType::WeightedSum;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

UPCGExParamFactoryBase* UPCGExAttributeBlendFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExAttributeBlendFactory* NewFactory = InContext->ManagedObjects->New<UPCGExAttributeBlendFactory>();
	return Super::CreateFactory(InContext, NewFactory);
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
