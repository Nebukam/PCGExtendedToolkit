// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExRandomRatioFilter.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Misc/ScopeLock.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

namespace PCGExRandomRatioFilter
{
	// Weights <= 0 stay 0 whatever the contrast, so pow(0, 0) never promotes an excluded element.
	FORCEINLINE double ShapeWeight(const double Weight, const double Contrast)
	{
		if (Weight <= 0.0)
		{
			return 0.0;
		}
		return Contrast == 1.0 ? Weight : FMath::Pow(Weight, Contrast);
	}
}

bool UPCGExRandomRatioFilterFactory::SupportsCollectionEvaluation() const
{
	return true;
}

void UPCGExRandomRatioFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.bWeighted)
	{
		Config.Weight.RegisterBufferDependencies(InContext, FacadePreloader);
	}
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExRandomRatioFilterFactory::CreateFilter() const
{
	PCGEX_MAKE_SHARED(Filter, PCGExPointFilter::FRandomRatioFilter, this)
	return Filter;
}

#if WITH_EDITOR
void UPCGExRandomRatioFilterProviderSettings::PCGExApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	PCGEX_IF_VERSION_LOWER(1, 73, 0)
	{
		Config.Random.RenamePins(this, InOutNode);
	}

	Super::PCGExApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);
}

void UPCGExRandomRatioFilterProviderSettings::PCGExApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_IF_VERSION_LOWER(1, 73, 0)
	{
		Config.Random.ApplyDeprecation();
	}

	Super::PCGExApplyDeprecation(InOutNode);
}
#endif

void PCGExPointFilter::FRandomRatioFilter::BuildCollectionPicks(const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	FScopeLock Lock(&CollectionPicksLock);
	if (bCollectionPicksBuilt.load(std::memory_order_relaxed))
	{
		return;
	}

	// One draw covers every dataset on the pin; seed and amount read the first dataset's @Data, weights read each dataset's.
	const FPCGExRandomRatioFilterConfig& Config = TypedFilterFactory->Config;
	const TSharedPtr<PCGExData::FPointIO> DrawSource = (*ParentCollection)[0];
	const int32 NumDatasets = ParentCollection->Num();

	TSet<int32> Picks;
	if (Config.bWeighted)
	{
		bool bValid = true;
		TArray<double> Weights;
		Weights.Reserve(NumDatasets);
		for (int32 i = 0; i < NumDatasets; ++i)
		{
			double Weight = Config.Weight.Constant;
			if (!Config.Weight.TryReadDataValue((*ParentCollection)[i], Weight, PCGEX_QUIET_HANDLING))
			{
				bValid = false;
			}
			Weights.Add(PCGExRandomRatioFilter::ShapeWeight(Weight, Config.WeightContrast));
		}
		bCollectionPicksValid = Config.Random.GetPicks(DrawSource->GetContext(), DrawSource->GetIn(), Weights, Picks, PCGEX_QUIET_HANDLING) && bValid;
	}
	else
	{
		bCollectionPicksValid = Config.Random.GetPicks(DrawSource->GetContext(), DrawSource->GetIn(), NumDatasets, Picks, PCGEX_QUIET_HANDLING);
	}
	CollectionPicks = MoveTemp(Picks);
	BuiltForCollection = ParentCollection.Get();

	bCollectionPicksBuilt.store(true, std::memory_order_release);
}

bool PCGExPointFilter::FRandomRatioFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade))
	{
		return false;
	}
	if (bWillBeUsedWithCollections)
	{
		return true;
	}

	const FPCGExRandomRatioFilterConfig& Config = TypedFilterFactory->Config;
	const int32 NumPoints = InPointDataFacade->GetNum();

	if (!Config.bWeighted)
	{
		Config.Random.GetPicks(InContext, InPointDataFacade->GetIn(), NumPoints, PointPicks);
		return true;
	}

	// Full (unscoped) read: every weight is needed before the first Test.
	TSharedPtr<PCGExDetails::TSettingValue<double>> WeightBuffer;
	if (!InitSettingValue(WeightBuffer, Config.Weight.GetValueSetting(PCGEX_QUIET_HANDLING), InPointDataFacade, false))
	{
		return false;
	}

	TArray<double> Weights;
	Weights.SetNumUninitialized(NumPoints);
	for (int32 i = 0; i < NumPoints; ++i)
	{
		Weights[i] = PCGExRandomRatioFilter::ShapeWeight(WeightBuffer->Read(i), Config.WeightContrast);
	}

	Config.Random.GetPicks(InContext, InPointDataFacade->GetIn(), Weights, PointPicks);
	return true;
}

bool PCGExPointFilter::FRandomRatioFilter::Test(const int32 PointIndex) const
{
	return TypedFilterFactory->Config.bInvertResult ? !PointPicks.Contains(PointIndex) : PointPicks.Contains(PointIndex);
}

bool PCGExPointFilter::FRandomRatioFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	if (!ParentCollection || ParentCollection->IsEmpty())
	{
		return false;
	}

	if (!bCollectionPicksBuilt.load(std::memory_order_acquire))
	{
		BuildCollectionPicks(ParentCollection);
	}

	ensureMsgf(BuiltForCollection == ParentCollection.Get(), TEXT("A Random Ratio filter instance must only test datasets from a single collection."));

	if (!bCollectionPicksValid)
	{
		PCGEX_QUIET_HANDLING_RET
	}

	const bool bPicked = CollectionPicks.Contains(IO->IOIndex);
	return TypedFilterFactory->Config.bInvertResult ? !bPicked : bPicked;
}

PCGEX_CREATE_FILTER_FACTORY(RandomRatio)

#if WITH_EDITOR
FString UPCGExRandomRatioFilterProviderSettings::GetDisplayName() const
{
	return PCGExCommon::FlagInvertLabel(Config.bWeighted ? TEXT("Random Ratio (Weighted)") : TEXT("Random Ratio"), Config.bInvertResult);
}
#endif


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
