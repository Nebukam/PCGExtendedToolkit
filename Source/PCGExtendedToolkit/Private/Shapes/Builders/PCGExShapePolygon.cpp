#include "Shapes/Builders/PCGExShapePolygon.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExCreateBuilderPolygon"
#define PCGEX_NAMESPACE CreateBuilderPolygon;

bool FPCGExShapePolygonBuilder::PrepareForSeeds(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InSeedDataFacade) {

	if (!FPCGExShapeBuilderOperation::PrepareForSeeds(InContext, InSeedDataFacade)) {
		return false;
	}

	NumVertices = Config.GetValueSettingNumVertices();
	if (!NumVertices->Init(InSeedDataFacade)) return false;

	HasSkeleton = Config.GetValueSettingAddSkeleton();
	if (!HasSkeleton->Init(InSeedDataFacade)) return false;
	
	return true;
}

void FPCGExShapePolygonBuilder::PrepareShape(const PCGExData::FConstPoint& Seed) {

	PCGEX_MAKE_SHARED(Polygon, PCGExShapes::FPolygon, Seed);

	Polygon->ComputeFit(BaseConfig);
	Polygon->Radius = Polygon->Fit.GetExtent().Length();
	Polygon->NumVertices = NumVertices->Read(Seed.Index);
	Polygon->bHasSkeleton = HasSkeleton->Read(Seed.Index);
	Polygon->bClosedLoop = Config.bIsClosedLoop;
	Polygon->EdgeLength = 2.f * Polygon->Radius * FMath::Sin(PI / static_cast<float>(Polygon->NumVertices));
	
	if (Config.ResolutionMode == EPCGExResolutionMode::Distance) {
		Polygon->PointsPerEdge = FMath::Max(Polygon->EdgeLength / (GetResolution(Seed)), 1); 
	}
	else {
		Polygon->PointsPerEdge = GetResolution(Seed);
	}

	
	Polygon->NumPoints = Polygon->PointsPerEdge * Polygon->NumVertices;

	// UE_LOG(LogTemp, Warning, TEXT("Radius: %f / Resolution: %f / Edge Length: %f / Points per edge: %i / Num points: %i"), Polygon->Radius, GetResolution(Seed), Polygon->EdgeLength, Polygon->PointsPerEdge, Polygon->NumPoints)

	ValidateShape(Polygon);
	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Polygon);
}

void FPCGExShapePolygonBuilder::BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope) {

	const TSharedPtr<PCGExShapes::FPolygon> Polygon = StaticCastSharedPtr<PCGExShapes::FPolygon>(InShape);

	

	const double Increment = TWO_PI / static_cast<double>(Polygon->NumVertices);
	FVector Target = FVector::ZeroVector;

	const FVector Extents = Polygon->Fit.GetExtent();
	const FVector Center = Polygon->Fit.GetCenter();

	TPCGValueRange<FTransform> OutTransforms = Scope.Data->GetTransformValueRange(false);

	for (int32 i = 0; i < Polygon->NumVertices; i++) {

		const double StartTheta = Increment * i;
		const double EndTheta = StartTheta + Increment;

		const FVector Start = Center + FVector(Extents.X * FMath::Cos(StartTheta), Extents.Y * FMath::Sin(StartTheta), 0.f);
		const FVector End = Center + FVector(Extents.Y * FMath::Cos(EndTheta), Extents.Y * FMath::Sin(EndTheta), 0.f);
		FVector Delta = (End - Start) / Polygon->PointsPerEdge;
		
		
		for (int32 j = 0; j < Polygon->PointsPerEdge; j++) {

			const FVector Point = Start + Delta * j;

			
			if (Config.PointsLookAt == EPCGExShapePointLookAt::None) {
				Target = End; 
			}

			OutTransforms[Scope.Start + (i * Polygon->PointsPerEdge) + j] = FTransform(
			PCGExMath::MakeLookAtTransform(Point - Target, FVector::UpVector, Config.LookAtAxis).GetRotation(),
			Point,
	FVector::OneVector
			);
		}

	}

	if (Polygon->bClosedLoop && InDataFacade->GetNum(PCGExData::EIOSide::Out) == Scope.Count) {
		PCGExPaths::SetClosedLoop(InDataFacade->GetOut(), true);
	}

	
}

PCGEX_SHAPE_BUILDER_BOILERPLATE(Polygon)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE






