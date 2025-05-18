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

bool FPCGExAttributeBlendOperation::PrepareForData(FPCGExContext* InContext)
{
	Weight = Config.Weighting.GetValueSettingWeight();
	if (!Weight->Init(InContext, WeightFacade)) { return false; }

	// Fix @Selectors based on siblings 
	if (!CopyAndFixSiblingSelector(InContext, Config.OperandA)) { return false; }
	if (!CopyAndFixSiblingSelector(InContext, Config.OperandB)) { return false; }
	if (!CopyAndFixSiblingSelector(InContext, Config.OutputTo)) { return false; }

	// TODO : Might be worth re-using the same descriptor if use copy


	PCGExData::FProxyDescriptor A = PCGExData::FProxyDescriptor(ConstantA ? ConstantA : Source_A_Facade);
	A.bIsConstant = A.DataFacade.Pin() != Source_A_Facade;
	if (!A.Capture(InContext, Config.OperandA, PCGExData::EIOSide::Out)) { return false; }

	PCGExData::FProxyDescriptor B = PCGExData::FProxyDescriptor(ConstantB ? ConstantB : Source_B_Facade);
	B.bIsConstant = B.DataFacade.Pin() != Source_B_Facade;
	if (!B.Capture(InContext, Config.OperandB, PCGExData::EIOSide::Out)) { return false; }

	PCGExData::FProxyDescriptor C = PCGExData::FProxyDescriptor(TargetFacade);
	C.Side = PCGExData::EIOSide::Out;

	Config.OperandA = A.Selector;
	Config.OperandB = B.Selector;

	Config.OutputTo = C.Selector = Config.OutputTo.CopyAndFixLast(TargetFacade->Source->GetOut());
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
			if (const FPCGMetadataAttributeBase* OutAttribute = TargetFacade->GetOut()->Metadata->GetConstAttribute(Config.OutputTo.GetAttributeName()))
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

	Blender = PCGExDataBlending::CreateProxyBlender(InContext, Config.BlendMode, A, B, C);

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
	NewOperation->ConstantA = ConstantA;
	NewOperation->ConstantB = ConstantB;
	return NewOperation;
}

bool UPCGExAttributeBlendFactory::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }

	ConstantA = PCGExData::TryGetSingleFacade(InContext, PCGExDataBlending::SourceConstantA, true, false);
	ConstantB = PCGExData::TryGetSingleFacade(InContext, PCGExDataBlending::SourceConstantB, true, false);

	return true;
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

void UPCGExAttributeBlendFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
}

void UPCGExAttributeBlendFactory::RegisterBuffersDependenciesForOperandA(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
}

void UPCGExAttributeBlendFactory::RegisterBuffersDependenciesForOperandB(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
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
	const TSet ValuesToSkip = {EPCGExABBlendingType::None};
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGExABBlendingType>(ValuesToSkip, FTEXT("Blend : "));
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

TArray<FPCGPinProperties> UPCGExAttributeBlendFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY_SINGLE(PCGExDataBlending::SourceConstantA, "Data used to read a constant from. Will read from the first element of the first data.", Advanced, {})
	PCGEX_PIN_ANY_SINGLE(PCGExDataBlending::SourceConstantB, "Data used to read a constant from. Will read from the first element of the first data.", Advanced, {})
	return PinProperties;
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

namespace PCGExDataBlending
{
	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExAttributeBlendFactory>>& Factories)
	{
		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Factories)
		{
			Factory->RegisterBuffersDependencies(InContext, FacadePreloader);
		}
	}

	void RegisterBuffersDependencies_SourceA(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExAttributeBlendFactory>>& Factories)
	{
		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Factories)
		{
			Factory->RegisterBuffersDependenciesForOperandA(InContext, FacadePreloader);
		}
	}

	void RegisterBuffersDependencies_SourceB(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExAttributeBlendFactory>>& Factories)
	{
		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Factories)
		{
			Factory->RegisterBuffersDependenciesForOperandB(InContext, FacadePreloader);
		}
	}

	void RegisterBuffersDependencies_Sources(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TArray<TObjectPtr<const UPCGExAttributeBlendFactory>>& Factories)
	{
		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Factories)
		{
			Factory->RegisterBuffersDependenciesForOperandA(InContext, FacadePreloader);
			Factory->RegisterBuffersDependenciesForOperandB(InContext, FacadePreloader);
		}
	}

	FBlendOpsManager::FBlendOpsManager(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		SetWeightFacade(InDataFacade);
		SetSources(InDataFacade);
		SetTargetFacade(InDataFacade);
		Operations = MakeShared<TArray<TSharedPtr<FPCGExAttributeBlendOperation>>>();
	}

	FBlendOpsManager::FBlendOpsManager()
	{
		Operations = MakeShared<TArray<TSharedPtr<FPCGExAttributeBlendOperation>>>();
	}

	void FBlendOpsManager::SetWeightFacade(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		WeightFacade = InDataFacade;
	}

	void FBlendOpsManager::SetSources(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		SetSourceA(InDataFacade);
		SetSourceB(InDataFacade);
	}

	void FBlendOpsManager::SetSourceA(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		SourceAFacade = InDataFacade;
	}

	void FBlendOpsManager::SetSourceB(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		SourceBFacade = InDataFacade;
	}

	void FBlendOpsManager::SetTargetFacade(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		TargetFacade = InDataFacade;
	}

	bool FBlendOpsManager::Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExAttributeBlendFactory>>& InFactories) const
	{
		check(WeightFacade)
		check(SourceAFacade)
		check(SourceBFacade)
		check(TargetFacade)

		const TSharedRef<PCGExData::FFacade> InDataFacade = SourceAFacade.ToSharedRef();

		Operations->Reserve(InFactories.Num());

		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : InFactories)
		{
			TSharedPtr<FPCGExAttributeBlendOperation> Op = Factory->CreateOperation(InContext);
			if (!Op)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("An operation could not be created."));
				return false; // FAIL
			}

			// Assign blender facades
			Op->WeightFacade = WeightFacade;
			Op->Source_A_Facade = SourceAFacade;
			Op->Source_B_Facade = SourceBFacade;
			Op->TargetFacade = TargetFacade;

			Op->OpIdx = Operations->Add(Op);
			Op->SiblingOperations = Operations;

			if (!Op->PrepareForData(InContext))
			{
				return false; // FAIL
			}
		}

		return true;
	}

	void FBlendOpsManager::BeginMultiBlend(const int32 TargetIndex, TArray<PCGExDataBlending::FBlendTracker>& OutTrackers) const
	{
		OutTrackers.SetNumUninitialized(Operations->Num());
		for (int i = 0; i < Operations->Num(); i++) { OutTrackers[i] = (*(Operations->GetData() + i))->BeginMultiBlend(TargetIndex); }
	}

	void FBlendOpsManager::MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, TArray<PCGExDataBlending::FBlendTracker>& Trackers) const
	{
		for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->MultiBlend(SourceIndex, TargetIndex, Weight, Trackers[i]); }
	}

	void FBlendOpsManager::EndMultiBlend(const int32 TargetIndex, TArray<PCGExDataBlending::FBlendTracker>& Trackers) const
	{
		for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->EndMultiBlend(TargetIndex, Trackers[i]); }
	}

	void FBlendOpsManager::Cleanup(FPCGExContext* InContext)
	{
		TSet<TSharedPtr<PCGExData::FBufferBase>> DisabledBuffers;
		for (int i = 0; i < Operations->Num(); i++) { (*(Operations->GetData() + i))->CompleteWork(DisabledBuffers); }

		for (const TSharedPtr<PCGExData::FBufferBase>& Buffer : DisabledBuffers)
		{
			// If disabled buffer does not exist on input, delete it entierely
			if (!Buffer->OutAttribute) { continue; }
			if (!SourceAFacade->GetIn()->Metadata->HasAttribute(Buffer->OutAttribute->Name))
			{
				SourceAFacade->GetOut()->Metadata->DeleteAttribute(Buffer->OutAttribute->Name);
				// TODO : Check types and make sure we're not deleting something
			}

			if (Buffer->InAttribute)
			{
				// Log a warning that can be silenced that we may have removed a valid attribute
			}
		}

		Operations->Empty();
		Operations.Reset();
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
