// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExCollisionDetails.h"

#include "CollisionQueryParams.h"
#include "GameFramework/Actor.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
//#include "Engine/StaticMesh.h"
//#include "Components/StaticMeshComponent.h"
//#include "StaticMeshResources.h"
#include "PCGComponent.h"
#include "Core/PCGExContext.h"

void FPCGExCollisionDetails::Init(FPCGExContext* InContext)
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
	case EPCGExCollisionFilterType::Channel: return World->LineTraceSingleByChannel(HitResult, From, To, CollisionChannel, CollisionParams);
	case EPCGExCollisionFilterType::ObjectType: return World->LineTraceSingleByObjectType(HitResult, From, To, FCollisionObjectQueryParams(CollisionObjectType), CollisionParams);
	case EPCGExCollisionFilterType::Profile: return World->LineTraceSingleByProfile(HitResult, From, To, CollisionProfileName, CollisionParams);
	default: return false;
	}
}

bool FPCGExCollisionDetails::Linecast(const FVector& From, const FVector& To) const
{
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	Update(CollisionParams);

	switch (CollisionType)
	{
	case EPCGExCollisionFilterType::Channel: return World->LineTraceSingleByChannel(HitResult, From, To, CollisionChannel, CollisionParams);
	case EPCGExCollisionFilterType::ObjectType: return World->LineTraceSingleByObjectType(HitResult, From, To, FCollisionObjectQueryParams(CollisionObjectType), CollisionParams);
	case EPCGExCollisionFilterType::Profile: return World->LineTraceSingleByProfile(HitResult, From, To, CollisionProfileName, CollisionParams);
	default: return false;
	}
}

bool FPCGExCollisionDetails::StrongLinecast(const FVector& From, const FVector& To) const
{
	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	Update(CollisionParams);

	switch (CollisionType)
	{
	case EPCGExCollisionFilterType::Channel:
		{
			if (!World->LineTraceSingleByChannel(HitResult, From, To, CollisionChannel, CollisionParams))
			{
				return World->LineTraceSingleByChannel(HitResult, To, From, CollisionChannel, CollisionParams);
			}
			return true;
		}
	case EPCGExCollisionFilterType::ObjectType:
		{
			if (!World->LineTraceSingleByObjectType(HitResult, From, To, FCollisionObjectQueryParams(CollisionObjectType), CollisionParams))
			{
				return World->LineTraceSingleByObjectType(HitResult, To, From, FCollisionObjectQueryParams(CollisionObjectType), CollisionParams);
			}
			return true;
		}
	case EPCGExCollisionFilterType::Profile:
		{
			if (!World->LineTraceSingleByProfile(HitResult, From, To, CollisionProfileName, CollisionParams))
			{
				return World->LineTraceSingleByProfile(HitResult, To, From, CollisionProfileName, CollisionParams);
			}
			return true;
		}
	default: return false;
	}
}

bool FPCGExCollisionDetails::Linecast(const FVector& From, const FVector& To, bool bStrong) const
{
	if (bStrong) { return StrongLinecast(From, To); }
	return Linecast(From, To);
}
