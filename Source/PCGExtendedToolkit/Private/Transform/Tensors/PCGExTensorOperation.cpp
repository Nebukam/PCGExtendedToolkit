// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Transform/Tensors/PCGExTensorOperation.h"

#include "Paths/PCGExPaths.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"

void UPCGExTensorOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	//if (const UPCGExTensorOperation* TypedOther = Cast<UPCGExTensorOperation>(Other))	{	}
}

bool UPCGExTensorOperation::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	Factory = InFactory;
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorOperation::SampleAtPosition(const FVector& InPosition) const
{
	return PCGExTensor::FTensorSample{};
}

bool UPCGExTensorOperation::ComputeFactor(const FVector& InPosition, const FPCGPointRef& InEffector, double& OutFactor) const
{
	const FVector Center = InEffector.Point->Transform.GetLocation();
	const double RadiusSquared = InEffector.Point->Color.W;
	const double DistSquared = FVector::DistSquared(InPosition, Center);

	if (FVector::DistSquared(InPosition, Center) > RadiusSquared) { return false; }

	OutFactor = DistSquared / RadiusSquared;
	return true;
}

bool UPCGExTensorOperation::ComputeFactor(const FVector& InPosition, const FPCGSplineStruct& InEffector, const double Radius, FTransform& OutTransform, double& OutFactor) const
{
	OutTransform = PCGExPaths::GetClosestTransform(InEffector, InPosition, true);

	const FVector Scale = OutTransform.GetScale3D();

	const double RadiusSquared = FMath::Square(FVector2D(Scale.Y, Scale.Z).Length() * Radius);
	const double DistSquared = FVector::DistSquared(InPosition, OutTransform.GetLocation());

	if (DistSquared > RadiusSquared) { return false; }

	OutFactor = DistSquared / RadiusSquared;
	return true;
}

void UPCGExTensorPointOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	//if (const UPCGExTensorPointOperation* TypedOther = Cast<UPCGExTensorPointOperation>(Other))	{	}
}

bool UPCGExTensorPointOperation::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	Octree = &InFactory->GetOctree();
	return true;
}
