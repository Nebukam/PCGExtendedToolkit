// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPolygonInclusionFilter.h"

#include "GeomTools.h"
#include "Data/PCGSplineData.h"


#include "Paths/PCGExPaths.h"


#define LOCTEXT_NAMESPACE "PCGExPolygonInclusionFilterDefinition"
#define PCGEX_NAMESPACE PCGExPolygonInclusionFilterDefinition

bool UPCGExPolygonInclusionFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	return true;
}

bool UPCGExPolygonInclusionFilterFactory::WantsPreparation(FPCGExContext* InContext)
{
	return true;
}

bool UPCGExPolygonInclusionFilterFactory::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	if (!Super::Prepare(InContext, AsyncManager)) { return false; }

	Polygons = MakeShared<TArray<TSharedPtr<TArray<FVector2D>>>>();

	if (TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(PCGExPaths::SourcePathsLabel);
		!Targets.IsEmpty())
	{
		FBox OctreeBounds = FBox(ForceInit);
		TArray<FBox> Boxes;
		TArray<FVector> SplinePoints;

		for (const FPCGTaggedData& TaggedData : Targets)
		{
			if (const UPCGBasePointData* PathData = Cast<UPCGBasePointData>(TaggedData.Data))
			{
				// Flatten points
				TConstPCGValueRange<FTransform> InTransforms = PathData->GetConstTransformValueRange();
				const TSharedPtr<TArray<FVector2D>> Polygon = MakeShared<TArray<FVector2D>>();

				FBox Box = FBox(FVector::OneVector * -1, FVector::OneVector);

				Polygon->SetNumUninitialized(InTransforms.Num());
				for (int i = 0; i < InTransforms.Num(); i++)
				{
					FVector Pos = InTransforms[i].GetLocation();
					Pos.Z = 0;
					Box += Pos;
					*(Polygon->GetData() + i) = FVector2D(Pos.X, Pos.Y);
				}

				OctreeBounds += Boxes.Add_GetRef(Box.ExpandBy(FVector::One()));
				Polygons->Add(Polygon);
			}
			else if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data))
			{
				// Flatten points
				SplineData->SplineStruct.ConvertSplineToPolyLine(ESplineCoordinateSpace::World, FMath::Square(Config.Fidelity), SplinePoints);
				const TSharedPtr<TArray<FVector2D>> Polygon = MakeShared<TArray<FVector2D>>();

				FBox Box = FBox(FVector::OneVector * -1, FVector::OneVector);

				Polygon->SetNumUninitialized(SplinePoints.Num());
				for (int i = 0; i < SplinePoints.Num(); i++)
				{
					FVector Pos = SplinePoints[i];
					Pos.Z = 0;
					Box += Pos;
					*(Polygon->GetData() + i) = FVector2D(Pos.X, Pos.Y);
				}

				OctreeBounds += Boxes.Add_GetRef(Box.ExpandBy(FVector::One()));
				Polygons->Add(Polygon);
			}
		}

		if (!Polygons->IsEmpty())
		{
			Octree = MakeShared<PCGEx::FIndexedItemOctree>(OctreeBounds.GetCenter(), OctreeBounds.GetExtent().Length());
			for (int i = 0; i < Polygons->Num(); i++) { Octree->AddElement(PCGEx::FIndexedItem(i, Boxes[i])); }
		}
	}

	if (Polygons->IsEmpty())
	{
		if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)")); }
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExPolygonInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FPolygonInclusionFilter>(this);
}

void UPCGExPolygonInclusionFilterFactory::BeginDestroy()
{
	Octree.Reset();
	Polygons.Reset();
	Super::BeginDestroy();
}

