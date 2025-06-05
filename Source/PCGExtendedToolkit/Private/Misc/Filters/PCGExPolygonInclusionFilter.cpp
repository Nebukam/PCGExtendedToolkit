// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExPolygonInclusionFilter.h"

#include "GeomTools.h"
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

bool UPCGExPolygonInclusionFilterFactory::Prepare(FPCGExContext* InContext)
{
	if (!Super::Prepare(InContext)) { return false; }

	Polygons = MakeShared<TArray<TSharedPtr<TArray<FVector2D>>>>();
	Bounds = MakeShared<TArray<FBox>>();

	if (TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(PCGExPaths::SourcePathsLabel);
		!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGBasePointData* PathData = Cast<UPCGBasePointData>(TaggedData.Data);
			if (!PathData) { continue; }

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

			Bounds->Add(Box);
			Polygons->Add(Polygon);
		}
	}

	if (Polygons->IsEmpty())
	{
		if (!bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)")); }
		return false;
	}

	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExPolygonInclusionFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FPolygonInclusionFilter>(this);
}

void UPCGExPolygonInclusionFilterFactory::BeginDestroy()
{
	Bounds.Reset();
	Polygons.Reset();
	Super::BeginDestroy();
}

namespace PCGExPointFilter
{
	bool FPolygonInclusionFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }
		InTransforms = InPointDataFacade->GetIn()->GetConstTransformValueRange();
		return true;
	}

	bool FPolygonInclusionFilter::Test(const PCGExData::FProxyPoint& Point) const
	{
		FVector Pos = Point.Transform.GetLocation();
		Pos.Z = 0;

		const FVector2D Pos2D = FVector2D(Pos.X, Pos.Y);

		if (!TypedFilterFactory->Config.bUseMinInclusionCount && !TypedFilterFactory->Config.bUseMaxInclusionCount)
		{
			for (int i = 0; i < Polygons->Num(); i++)
			{
				if (!(Bounds->GetData() + i)->IsInside(Pos)) { continue; }
				if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + i)->Get())) { return !TypedFilterFactory->Config.bInvert; }
			}
		}
		else
		{
			int32 Inclusions = 0;

			if (TypedFilterFactory->Config.bUseMinInclusionCount && TypedFilterFactory->Config.bUseMaxInclusionCount)
			{
				for (int i = 0; i < Polygons->Num(); i++)
				{
					if (!(Bounds->GetData() + i)->IsInside(Pos)) { continue; }
					if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + i)->Get()))
					{
						Inclusions++;
						if (Inclusions > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
					}
				}

				return Inclusions > TypedFilterFactory->Config.MinInclusionCount ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
			}
			if (TypedFilterFactory->Config.bUseMaxInclusionCount)
			{
				for (int i = 0; i < Polygons->Num(); i++)
				{
					if (!(Bounds->GetData() + i)->IsInside(Pos)) { continue; }
					if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + i)->Get()))
					{
						Inclusions++;
						if (Inclusions > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
					}
				}

				return Inclusions > 0 ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
			}
			if (TypedFilterFactory->Config.bUseMinInclusionCount)
			{
				for (int i = 0; i < Polygons->Num(); i++)
				{
					if (!(Bounds->GetData() + i)->IsInside(Pos)) { continue; }
					if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + i)->Get())) { Inclusions++; }
				}

				return Inclusions > TypedFilterFactory->Config.MinInclusionCount ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
			}
		}


		return TypedFilterFactory->Config.bInvert;
	}

	bool FPolygonInclusionFilter::Test(const int32 PointIndex) const
	{
		FVector Pos = InTransforms[PointIndex].GetLocation();
		Pos.Z = 0;

		const FVector2D Pos2D = FVector2D(Pos.X, Pos.Y);

		if (!TypedFilterFactory->Config.bUseMinInclusionCount && !TypedFilterFactory->Config.bUseMaxInclusionCount)
		{
			for (int i = 0; i < Polygons->Num(); i++)
			{
				if (!(Bounds->GetData() + i)->IsInside(Pos)) { continue; }
				if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + i)->Get())) { return !TypedFilterFactory->Config.bInvert; }
			}
		}
		else
		{
			int32 Inclusions = 0;

			if (TypedFilterFactory->Config.bUseMinInclusionCount && TypedFilterFactory->Config.bUseMaxInclusionCount)
			{
				for (int i = 0; i < Polygons->Num(); i++)
				{
					if (!(Bounds->GetData() + i)->IsInside(Pos)) { continue; }
					if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + i)->Get()))
					{
						Inclusions++;
						if (Inclusions > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
					}
				}

				return Inclusions < TypedFilterFactory->Config.MinInclusionCount ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
			}
			if (TypedFilterFactory->Config.bUseMaxInclusionCount)
			{
				for (int i = 0; i < Polygons->Num(); i++)
				{
					if (!(Bounds->GetData() + i)->IsInside(Pos)) { continue; }
					if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + i)->Get()))
					{
						Inclusions++;
						if (Inclusions > TypedFilterFactory->Config.MaxInclusionCount) { return TypedFilterFactory->Config.bInvert; }
					}
				}

				return Inclusions > 0 ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
			}
			if (TypedFilterFactory->Config.bUseMinInclusionCount)
			{
				for (int i = 0; i < Polygons->Num(); i++)
				{
					if (!(Bounds->GetData() + i)->IsInside(Pos)) { continue; }
					if (FGeomTools2D::IsPointInPolygon(Pos2D, *(Polygons->GetData() + i)->Get())) { Inclusions++; }
				}

				return Inclusions > TypedFilterFactory->Config.MinInclusionCount ? !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
			}
		}

		return TypedFilterFactory->Config.bInvert;
	}
}

TArray<FPCGPinProperties> UPCGExPolygonInclusionFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExPaths::SourcePathsLabel, TEXT("Paths will be used for testing"), Required, {})
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
