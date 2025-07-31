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

	PCGEX_VALIDATE_NAME_C(InContext, Config.AngleAttribute)
	PCGEX_VALIDATE_NAME_C(InContext, Config.EdgeIndexAttribute)
	PCGEX_VALIDATE_NAME_C(InContext, Config.OnHullAttribute)
	PCGEX_VALIDATE_NAME_C(InContext, Config.EdgeAlphaAttribute)
	
	return true;
}

void FPCGExShapePolygonBuilder::PrepareShape(const PCGExData::FConstPoint& Seed) {

	PCGEX_MAKE_SHARED(Polygon, PCGExShapes::FPolygon, Seed);

	Polygon->ComputeFit(BaseConfig);
	Polygon->Radius = Polygon->Fit.GetExtent().Length();
	
	Polygon->NumVertices = NumVertices->Read(Seed.Index);
	Polygon->bHasSkeleton = HasSkeleton->Read(Seed.Index);
	Polygon->EdgeLength = 2.f * Polygon->Radius * FMath::Sin(PI / static_cast<float>(Polygon->NumVertices));
	Polygon->InRadius = 0.5 * Polygon->EdgeLength * (1.0 / FMath::Tan(PI / static_cast<float>(Polygon->NumVertices)));
	
	Polygon->bWriteAngle = Config.bWriteAngleAttribute;
	Polygon->AngleAttr = Config.AngleAttribute;

	Polygon->bWriteEdgeIndex = Config.bWriteEdgeIndexAttribute;
	Polygon->EdgeAttr = Config.EdgeIndexAttribute;

	Polygon->bWriteEdgeAlpha = Config.bWriteEdgeAlphaAttribute;
	Polygon->EdgeAlphaAttr = Config.EdgeAlphaAttribute;

	Polygon->HullAttr = Config.OnHullAttribute;
	Polygon->bWriteHullFlag = Config.bWriteHullAttribute;
	
	TArray<FVector> VertexPositions = {};

	const double Increment = TWO_PI / static_cast<double>(Polygon->NumVertices);
	switch (Config.PolygonOrientation) {
		case EPCGExPolygonFittingMethod::VertexForward:
			break;
		case EPCGExPolygonFittingMethod::EdgeForward:
			Polygon->Orientation = Increment * .5;
			break;
		case EPCGExPolygonFittingMethod::Custom:
			Polygon->Orientation = Config.CustomPolygonOrientation;
			break;
	}

	FVector Min = {MAX_FLT, MAX_FLT, 0};
	FVector Max = -Min;
	
	for (int32 i = 0; i < Polygon->NumVertices; i++) {
		const double Theta = Polygon->Orientation + Increment * i;
		const FVector Point = FVector(FMath::Cos(Theta), FMath::Sin(Theta), 0.f);
		Min = Min.ComponentMin(Point);
		Max = Max.ComponentMax(Point);
	}

	// This is only really required for polygons where n % 4 == 0, as they can be expanded to fit the corners of the shape
	FVector AbsMin = Min.GetAbs();

	// Get the lowest difference in X and Y and the unit circle
	float MinXScaleDiff = FMath::Min(1.0 - Max.X, 1.0 - AbsMin.X);
	float MinYScaleDiff = FMath::Min(1.0 - Max.Y, 1.0 - AbsMin.Y);

	// Get the lowest of those two
	float MinScaleDiff = FMath::Min(MinXScaleDiff, MinYScaleDiff);

	// Scale up by 1 / (1 - min diff) so if min diff is 0, this will be 1, but if greater, it will expand to fit
	if (!FMath::IsNearlyZero(MinScaleDiff)) {
		Polygon->ScaleAdjustment = 1.0;//1.0 / (1.0 - MinXScaleDiff);
		Polygon->EdgeLength *= Polygon->ScaleAdjustment;
		Polygon->InRadius *= Polygon->ScaleAdjustment;
		Polygon->Radius *= Polygon->ScaleAdjustment;
	}

	const bool UseDistance = Config.ResolutionMode == EPCGExResolutionMode::Distance;
	const float Res = GetResolution(Seed);
	Polygon->PointsPerEdge = UseDistance ? static_cast<int32>(FMath::Max(Polygon->EdgeLength / Res, 1)) : Res;
	
	if (Polygon->bHasSkeleton) {

		switch (Config.SkeletonConnectionMode) {
			case EPCGExPolygonSkeletonConnectionType::Vertex:
				Polygon->bConnectSkeletonToVertices = true;
				break;
			case EPCGExPolygonSkeletonConnectionType::Edge:
				Polygon->bConnectSkeletonToEdges = true;
				break;
			case EPCGExPolygonSkeletonConnectionType::Both:
				Polygon->bConnectSkeletonToVertices = true;
				Polygon->bConnectSkeletonToEdges = true;
				break;
		}
		
		if (Polygon->bConnectSkeletonToVertices) {
			Polygon->PointsPerSpoke = UseDistance ? FMath::Max(Polygon->Radius / Res, 1) : Res;
		}
		if (Polygon->bConnectSkeletonToEdges) {
			Polygon->PointsPerEdgeSpoke = UseDistance ? FMath::Max(Polygon->InRadius / Res, 1) : Res;
		}

	}
	

	Polygon->NumPoints = (Polygon->PointsPerEdge + Polygon->PointsPerSpoke + Polygon->PointsPerEdgeSpoke) * Polygon->NumVertices;
	
	ValidateShape(Polygon);
	Shapes[Seed.Index] = StaticCastSharedPtr<PCGExShapes::FShape>(Polygon);
}

