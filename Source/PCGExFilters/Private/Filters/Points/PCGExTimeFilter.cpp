// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExTimeFilter.h"


#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPolyPath.h"

#define LOCTEXT_NAMESPACE "PCGExTimeFilterDefinition"
#define PCGEX_NAMESPACE PCGExTimeFilterDefinition

PCGEX_SETTING_VALUE_IMPL(FPCGExTimeFilterConfig, OperandB, float, CompareAgainst, OperandB, OperandBConstant)

bool UPCGExTimeFilterFactory::SupportsCollectionEvaluation() const
{
	return Config.bCheckAgainstDataBounds;
}

bool UPCGExTimeFilterFactory::SupportsProxyEvaluation() const
{
	return Config.CompareAgainst == EPCGExInputValueType::Constant;
}

void UPCGExTimeFilterFactory::InitConfig_Internal()
{
	Super::InitConfig_Internal();
	LocalFidelity = Config.Fidelity;
	LocalExpansion = Config.Tolerance;
	LocalExpansionZ = -1;
	LocalSampleInputs = Config.SampleInputs;
	WindingMutation = Config.WindingMutation;
	bScaleTolerance = false;
	bUsedForInclusion = false;
	bIgnoreSelf = Config.bIgnoreSelf;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExTimeFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FTimeFilter>(this);
}

void UPCGExTimeFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.OperandB); }
}

bool UPCGExTimeFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)

	return true;
}

FName UPCGExTimeFilterFactory::GetInputLabel() const { return PCGExCommon::Labels::SourceTargetsLabel; }

namespace PCGExPointFilter
{
	bool FTimeFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

		OperandB = TypedFilterFactory->Config.GetValueSettingOperandB();
		if (!OperandB->Init(PointDataFacade)) { return false; }

		InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();
		return true;
	}

	bool FTimeFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		const FVector WorldPosition = Point.Transform.GetLocation();
		float Alpha = 0;

		if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Min) { Alpha = MAX_flt; }

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double BestDist = MAX_dbl;

			TypedFilterFactory->Octree->FindElementsWithBoundsTest(FBoxCenterAndExtent(WorldPosition, FVector::OneVector), [&](const PCGExOctree::FItem& Item)
			{
				float LocalAlpha = 0;
				const TSharedPtr<PCGExPaths::FPolyPath> Path = (*(TypedFilterFactory->PolyPaths.GetData() + Item.Index));
				const FTransform Closest = Path->GetClosestTransform(WorldPosition, LocalAlpha, false);

				const double D = FVector::DistSquared(Closest.GetLocation(), WorldPosition);

				if (D < BestDist)
				{
					Alpha = LocalAlpha;
					BestDist = D;
				}
			});
		}
		else
		{
			for (const TSharedPtr<PCGExPaths::FPolyPath>& Path : TypedFilterFactory->PolyPaths)
			{
				float LocalAlpha = 0;
				(void)Path->GetClosestTransform(WorldPosition, LocalAlpha, false);

				switch (TypedFilterFactory->Config.TimeConsolidation)
				{
				case EPCGExSplineTimeConsolidation::Min: Alpha = FMath::Min(LocalAlpha, Alpha);
					break;
				case EPCGExSplineTimeConsolidation::Max: Alpha = FMath::Max(LocalAlpha, Alpha);
					break;
				case EPCGExSplineTimeConsolidation::Average: Alpha += LocalAlpha;
					break;
				}
			}

			if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Average)
			{
				Alpha /= TypedFilterFactory->PolyPaths.Num();
			}
		}

		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, Alpha, TypedFilterFactory->Config.OperandBConstant, TypedFilterFactory->Config.Tolerance);
	}

	bool FTimeFilter::Test(const int32 PointIndex) const
	{
		const FVector WorldPosition = InTransforms[PointIndex].GetLocation();
		float Alpha = 0;

		if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Min) { Alpha = MAX_flt; }

		if (TypedFilterFactory->Config.Pick == EPCGExSplineFilterPick::Closest)
		{
			double BestDist = MAX_dbl;

			TypedFilterFactory->Octree->FindElementsWithBoundsTest(FBoxCenterAndExtent(WorldPosition, FVector::OneVector), [&](const PCGExOctree::FItem& Item)
			{
				float LocalAlpha = 0;
				const TSharedPtr<PCGExPaths::FPolyPath> Path = (*(TypedFilterFactory->PolyPaths.GetData() + Item.Index));
				const FTransform Closest = Path->GetClosestTransform(WorldPosition, LocalAlpha, false);

				const double D = FVector::DistSquared(Closest.GetLocation(), WorldPosition);

				if (D < BestDist)
				{
					Alpha = LocalAlpha;
					BestDist = D;
				}
			});
		}
		else
		{
			for (const TSharedPtr<PCGExPaths::FPolyPath>& Path : TypedFilterFactory->PolyPaths)
			{
				float LocalAlpha = 0;
				(void)Path->GetClosestTransform(WorldPosition, LocalAlpha, false);

				switch (TypedFilterFactory->Config.TimeConsolidation)
				{
				case EPCGExSplineTimeConsolidation::Min: Alpha = FMath::Min(LocalAlpha, Alpha);
					break;
				case EPCGExSplineTimeConsolidation::Max: Alpha = FMath::Max(LocalAlpha, Alpha);
					break;
				case EPCGExSplineTimeConsolidation::Average: Alpha += LocalAlpha;
					break;
				}
			}

			if (TypedFilterFactory->Config.TimeConsolidation == EPCGExSplineTimeConsolidation::Average)
			{
				Alpha /= TypedFilterFactory->PolyPaths.Num();
			}
		}

		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, Alpha, TypedFilterFactory->Config.OperandBConstant, TypedFilterFactory->Config.Tolerance);
	}

	bool FTimeFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
	{
		PCGExData::FProxyPoint ProxyPoint;
		IO->GetDataAsProxyPoint(ProxyPoint);
		return Test(ProxyPoint);
	}
}

TArray<FPCGPinProperties> UPCGExTimeFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExPathInclusion::DeclareInclusionPin(PinProperties);
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(Time)

#if WITH_EDITOR
FString UPCGExTimeFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = TEXT("Time ") + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
