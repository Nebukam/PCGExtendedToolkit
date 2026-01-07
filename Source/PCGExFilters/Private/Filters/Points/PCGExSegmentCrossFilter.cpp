// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExSegmentCrossFilter.h"


#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"


#define LOCTEXT_NAMESPACE "PCGExSegmentCrossFilterDefinition"
#define PCGEX_NAMESPACE PCGExSegmentCrossFilterDefinition

TSharedPtr<PCGExPointFilter::IFilter> UPCGExSegmentCrossFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FSegmentCrossFilter>(this);
}

FName UPCGExSegmentCrossFilterFactory::GetInputLabel() const
{
	return PCGExCommon::Labels::SourceTargetsLabel;
}

void UPCGExSegmentCrossFilterFactory::InitConfig_Internal()
{
	Super::InitConfig_Internal();

	Config.IntersectionSettings.Init();

	LocalFidelity = Config.Fidelity;
	LocalExpansion = Config.IntersectionSettings.Tolerance;
	LocalExpansionZ = -1;
	//LocalProjection = Config.ProjectionDetails;
	LocalSampleInputs = Config.SampleInputs;
	WindingMutation = EPCGExWindingMutation::Unchanged;
	bScaleTolerance = false;
	bIgnoreSelf = Config.bIgnoreSelf;
	bBuildEdgeOctree = true;
}

namespace PCGExPointFilter
{
	bool FSegmentCrossFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

		bClosedLoop = PCGExPaths::Helpers::GetClosedLoop(InPointDataFacade->Source->GetIn());
		LastIndex = InPointDataFacade->GetNum() - 1;

		InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();
		return true;
	}

	bool FSegmentCrossFilter::Test(const int32 PointIndex) const
	{
		int32 NextIndex = PointIndex;

		if (TypedFilterFactory->Config.Direction == EPCGExSegmentCrossWinding::ToNext)
		{
			NextIndex++;
			if (NextIndex > LastIndex)
			{
				if (!bClosedLoop) { return TypedFilterFactory->Config.bInvert; }
				NextIndex = 0;
			}
		}
		else
		{
			NextIndex--;
			if (NextIndex < 0)
			{
				if (!bClosedLoop) { return TypedFilterFactory->Config.bInvert; }
				NextIndex = LastIndex;
			}
		}


		const PCGExMath::FSegment Segment(InTransforms[PointIndex].GetLocation(), InTransforms[NextIndex].GetLocation(), Handler->Tolerance);
		const PCGExMath::FClosestPosition ClosestPosition = Handler->FindClosestIntersection(Segment, TypedFilterFactory->Config.IntersectionSettings, PointDataFacade->Source->GetIn());
		return TypedFilterFactory->Config.bInvert ? !ClosestPosition.bValid : ClosestPosition.bValid;
	}
}

TArray<FPCGPinProperties> UPCGExSegmentCrossFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExPathInclusion::DeclareInclusionPin(PinProperties);
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(SegmentCross)

#if WITH_EDITOR
FString UPCGExSegmentCrossFilterProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString();
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