void FPCGExShapePolygonBuilder::BuildShape(TSharedPtr<PCGExShapes::FShape> InShape, TSharedPtr<PCGExData::FFacade> InDataFacade, const PCGExData::FScope& Scope, const bool bIsolated) {

	const TSharedPtr<PCGExShapes::FPolygon> Polygon = StaticCastSharedPtr<PCGExShapes::FPolygon>(InShape);
	
	TSharedPtr<PCGExData::TBuffer<double>> AngleBuffer = nullptr;
	if (Polygon->bWriteAngle) {
		AngleBuffer = InDataFacade->GetWritable<double>(Polygon->AngleAttr, 0, true, PCGExData::EBufferInit::New);
	}

	TSharedPtr<PCGExData::TBuffer<int32>> EdgeIndexBuffer = nullptr;
	if (Polygon->bWriteEdgeIndex) {
		EdgeIndexBuffer = InDataFacade->GetWritable<int32>(Polygon->EdgeAttr, 0, true, PCGExData::EBufferInit::New);
	}

	TSharedPtr<PCGExData::TBuffer<double>> EdgeAlphaBuffer = nullptr;
	if (Polygon->bWriteEdgeAlpha) {
		EdgeAlphaBuffer = InDataFacade->GetWritable<double>(Polygon->EdgeAlphaAttr, 0, true, PCGExData::EBufferInit::New);
	}

	TSharedPtr<PCGExData::TBuffer<bool>> HullFlagBuffer = nullptr;
	if (Polygon->bWriteHullFlag) {
		HullFlagBuffer = InDataFacade->GetWritable<bool>(Polygon->HullAttr, false, false, PCGExData::EBufferInit::New);
	}
	
	const double Increment = TWO_PI / static_cast<double>(Polygon->NumVertices);
	const double Offset = Polygon->Orientation;
	FVector Target = FVector::ZeroVector;

	const FVector Extents = Polygon->Fit.GetExtent();
	const FVector Center = Polygon->Fit.GetCenter();

	TPCGValueRange<FTransform> OutTransforms = Scope.Data->GetTransformValueRange(false);

	int32 Index = Scope.Start;

	for (int32 i = 0; i < Polygon->NumVertices; i++) {

		const double StartTheta = Offset + Increment * i;
		const double EndTheta = StartTheta + Increment;
		
		const FVector Start = Center + FVector(Extents.X * FMath::Cos(StartTheta), Extents.Y * FMath::Sin(StartTheta), 0.f) * Polygon->ScaleAdjustment;
		const FVector End = Center + FVector(Extents.X * FMath::Cos(EndTheta), Extents.Y * FMath::Sin(EndTheta), 0.f) * Polygon->ScaleAdjustment;
		FVector Delta = (End - Start) / Polygon->PointsPerEdge;
		
		const double Degrees = FMath::RadiansToDegrees(StartTheta);
	
		// Outer edges
		for (int32 j = 0; j < Polygon->PointsPerEdge; j++) {
			const FVector Point = Start + Delta * j;
			if (Config.PointsLookAt == EPCGExShapePointLookAt::None) Target = End;

			AppendPoint(
				OutTransforms, Index, Point, Target, Config.LookAtAxis,
				Degrees, true, i, static_cast<double>(j) / static_cast<double>(Polygon->PointsPerEdge),
				AngleBuffer, EdgeIndexBuffer, EdgeAlphaBuffer, HullFlagBuffer
			);

			Index++;
		}

		// Centre -> Vertices
		Delta = (Start - Center) / Polygon->PointsPerSpoke;
		for (int32 j = 0; j < Polygon->PointsPerSpoke; j++) {
			FVector Point = Center + Delta * j;

			if (Config.PointsLookAt == EPCGExShapePointLookAt::None) Target = Start;

			AppendPoint(
				OutTransforms, Index, Point, Target, Config.LookAtAxis,
				Degrees, false, i, static_cast<double>(j) / static_cast<double>(Polygon->PointsPerSpoke),
				AngleBuffer, EdgeIndexBuffer, EdgeAlphaBuffer, HullFlagBuffer
			);

			Index++;
		}

		// Center -> Midpoints
		FVector MidPoint = Start + (End - Start) * .5;
		Delta = (MidPoint - Center) / Polygon->PointsPerEdgeSpoke;
		for (int32 j = 0; j < Polygon->PointsPerEdgeSpoke; j++) {
			FVector Point = Center + Delta * j;

			if (Config.PointsLookAt == EPCGExShapePointLookAt::None) Target = MidPoint;

			AppendPoint(
				OutTransforms, Index, Point, Target, Config.LookAtAxis,
				Degrees, false, i, static_cast<double>(j) / static_cast<double>(Polygon->PointsPerEdgeSpoke),
				AngleBuffer, EdgeIndexBuffer, EdgeAlphaBuffer, HullFlagBuffer
			);

			Index++;
		}
		
	}
	
}

