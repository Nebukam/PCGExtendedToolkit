// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Elements/PCGActorSelector.h"

#include "PCGExCollisionDetails.generated.h"

struct FPCGExContext;
struct FHitResult;
class UWorld;
class AActor;

UENUM()
enum class EPCGExCollisionFilterType : uint8
{
	Channel    = 0 UMETA(DisplayName = "Channel", ToolTip="Channel"),
	ObjectType = 1 UMETA(DisplayName = "Object Type", ToolTip="Object Type"),
	Profile    = 2 UMETA(DisplayName = "Profile", ToolTip="Profile"),
};

USTRUCT(BlueprintType)
struct PCGEXFOUNDATIONS_API FPCGExCollisionDetails
{
	GENERATED_BODY()

	FPCGExCollisionDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTraceComplex = false;

	/** Collision type to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCollisionFilterType CollisionType = EPCGExCollisionFilterType::Channel;

	/** Collision channel to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollisionType == EPCGExCollisionFilterType::Channel", EditConditionHides, Bitmask, BitmaskEnum="/Script/Engine.ECollisionChannel"))
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldDynamic;

	/** Collision object type to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollisionType == EPCGExCollisionFilterType::ObjectType", EditConditionHides, Bitmask, BitmaskEnum="/Script/Engine.EObjectTypeQuery"))
	int32 CollisionObjectType = ObjectTypeQuery1;

	/** Collision profile to check against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollisionType == EPCGExCollisionFilterType::Profile", EditConditionHides))
	FName CollisionProfileName = NAME_None;

	/** Ignore this graph' PCG content */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bIgnoreSelf = true;

	/** Ignore a procedural selection of actors */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bIgnoreActors = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bIgnoreActors"))
	FPCGActorSelectorSettings IgnoredActorSelector;

	TArray<AActor*> IgnoredActors;
	UWorld* World = nullptr;

	void Init(FPCGExContext* InContext);
	void Update(FCollisionQueryParams& InCollisionParams) const;
	bool Linecast(const FVector& From, const FVector& To, FHitResult& HitResult) const;
	bool Linecast(const FVector& From, const FVector& To) const;
	bool StrongLinecast(const FVector& From, const FVector& To) const;
	bool Linecast(const FVector& From, const FVector& To, bool bStrong) const;
};
