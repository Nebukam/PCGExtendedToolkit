// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExBlendOpFactoryProvider.h"

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

bool FPCGExBlendOperation::PrepareForData(FPCGExContext* InContext)
{
	Weight = Config.Weighting.GetValueSettingWeight();
	if (!Weight->Init(WeightFacade)) { return false; }

	// Fix @Selectors based on siblings 
	if (!CopyAndFixSiblingSelector(InContext, Config.OperandA)) { return false; }
	if (Config.bUseOperandB) { if (!CopyAndFixSiblingSelector(InContext, Config.OperandB)) { return false; } }
	else { Config.OperandB = Config.OperandA; }

	switch (Config.OutputMode)
	{
	case EPCGExBlendOpOutputMode::SameAsA:
		Config.OutputTo = Config.OperandA;
		break;
	case EPCGExBlendOpOutputMode::SameAsB:
		Config.OutputTo = Config.OperandB;
		break;
	case EPCGExBlendOpOutputMode::New:
	case EPCGExBlendOpOutputMode::Transient:
		if (!CopyAndFixSiblingSelector(InContext, Config.OutputTo)) { return false; }
		break;
	}

	// Build output descriptor
	PCGExData::FProxyDescriptor C = PCGExData::FProxyDescriptor(TargetFacade, PCGExData::EProxyRole::Write);
	C.Role = PCGExData::EProxyRole::Write;
	C.Side = PCGExData::EIOSide::Out;

	Config.OutputTo = C.Selector = Config.OutputTo.CopyAndFixLast(TargetFacade->Source->GetOut());
	C.UpdateSubSelection();

	// Build main source descriptor
	PCGExData::FProxyDescriptor A = PCGExData::FProxyDescriptor(ConstantA ? ConstantA : Source_A_Facade, PCGExData::EProxyRole::Read);
	A.bIsConstant = A.DataFacade.Pin() != Source_A_Facade;
	if (!A.Capture(InContext, Config.OperandA, A.bIsConstant ? PCGExData::EIOSide::In : SideA)) { return false; }

	// Build secondary source descriptor
	bool bSkipSourceB = false;
	PCGExData::FProxyDescriptor B;

	if (bUsedForMultiBlendOnly || Config.BlendMode == EPCGExABBlendingType::CopySource)
	{
		bSkipSourceB = true;
		B = C;
	}
	else
	{
		B = PCGExData::FProxyDescriptor(ConstantB ? ConstantB : Source_B_Facade, PCGExData::EProxyRole::Read);
		B.bIsConstant = B.DataFacade.Pin() != Source_B_Facade;
		if (!B.Capture(InContext, Config.OperandB, B.bIsConstant ? PCGExData::EIOSide::In : SideB)) { return false; }
	}

	Config.OperandA = A.Selector;
	Config.OperandB = B.Selector;

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
		const FPCGMetadataAttributeBase* OutAttribute = TargetFacade->GetOut()->Metadata->GetConstAttribute(PCGEx::GetAttributeIdentifier(Config.OutputTo, TargetFacade->GetOut()));
		if (OutAttribute)
		{
			RealTypeC = static_cast<EPCGMetadataTypes>(OutAttribute->GetTypeId());

			if ((Config.OutputType == EPCGExOperandAuthority::A && RealTypeC != A.RealType) ||
				(Config.OutputType == EPCGExOperandAuthority::B && RealTypeC != B.RealType) ||
				(Config.OutputType == EPCGExOperandAuthority::Custom && RealTypeC != Config.CustomType))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("An output attribute existing type will differ from its desired type."));
			}
		}
		else
		{
			if (Config.OutputType == EPCGExOperandAuthority::A) { RealTypeC = A.RealType; }
			else if (Config.OutputType == EPCGExOperandAuthority::B) { RealTypeC = B.RealType; }
			else if (Config.OutputType == EPCGExOperandAuthority::Custom) { RealTypeC = Config.CustomType; }
			else if (Config.OutputType == EPCGExOperandAuthority::Auto)
			{
				if (OutAttribute)
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

	if (bSkipSourceB) { Blender = PCGExDataBlending::CreateProxyBlender(InContext, Config.BlendMode, A, C, Config.bResetValueBeforeMultiSourceBlend); }
	else { Blender = PCGExDataBlending::CreateProxyBlender(InContext, Config.BlendMode, A, B, C, Config.bResetValueBeforeMultiSourceBlend); }


	if (!Blender) { return false; }
	return true;
}

void FPCGExBlendOperation::BlendAutoWeight(const int32 SourceIndex, const int32 TargetIndex)
{
	Blender->Blend(SourceIndex, TargetIndex, Config.Weighting.ScoreCurveObj->Eval(Weight->Read(SourceIndex)));
}

void FPCGExBlendOperation::Blend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight)
{
	Blender->Blend(SourceIndex, TargetIndex, Config.Weighting.ScoreCurveObj->Eval(InWeight));
}