namespace PCGExPointFilter
{
	bool FPolygonInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }
		InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();

		bCheckAgainstDataBounds = TypedFilterFactory->Config.bCheckAgainstDataBounds;
		
		if (bCheckAgainstDataBounds)
		{
			PCGExData::FProxyPoint ProxyPoint;
			InPointDataFacade->Source->GetDataAsProxyPoint(ProxyPoint);
			bCollectionTestResult = Test(ProxyPoint);
		}

		return true;
	}

	bool FPolygonInclusionFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		FVector Pos = Point.Transform.GetLocation();
		Pos.Z = 0;

		const FBox Box = FBox(Pos - FVector::OneVector, Pos + FVector::OneVector);
		const FVector2D Pos2D = FVector2D(Pos.X, Pos.Y);

		if (!TypedFilterFactory->Config.bUseMinInclusionCount && !TypedFilterFactory->Config.bUseMaxInclusionCount)
		{
			bool bResult = TypedFilterFactory->Config.bInvert;
			Octree->FindFirstElementWithBoundsTest(
				Box,
				[&](const PCGEx::FIndexedItem& Item)
				{
					if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + Item.Index)->Get()))
					{
						bResult = !bResult;
						return false;
					}
					return true;
				});

			return bResult;
		}
		int32 Inclusions = 0;

		Octree->FindElementsWithBoundsTest(
			Box,
			[&](const PCGEx::FIndexedItem& Item)
			{
				if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + Item.Index)->Get())) { Inclusions++; }
			});

		if (TypedFilterFactory->Config.bUseMinInclusionCount && TypedFilterFactory->Config.bUseMaxInclusionCount)
		{
			if (Inclusions > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
			return Inclusions > TypedFilterFactory->Config.MinInclusionCount ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
		}
		if (TypedFilterFactory->Config.bUseMaxInclusionCount)
		{
			if (Inclusions > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
			return Inclusions > 0 ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
		}
		if (TypedFilterFactory->Config.bUseMinInclusionCount)
		{
			return Inclusions > TypedFilterFactory->Config.MinInclusionCount ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
		}


		return TypedFilterFactory->Config.bInvert;
	}

	bool FPolygonInclusionFilter::Test(const int32 PointIndex) const
	{
		if (bCheckAgainstDataBounds) { return bCollectionTestResult; }

		FVector Pos = InTransforms[PointIndex].GetLocation();
		Pos.Z = 0;

		const FBox Box = FBox(Pos - FVector::OneVector, Pos + FVector::OneVector);
		const FVector2D Pos2D = FVector2D(Pos.X, Pos.Y);

		if (!TypedFilterFactory->Config.bUseMinInclusionCount && !TypedFilterFactory->Config.bUseMaxInclusionCount)
		{
			bool bResult = TypedFilterFactory->Config.bInvert;
			Octree->FindFirstElementWithBoundsTest(
				Box,
				[&](const PCGEx::FIndexedItem& Item)
				{
					if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + Item.Index)->Get()))
					{
						bResult = !bResult;
						return false;
					}
					return true;
				});

			return bResult;
		}
		int32 Inclusions = 0;

		Octree->FindElementsWithBoundsTest(
			Box,
			[&](const PCGEx::FIndexedItem& Item)
			{
				if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + Item.Index)->Get())) { Inclusions++; }
			});

		if (TypedFilterFactory->Config.bUseMinInclusionCount && TypedFilterFactory->Config.bUseMaxInclusionCount)
		{
			if (Inclusions > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
			return Inclusions > TypedFilterFactory->Config.MinInclusionCount ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
		}
		if (TypedFilterFactory->Config.bUseMaxInclusionCount)
		{
			if (Inclusions > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
			return Inclusions > 0 ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
		}
		if (TypedFilterFactory->Config.bUseMinInclusionCount)
		{
			return Inclusions > TypedFilterFactory->Config.MinInclusionCount ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
		}

		return TypedFilterFactory->Config.bInvert;
	}

	bool FPolygonInclusionFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
	{
		PCGExData::FProxyPoint ProxyPoint;
		IO->GetDataAsProxyPoint(ProxyPoint);
		return Test(ProxyPoint);
	}
}

TArray<FPCGPinProperties> UPCGExPolygonInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExPaths::SourcePathsLabel, TEXT("Paths or splines that will be used for testing"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(PolygonInclusion)

#if WITH_EDITOR
FString UPCGExPolygonInclusionFilterProviderSettings::GetDisplayName() const
{
	return TEXT("Inside Polygon");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
