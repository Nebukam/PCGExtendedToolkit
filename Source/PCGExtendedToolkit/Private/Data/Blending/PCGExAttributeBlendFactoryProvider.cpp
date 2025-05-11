// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"

#include "PCGExDetailsData.h"
#include "Data/Blending/PCGExProxyDataBlending.h"


#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExCreateAttributeBlend"
#define PCGEX_NAMESPACE CreateAttributeBlend

void FPCGExAttributeBlendWeight::Init()
{
	if (!bUseLocalCurve) { LocalWeightCurve.ExternalCurve = WeightCurve.Get(); }
	ScoreCurveObj = LocalWeightCurve.GetRichCurveConst();
}

void FPCGExAttributeBlendConfig::Init()
{
	bRequiresWeight =
		BlendMode == EPCGExABBlendingType::Lerp ||
		BlendMode == EPCGExABBlendingType::Weight ||
		BlendMode == EPCGExABBlendingType::WeightedSubtract ||
		BlendMode == EPCGExABBlendingType::WeightedAdd;

	Weighting.Init();
}

bool FPCGExAttributeBlendOperation::PrepareForData(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
	PrimaryDataFacade = InDataFacade;

	Weight = Config.Weighting.GetValueSettingWeight();
	if (!Weight->Init(InContext, InDataFacade)) { return false; }

	// Fix @Selectors based on siblings 
	if (!CopyAndFixSiblingSelector(InContext, Config.OperandA)) { return false; }
	if (!CopyAndFixSiblingSelector(InContext, Config.OperandB)) { return false; }
	if (!CopyAndFixSiblingSelector(InContext, Config.OutputTo)) { return false; }

	PCGExData::FProxyDescriptor A = PCGExData::FProxyDescriptor();
	if (!A.Capture(InContext, InDataFacade, Config.OperandA, PCGExData::ESource::Out)) { return false; }

	PCGExData::FProxyDescriptor B = PCGExData::FProxyDescriptor();
	if (!B.Capture(InContext, InDataFacade, Config.OperandB, PCGExData::ESource::Out)) { return false; }

	PCGExData::FProxyDescriptor C = PCGExData::FProxyDescriptor();
	C.Source = PCGExData::ESource::Out;

	Config.OperandA = A.Selector;
	Config.OperandB = B.Selector;

	Config.OutputTo = C.Selector = Config.OutputTo.CopyAndFixLast(InDataFacade->Source->GetOut());
	C.UpdateSubSelection();

	PCGEx::FSubSelection OutputSubselection(Config.OutputTo);
	EPCGMetadataTypes RealTypeC = EPCGMetadataTypes::Unknown;
	EPCGMetadataTypes WorkingTypeC = EPCGMetadataTypes::Unknown;

	if (Config.OutputTo.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Only attributes and point properties are supported as outputs; it's not possible to write to extras."));
		return false;
	}
	if (Config.OutputTo.GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		if (Config.OutputType == EPCGExOperandAuthority::A) { RealTypeC = A.RealType; }
		else if (Config.OutputType == EPCGExOperandAuthority::B) { RealTypeC = B.RealType; }
		else if (Config.OutputType == EPCGExOperandAuthority::Custom) { RealTypeC = Config.CustomType; }
		else if (Config.OutputType == EPCGExOperandAuthority::Auto)
		{
			if (const FPCGMetadataAttributeBase* OutAttribute = InDataFacade->GetOut()->Metadata->GetConstAttribute(Config.OutputTo.GetAttributeName()))
			{
				// First, check for an existing attribute
				RealTypeC = static_cast<EPCGMetadataTypes>(OutAttribute->GetTypeId());
				// TODO : Account for possible desired cast to a different type in the blend stack
			}

			if (OutputSubselection.bIsValid && RealTypeC == EPCGMetadataTypes::Unknown)
			{
				// Take a wild guess based on subselection, if any
				RealTypeC = OutputSubselection.PossibleSourceType;
			}

			if (RealTypeC == EPCGMetadataTypes::Unknown)
			{
				// Ok we really have little to work with,
				// take a guess based on other attribute types and pick the broader type

				EPCGMetadataTypes TypeA = A.SubSelection.bIsValid && A.SubSelection.bIsFieldSet ? EPCGMetadataTypes::Double : A.RealType;
				EPCGMetadataTypes TypeB = B.SubSelection.bIsValid && B.SubSelection.bIsFieldSet ? EPCGMetadataTypes::Double : B.RealType;

				int32 RatingA = PCGEx::GetMetadataRating(TypeA);
				int32 RatingB = PCGEx::GetMetadataRating(TypeB);

				RealTypeC = RatingA > RatingB ? TypeA : TypeB;
			}
		}
	}
	else // Point property
	{
		RealTypeC = PCGEx::GetPropertyType(Config.OutputTo.GetPointProperty());
	}

	if (RealTypeC == EPCGMetadataTypes::Unknown)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Could not infer output type."));
		return false;
	}

	WorkingTypeC = C.SubSelection.GetSubType(RealTypeC);

	A.WorkingType = WorkingTypeC;
	B.WorkingType = WorkingTypeC;

	C.RealType = RealTypeC;
	C.WorkingType = WorkingTypeC;

	Blender = PCGExDataBlending::CreateProxyBlender(InContext, InDataFacade, Config.BlendMode, A, B, C);

	if (!Blender) { return false; }
	return true;
}

void FPCGExAttributeBlendOperation::CompleteWork(TSet<TSharedPtr<PCGExData::FBufferBase>>& OutDisabledBuffers)
{
	if (Blender)
	{
		if (TSharedPtr<PCGExData::FBufferBase> OutputBuffer = Blender->GetOutputBuffer())
		{
			if (Config.bTransactional)
			{
				OutputBuffer->Disable();
				OutDisabledBuffers.Add(OutputBuffer);
			}
			else
			{
				OutputBuffer->Enable();
				OutDisabledBuffers.Remove(OutputBuffer);
			}
		}

		// TODO : Restore point properties to their original values...?
	}
}

bool FPCGExAttributeBlendOperation::CopyAndFixSiblingSelector(FPCGExContext* InContext, FPCGAttributePropertyInputSelector& Selector) const
{
	// TODO : Support index shortcuts like @0 @1 @2 etc

	if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		if (Selector.GetAttributeName() == PCGEx::PreviousAttributeName)
		{
			const TSharedPtr<FPCGExAttributeBlendOperation> PreviousOperation = SiblingOperations && SiblingOperations->IsValidIndex(OpIdx - 1) ? (*SiblingOperations.Get())[OpIdx - 1] : nullptr;

			if (!PreviousOperation)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("There is no valid #Previous attribute. Check priority order!"));
				return false;
			}

			Selector = PreviousOperation->Config.OutputTo;
			return true;
		}

		{
			FString Shortcut = Selector.GetAttributeName().ToString();
			if (Shortcut.RemoveFromStart(TEXT("#")))
			{
				if (Shortcut.IsNumeric())
				{
					int32 Idx = FCString::Atoi(*Shortcut);
					const TSharedPtr<FPCGExAttributeBlendOperation> TargetOperation = SiblingOperations && SiblingOperations->IsValidIndex(Idx) ? (*SiblingOperations.Get())[Idx] : nullptr;

					if (TargetOperation.Get() == this)
					{
						PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attempting to reference self using #INDEX, this is not allowed -- you can only reference previous operations."));
						return false;
					}

					if (!TargetOperation)
					{
						PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("There is no valid operation at the specified #INDEX. Check priority order -- you can only reference previous operations."));
						return false;
					}

					Selector = TargetOperation->Config.OutputTo;
					return true;
				}
			}
		}
	}

	return true;
}

TSharedPtr<FPCGExAttributeBlendOperation> UPCGExAttributeBlendFactory::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(AttributeBlendOperation)
	NewOperation->Config = Config;
	NewOperation->Config.Init();
	return NewOperation;
}

void UPCGExAttributeBlendFactory::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	Super::RegisterAssetDependencies(InContext);
	if (Config.bRequiresWeight && !Config.Weighting.bUseLocalCurve) { InContext->AddAssetDependency(Config.Weighting.WeightCurve.ToSoftObjectPath()); }
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
	NewFactory->Config = Config;

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

bool PCGExDataBlending::PrepareBlendOps(
	FPCGExContext* InContext,
	const TSharedRef<PCGExData::FFacade>& InDataFacade,
	const TArray<TObjectPtr<const UPCGExAttributeBlendFactory>>& InFactories,
	const TSharedPtr<TArray<TSharedPtr<FPCGExAttributeBlendOperation>>>& OutOperations)
{
	for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : InFactories)
	{
		TSharedPtr<FPCGExAttributeBlendOperation> Op = Factory->CreateOperation(InContext);
		if (!Op)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("An operation could not be created."));
			return false; // FAIL
		}

		Op->OpIdx = OutOperations->Add(Op);
		Op->SiblingOperations = OutOperations;

		if (!Op->PrepareForData(InContext, InDataFacade))
		{
			return false; // FAIL
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
