// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExDetails.h"

#include "PCGComponent.h"

namespace PCGExDetails
{
	TSharedPtr<FDistances> MakeDistances(const EPCGExDistance Source, const EPCGExDistance Target, const bool bOverlapIsZero)
	{
		if (Source == EPCGExDistance::None || Target == EPCGExDistance::None)
		{
			return MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
		}
		if (Source == EPCGExDistance::Center)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::SphereBounds)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::BoxBounds)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}

		return nullptr;
	}

	TSharedPtr<FDistances> MakeNoneDistances()
	{
		return MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
	}
}

TSharedPtr<PCGExDetails::FDistances> FPCGExDistanceDetails::MakeDistances() const
{
	return PCGExDetails::MakeDistances(Source, Target);
}

void FPCGExCollisionDetails::Init(const FPCGExContext* InContext)
{
	World = InContext->GetWorld();

	if (bIgnoreActors)
	{
		const TFunction<bool(const AActor*)> BoundsCheck = [](const AActor*) -> bool { return true; };
		const TFunction<bool(const AActor*)> SelfIgnoreCheck = [](const AActor*) -> bool { return true; };
		IgnoredActors = PCGActorSelector::FindActors(IgnoredActorSelector, InContext->GetComponent(), BoundsCheck, SelfIgnoreCheck);
	}

	if (bIgnoreSelf) { IgnoredActors.Add(InContext->GetComponent()->GetOwner()); }
}

void FPCGExCollisionDetails::Update(FCollisionQueryParams& InCollisionParams) const
{
	InCollisionParams.bTraceComplex = bTraceComplex;
	InCollisionParams.AddIgnoredActors(IgnoredActors);
}

bool FPCGExCollisionDetails::Linecast(const FVector& From, const FVector& To, FHitResult& HitResult) const
{
	FCollisionQueryParams CollisionParams;
	Update(CollisionParams);

	switch (CollisionType)
	{
	case EPCGExCollisionFilterType::Channel:
		return World->LineTraceSingleByChannel(HitResult, From, To, CollisionChannel, CollisionParams);
	case EPCGExCollisionFilterType::ObjectType:
		return World->LineTraceSingleByObjectType(HitResult, From, To, FCollisionObjectQueryParams(CollisionObjectType), CollisionParams);
	case EPCGExCollisionFilterType::Profile:
		return World->LineTraceSingleByProfile(HitResult, From, To, CollisionProfileName, CollisionParams);
	default:
		return false;
	}
}

namespace PCGEx
{
}