void FPCGExBlendOperation::Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double InWeight)
{
	Blender->Blend(SourceIndexA, SourceIndexB, TargetIndex, Config.Weighting.ScoreCurveObj->Eval(InWeight));
}

PCGEx::FOpStats FPCGExBlendOperation::BeginMultiBlend(const int32 TargetIndex)
{
	return Blender->BeginMultiBlend(TargetIndex);
}

void FPCGExBlendOperation::MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double InWeight, PCGEx::FOpStats& Tracker)
{
	Blender->MultiBlend(SourceIndex, TargetIndex, Config.Weighting.ScoreCurveObj->Eval(InWeight), Tracker);
}

void FPCGExBlendOperation::EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker)
{
	Blender->EndMultiBlend(TargetIndex, Tracker);
}

void FPCGExBlendOperation::CompleteWork(TSet<TSharedPtr<PCGExData::IBuffer>>& OutDisabledBuffers)
{
	if (Blender)
	{
		if (TSharedPtr<PCGExData::IBuffer> OutputBuffer = Blender->GetOutputBuffer())
		{
			if (Config.OutputMode == EPCGExBlendOpOutputMode::Transient)
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
	}
}

bool FPCGExBlendOperation::CopyAndFixSiblingSelector(FPCGExContext* InContext, FPCGAttributePropertyInputSelector& Selector) const
{
	// TODO : Support index shortcuts like @0 @1 @2 etc

	if (Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		if (Selector.GetAttributeName() == PCGEx::PreviousAttributeName)
		{
			const TSharedPtr<FPCGExBlendOperation> PreviousOperation = SiblingOperations && SiblingOperations->IsValidIndex(OpIdx - 1) ? (*SiblingOperations.Get())[OpIdx - 1] : nullptr;

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
					const TSharedPtr<FPCGExBlendOperation> TargetOperation = SiblingOperations && SiblingOperations->IsValidIndex(Idx) ? (*SiblingOperations.Get())[Idx] : nullptr;

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

TSharedPtr<FPCGExBlendOperation> UPCGExBlendOpFactory::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(BlendOperation)
	NewOperation->Config = Config;
	NewOperation->Config.Init();
	NewOperation->ConstantA = ConstantA;
	NewOperation->ConstantB = ConstantB;
	return NewOperation;
}

PCGExFactories::EPreparationResult UPCGExBlendOpFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	PCGExFactories::EPreparationResult Result = Super::Prepare(InContext, AsyncManager);
	if (Result != PCGExFactories::EPreparationResult::Success) { return Result; }

	ConstantA = PCGExData::TryGetSingleFacade(InContext, PCGExDataBlending::SourceConstantA, true, false);
	if (Config.bUseOperandB) { ConstantB = PCGExData::TryGetSingleFacade(InContext, PCGExDataBlending::SourceConstantB, true, false); }

	if (ConstantA)
	{
		InContext->ManagedObjects->Remove(const_cast<UPCGBasePointData*>(ConstantA->Source->GetIn()));
		AddDataDependency(ConstantA->Source->GetIn());
	}

	if (ConstantB)
	{
		InContext->ManagedObjects->Remove(const_cast<UPCGBasePointData*>(ConstantB->Source->GetIn()));
		AddDataDependency(ConstantB->Source->GetIn());
	}

	return Result;
}

void UPCGExBlendOpFactory::RegisterAssetDependencies(FPCGExContext* InContext) const
{
	Super::RegisterAssetDependencies(InContext);
	if (!Config.Weighting.bUseLocalCurve) { InContext->AddAssetDependency(Config.Weighting.WeightCurve.ToSoftObjectPath()); }
}

bool UPCGExBlendOpFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandB, Consumable)

	return true;
}

void UPCGExBlendOpFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	RegisterBuffersDependenciesForSourceA(InContext, FacadePreloader);
	RegisterBuffersDependenciesForSourceB(InContext, FacadePreloader);
}

void UPCGExBlendOpFactory::RegisterBuffersDependenciesForSourceA(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	FacadePreloader.TryRegister(InContext, Config.OperandA);
}

void UPCGExBlendOpFactory::RegisterBuffersDependenciesForSourceB(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	if (Config.bUseOperandB)
	{
		FacadePreloader.TryRegister(InContext, Config.OperandB);
	}
	else
	{
		FacadePreloader.TryRegister(InContext, Config.OperandA);
	}
}

#if WITH_EDITOR
void UPCGExBlendOpFactoryProviderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Config.bRequiresWeight =
		Config.BlendMode == EPCGExABBlendingType::Lerp ||
		Config.BlendMode == EPCGExABBlendingType::Weight ||
		Config.BlendMode == EPCGExABBlendingType::WeightedSubtract ||
		Config.BlendMode == EPCGExABBlendingType::WeightedAdd;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TArray<FPCGPreConfiguredSettingsInfo> UPCGExBlendOpFactoryProviderSettings::GetPreconfiguredInfo() const
{
	const TSet ValuesToSkip = {EPCGExABBlendingType::None};
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGExABBlendingType>(ValuesToSkip, FTEXT("Blend : {0}"));
}
#endif

void UPCGExBlendOpFactoryProviderSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExABBlendingType>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			Config.BlendMode = static_cast<EPCGExABBlendingType>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

TArray<FPCGPinProperties> UPCGExBlendOpFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_ANY_SINGLE(PCGExDataBlending::SourceConstantA, "Data used to read a constant from. Will read from the first element of the first data.", Advanced, {})

	if (Config.bUseOperandB)
	{
		PCGEX_PIN_ANY_SINGLE(PCGExDataBlending::SourceConstantB, "Data used to read a constant from. Will read from the first element of the first data.", Advanced, {})
	}

	return PinProperties;
}

UPCGExFactoryData* UPCGExBlendOpFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExBlendOpFactory* NewFactory = InContext->ManagedObjects->New<UPCGExBlendOpFactory>();
	NewFactory->Priority = Priority;
	NewFactory->Config = Config;

	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExBlendOpFactoryProviderSettings::GetDisplayName() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExABBlendingType>())
	{
		FString Str = FString::Printf(TEXT("%s %s"), *EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Config.BlendMode)).ToString(), *PCGEx::GetSelectorDisplayName(Config.OperandA));

		switch (Config.OutputMode)
		{
		case EPCGExBlendOpOutputMode::SameAsA:
			break;
		case EPCGExBlendOpOutputMode::SameAsB:
			if (Config.bUseOperandB) { Str += FString::Printf(TEXT(" ⇌ %s"), *PCGEx::GetSelectorDisplayName(Config.OperandB)); }
			else { Str += FString::Printf(TEXT(" → %s"), *PCGEx::GetSelectorDisplayName(Config.OperandB)); }
			break;
		case EPCGExBlendOpOutputMode::New:
			if (Config.bUseOperandB) { Str += FString::Printf(TEXT(" & %s"), *PCGEx::GetSelectorDisplayName(Config.OperandB)); }
			else { Str += FString::Printf(TEXT(" → %s"), *PCGEx::GetSelectorDisplayName(Config.OutputTo)); }
			break;
		case EPCGExBlendOpOutputMode::Transient:
			if (Config.bUseOperandB) { Str += FString::Printf(TEXT(" & %s"), *PCGEx::GetSelectorDisplayName(Config.OperandB)); }
			Str += FString::Printf(TEXT(" ⇢ %s"), *PCGEx::GetSelectorDisplayName(Config.OutputTo));
			break;
		}
		return Str;
	}

	return TEXT("PCGEx | Blend Op");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
