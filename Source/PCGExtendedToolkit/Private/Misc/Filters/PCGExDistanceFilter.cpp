// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDistanceFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExDistanceFilterFactory::SupportsLiveTesting()
{
	return Config.CompareAgainst == EPCGExInputValueType::Constant;
}

bool UPCGExDistanceFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	return false;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExDistanceFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::FDistanceFilter>(this);
}

bool UPCGExDistanceFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.DistanceThreshold, Consumable)

	return true;
}

bool UPCGExDistanceFilterFactory::Prepare(FPCGExContext* InContext)
{
	TSharedPtr<PCGExData::FPointIOCollection> PointIOCollection = MakeShared<PCGExData::FPointIOCollection>(InContext, PCGEx::SourceTargetsLabel);
	if (PointIOCollection->IsEmpty()) { return false; }

	Octrees.Reserve(PointIOCollection->Num());
	TargetsPtr.Reserve(PointIOCollection->Num());

	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : PointIOCollection->Pairs)
	{
		Octrees.Add(&PointIO->GetIn()->GetOctree());
		TargetsPtr.Add(reinterpret_cast<uintptr_t>(PointIO->GetIn()->GetPoints().GetData()));
	}

	return Super::Prepare(InContext);
}

void UPCGExDistanceFilterFactory::BeginDestroy()
{
	Super::BeginDestroy();
}

bool PCGExPointsFilter::FDistanceFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	if (!Octrees) { return false; }

	NumTargets = Octrees->Num();

	SelfPtr = reinterpret_cast<uintptr_t>(InPointDataFacade->GetIn()->GetPoints().GetData());
	Distances = TypedFilterFactory->Config.DistanceDetails.MakeDistances();

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
	{
		DistanceThresholdGetter = InPointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.DistanceThreshold);

		if (!DistanceThresholdGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Distance Threshold attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.DistanceThreshold.GetName())));
			return false;
		}
	}

	return true;
}

TArray<FPCGPinProperties> UPCGExDistanceFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGEx::SourceTargetsLabel, TEXT("Target points to read operand B from"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(Distance)

#if WITH_EDITOR
FString UPCGExDistanceFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Distance ") + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.DistanceThreshold); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.DistanceThresholdConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
