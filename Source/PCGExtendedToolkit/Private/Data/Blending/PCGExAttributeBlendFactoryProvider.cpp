// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"
#include "Data/Blending/PCGExProxyDataBlending.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExCreateAttributeBlend"
#define PCGEX_NAMESPACE CreateAttributeBlend

void FPCGExAttributeBlendConfig::Init()
{
	bRequiresWeight =
		BlendMode == EPCGExABBlendingType::Lerp ||
		BlendMode == EPCGExABBlendingType::Weight ||
		BlendMode == EPCGExABBlendingType::WeightedSubtract ||
		BlendMode == EPCGExABBlendingType::WeightedAdd;

	if (!bUseLocalCurve) { LocalWeightCurve.ExternalCurve = WeightCurve.Get(); }
	ScoreCurveObj = LocalWeightCurve.GetRichCurveConst();
}

bool UPCGExAttributeBlendOperation::PrepareForData(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
	PrimaryDataFacade = InDataFacade;

	// TODO : Determine output type

	PCGExData::ESource SourceA = PCGExData::ESource::Out;
	EPCGMetadataTypes TypeA = EPCGMetadataTypes::Unknown;

	PCGExData::ESource SourceB = PCGExData::ESource::Out;
	EPCGMetadataTypes TypeB = EPCGMetadataTypes::Unknown;

	if (PCGEx::TryGetTypeAndSource(Config.OperandA, InDataFacade, TypeA, SourceA))
	{
	}
	else
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, OperandA, Config.OperandA)
		return false;
	}

	if (PCGEx::TryGetTypeAndSource(Config.OperandB, InDataFacade, TypeB, SourceB))
	{
	}
	else
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, OperandB, Config.OperandB)
		return false;
	}

	// TODO : Support @Previous attribute and swap it with the previous ops' output path

	Config.OperandA = Config.OperandA.CopyAndFixLast(InDataFacade->Source->GetData(SourceA));
	Config.OperandB = Config.OperandB.CopyAndFixLast(InDataFacade->Source->GetData(SourceB));

	// TODO : If we target an attribute, make sure to initialize it as writable first first 

	Config.OutputTo = Config.OutputTo.CopyAndFixLast(InDataFacade->Source->GetOut());

	EPCGMetadataTypes OutType = EPCGMetadataTypes::Unknown;

	PCGEx::ExecuteWithRightType(
		OutType, [&](auto DummyValue)
		{
			using T = decltype(DummyValue);
			Blender = PCGExDataBlending::CreateProxyBlender<T>(
				InContext, InDataFacade, Config.BlendMode,
				Config.OperandA, Config.OperandB, Config.OutputTo);
		});
	
	return true;
}

void UPCGExAttributeBlendOperation::Cleanup()
{
	Blender.Reset();
	Super::Cleanup();
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

bool UPCGExAttributeBlendFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandB, Consumable)

	return true;
}

#if WITH_EDITOR
void UPCGExAttributeBlendFactoryProviderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Config.bRequiresWeight =
		Config.BlendMode == EPCGExABBlendingType::Lerp ||
		Config.BlendMode == EPCGExABBlendingType::Weight ||
		Config.BlendMode == EPCGExABBlendingType::WeightedSubtract ||
		Config.BlendMode == EPCGExABBlendingType::WeightedAdd;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TArray<FPCGPreConfiguredSettingsInfo> UPCGExAttributeBlendFactoryProviderSettings::GetPreconfiguredInfo() const
{
	const TSet ValuesToSkip = {
		EPCGExABBlendingType::None,
	};

#if PCGEX_ENGINE_VERSION == 503
	return PCGMetadataElementCommon::FillPreconfiguredSettingsInfoFromEnum<EPCGExABBlendingType>(ValuesToSkip);
#else
	return PCGMetadataElementCommon::FillPreconfiguredSettingsInfoFromEnum<EPCGExABBlendingType>(ValuesToSkip, FTEXT("Blend : "));
#endif
}
#endif

void UPCGExAttributeBlendFactoryProviderSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExABBlendingType>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			Config.BlendMode = static_cast<EPCGExABBlendingType>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

UPCGExFactoryData* UPCGExAttributeBlendFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExAttributeBlendFactory* NewFactory = InContext->ManagedObjects->New<UPCGExAttributeBlendFactory>();
	NewFactory->Priority = Priority;

	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExAttributeBlendFactoryProviderSettings::GetDisplayName() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExABBlendingType>())
	{
		return FString::Printf(TEXT("Blend Op : %s"), *EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Config.BlendMode)).ToString());
	}

	return TEXT("PCGEx | Blend Op");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