void FPCGExShapePolygonBuilder::AppendPoint(
		const TPCGValueRange<FTransform>& OutTransforms,
		const int32 Index,
		const FVector& Point,
		const FVector& LookTarget,
		const EPCGExAxisAlign LookAtAxis,
		const double Angle,
		const bool IsOnHull,
		const int32 EdgeIndex,
		const double Alpha,
		const TSharedPtr<PCGExData::TBuffer<double>>& AngleBuffer,
		const TSharedPtr<PCGExData::TBuffer<int32>>& EdgeIndexBuffer,
		const TSharedPtr<PCGExData::TBuffer<double>>& EdgeAlphaBuffer,
		const TSharedPtr<PCGExData::TBuffer<bool>>& HullFlagBuffer
	) {

	OutTransforms[Index] = FTransform(
			PCGExMath::MakeLookAtTransform(Point - LookTarget, FVector::UpVector, LookAtAxis).GetRotation(),
			Point,
	FVector::OneVector
			);

	if (AngleBuffer) AngleBuffer->SetValue(Index, Angle); 
	if (HullFlagBuffer) HullFlagBuffer->SetValue(Index, IsOnHull); 
	if (EdgeIndexBuffer) EdgeIndexBuffer->SetValue(Index, EdgeIndex);
	if (EdgeAlphaBuffer) EdgeAlphaBuffer->SetValue(Index, Alpha );

}

TSharedPtr<FPCGExShapeBuilderOperation> UPCGExShapePolygonFactory::CreateOperation(FPCGExContext* InContext) const{ PCGEX_FACTORY_NEW_OPERATION(ShapePolygonBuilder) NewOperation->Config = Config; NewOperation->Config.Init(); NewOperation->BaseConfig = NewOperation->Config; NewOperation->Transform = NewOperation->Config.LocalTransform; return NewOperation; } UPCGExFactoryData* UPCGExCreateShapePolygonSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{ UPCGExShapePolygonFactory* NewFactory = InContext->ManagedObjects->New<UPCGExShapePolygonFactory>(); NewFactory->Config = Config; return Super::CreateFactory(InContext, NewFactory);}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE






