// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExNumericCompareNearestFilter.h"


#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExData.h"
#include "Details/PCGExSettingsDetails.h"
#include "PCGExMatching/Public/Helpers/PCGExTargetsHandler.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_SETTING_VALUE_IMPL(FPCGExNumericCompareNearestFilterConfig, OperandB, double, CompareAgainst, OperandB, OperandBConstant)

bool UPCGExNumericCompareNearestFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	return true;
}

PCGExFactories::EPreparationResult UPCGExNumericCompareNearestFilterFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
{
	TargetsHandler = MakeShared<PCGExMatching::FTargetsHandler>();
	if (!TargetsHandler->Init(InContext, PCGExCommon::Labels::SourceTargetsLabel)) { return PCGExFactories::EPreparationResult::MissingData; }

	TargetsHandler->SetDistances(Config.DistanceDetails);
	TargetsHandler->ForEachPreloader([&](PCGExData::FFacadePreloader& Preloader) { Preloader.Register<double>(InContext, Config.OperandA); });

	OperandA = MakeShared<TArray<TSharedPtr<PCGExData::TBuffer<double>>>>();
	OperandA->Reserve(TargetsHandler->Num());

	TWeakPtr<FPCGContextHandle> WeakHandle = InContext->GetOrCreateHandle();
	TargetsHandler->TargetsPreloader->OnCompleteCallback = [this, WeakHandle]()
	{
		PCGEX_SHARED_CONTEXT_VOID(WeakHandle)
		const bool bError = TargetsHandler->ForEachTarget([&](const TSharedRef<PCGExData::FFacade>& Target, const int32 TargetIndex, bool& bBreak)
		{
			// Prep weights
			TSharedPtr<PCGExData::TBuffer<double>> LocalOperandA = Target->GetBroadcaster<double>(Config.OperandA, true);
			OperandA->Add(LocalOperandA);
			if (!LocalOperandA)
			{
				bBreak = true;
				PCGEX_LOG_INVALID_SELECTOR_C(SharedContext.Get(), Operand A, Config.OperandA)
			}
		});

		PrepResult = bError ? PCGExFactories::EPreparationResult::Fail : PCGExFactories::EPreparationResult::Success;
	};

	TargetsHandler->StartLoading(TaskManager);
	return Super::Prepare(InContext, TaskManager);
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExNumericCompareNearestFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FNumericCompareNearestFilter>(this);
}

void UPCGExNumericCompareNearestFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.OperandB); }
}

bool UPCGExNumericCompareNearestFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)

	return true;
}

void UPCGExNumericCompareNearestFilterFactory::BeginDestroy()
{
	TargetsHandler.Reset();
	Super::BeginDestroy();
}

bool PCGExPointFilter::FNumericCompareNearestFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	if (!TargetsHandler || TargetsHandler->IsEmpty()) { return false; }

	OperandB = TypedFilterFactory->Config.GetValueSettingOperandB(PCGEX_QUIET_HANDLING);
	if (!OperandB->Init(PointDataFacade, false)) { return false; }

	if (TypedFilterFactory->Config.bIgnoreSelf) { IgnoreList.Add(InPointDataFacade->GetIn()); }

	return true;
}

bool PCGExPointFilter::FNumericCompareNearestFilter::Test(const int32 PointIndex) const
{
	const double B = OperandB->Read(PointIndex);
	const PCGExData::FConstPoint SourcePt = PointDataFacade->GetInPoint(PointIndex);
	PCGExData::FConstPoint TargetPt = PCGExData::FConstPoint();

	double BestDist = MAX_dbl;
	TargetsHandler->FindClosestTarget(SourcePt, TargetPt, BestDist, &IgnoreList);

	if (!TargetPt.IsValid()) { return false; }

	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, (OperandA->GetData() + TargetPt.IO)->Get()->Read(TargetPt.Index), B, TypedFilterFactory->Config.Tolerance);
}

TArray<FPCGPinProperties> UPCGExNumericCompareNearestFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExCommon::Labels::SourceTargetsLabel, TEXT("Target points to read operand B from"), Required)
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(NumericCompareNearest)

#if WITH_EDITOR
FString UPCGExNumericCompareNearestFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandA) + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
