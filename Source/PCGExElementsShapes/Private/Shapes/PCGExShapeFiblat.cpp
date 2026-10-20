// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapeFiblat.h"


#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExMTCommon.h"
#include "Data/PCGExData.h"
#include "Details/PCGExSettingsDetails.h"
#include "Paths/PCGExPath.h"

#define LOCTEXT_NAMESPACE "PCGExCreateBuilderFiblat"
#define PCGEX_NAMESPACE CreateBuilderFiblat

#pragma region FPCGExShapeFiblatBuilderBase

bool FPCGExShapeFiblatBuilderBase::PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade)
{
	if (!FPCGExShapeBuilderOperation::PrepareForSeeds(InContext, InSeedDataFacade))
	{
		return false;
	}

	switch (Config.PhiConstant)
	{
	case EPCGExFibPhiConstant::GoldenRatio:
		ConstantPhi = (FMath::Sqrt(5.0) - 1.0) * 0.5;
		break;
	case EPCGExFibPhiConstant::SqRootOfTwo:
		ConstantPhi = FMath::Sqrt(2.0);
		break;
	case EPCGExFibPhiConstant::Irrational:
		ConstantPhi = (9.0 + FMath::Sqrt(221.0)) * 0.1;
		break;
	case EPCGExFibPhiConstant::SqRootOfTthree:
		ConstantPhi = FMath::Sqrt(3.0);
		break;
	case EPCGExFibPhiConstant::Ln2:
		ConstantPhi = 0.69314718056;
		break;
	case EPCGExFibPhiConstant::Custom:
		if (!InitSettingValue(Phi, Config.Phi.GetValueSetting(), InSeedDataFacade))
		{
			return false;
		}
		break;
	default:
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Unsupported Phi constant."));
		return false;
	}

	return true;
}

void FPCGExShapeFiblatBuilderBase::PrepareLattice(const TSharedPtr<PCGExShapes::FFiblat>& InFiblat, const PCGExData::FConstPoint& Seed) const
{
	InFiblat->Phi = Config.PhiConstant == EPCGExFibPhiConstant::Custom ? Phi->Read(Seed.Index) : ConstantPhi;
	InFiblat->Epsilon = Config.Epsilon;
}

void FPCGExShapeFiblatBuilderBase::ComputeExtents(const TSharedPtr<PCGExShapes::FFiblat>& InFiblat, const double Area) const
{
	if (BaseConfig.BoundsSource == EPCGExShapeBoundsSource::Fit)
	{
		const double AreaPerPoint = InFiblat->NumPoints > 0 ? Area / InFiblat->NumPoints : Area;
		InFiblat->Extents = FVector(FMath::Sqrt(AreaPerPoint) * 0.5);
	}
	else
	{
		InFiblat->Extents = BaseConfig.DefaultExtents;
	}
}

int32 FPCGExShapeFiblatBuilderBase::CountFromSpacing(const double Area, const double Spacing)
{
	if (Spacing <= 0)
	{
		return 0;
	}

	const double Count = Area / (Spacing * Spacing);
	if (!(Count < static_cast<double>(MAX_int32)))
	{
		return 0;
	}

	return FMath::Max(1, static_cast<int32>(Count));
}

FQuat FPCGExShapeFiblatBuilderBase::LookAtRotation(const FVector& Dir, const EPCGExAxisAlign LookAtAxis)
{
	if (Dir.IsNearlyZero())
	{
		return FQuat::Identity;
	}

	return PCGExMath::MakeLookAtTransform(Dir, FVector::UpVector, LookAtAxis).GetRotation();
}

#pragma endregion

#pragma region FPCGExShapeFiblatBuilder

void FPCGExShapeFiblatBuilder::PrepareShape(const PCGExData::FConstPoint& Seed)
{
	PCGEX_MAKE_SHARED(Fiblat, PCGExShapes::FFiblat, Seed)

	Fiblat->ComputeFit(BaseConfig);
	PrepareLattice(Fiblat, Seed);

	if (Config.ResolutionMode == EPCGExResolutionMode::Distance)
	{
		const double Spacing = GetResolution(Seed) * 100;
		const double Radius = Fiblat->Fit.GetExtent().Length();

		// Estimate number of points based on surface area and desired spacing
		if (Spacing > 0)
		{
			const double SurfaceArea = 4.0 * PI * Radius * Radius;
			const double AreaPerPoint = Spacing * Spacing;
			Fiblat->NumPoints = FMath::Max(1, static_cast<int32>(SurfaceArea / AreaPerPoint));
		}
		else
		{
			Fiblat->NumPoints = 100; // fallback
		}
	}
	else
	{
		Fiblat->NumPoints = GetResolution(Seed);
	}

	// Approximate spacing from surface area distribution
	const FVector FitExtents = Fiblat->Fit.GetExtent();
	const double AvgRadius = (FitExtents.X + FitExtents.Y + FitExtents.Z) / 3.0;
	ComputeExtents(Fiblat, 4.0 * PI * AvgRadius * AvgRadius);

	ValidateShape(Fiblat);

	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Fiblat);
}

void FPCGExShapeFiblatBuilder::BuildShape(const TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, const bool bOwnsData)
{
	const TSharedPtr<PCGExShapes::FFiblat> Fiblat = StaticCastSharedPtr<PCGExShapes::FFiblat>(InShape);

	const FVector Extents = Fiblat->Fit.GetExtent();
	const FVector Center = Fiblat->Fit.GetCenter();
	const int32 Count = Fiblat->NumPoints;

	TPCGValueRange<FTransform> OutTransforms = Scope.Data->GetTransformValueRange(false);

	PCGExMT::ParallelOrSequential(
		Count,
		[&](const int32 i)
		{
			const FVector P = Center + Fiblat->ComputeSpherePoint(i, Count) * Extents;
			OutTransforms[Scope.Start + i] = FTransform(PCGExMath::MakeLookAtTransform(P - Center, FVector::UpVector, Config.LookAtAxis).GetRotation(), P, FVector::OneVector);
		});
}

#pragma endregion

#if WITH_EDITOR
void UPCGExCreateShapeFiblatSettings::PCGExApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_IF_VERSION_LOWER(1, 75, 11)
	{
		Config.ApplyDeprecation();
	}

	Super::PCGExApplyDeprecation(InOutNode);
}
#endif


PCGEX_SHAPE_BUILDER_BOILERPLATE(Fiblat)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
