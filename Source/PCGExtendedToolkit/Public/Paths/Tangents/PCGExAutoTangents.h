// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExTangentsOperation.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExAutoTangents.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName="Auto")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAutoTangents : public UPCGExTangentsOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExAutoTangents* TypedOther = Cast<UPCGExAutoTangents>(Other))
		{
			Scale = TypedOther->Scale;
		}
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Tangents, meta=(PCG_Overridable))
	double Scale = 1;

	FORCEINLINE virtual void ProcessFirstPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		PCGExGeo::FApex Apex = PCGExGeo::FApex::FromStartOnly(
			InPoints[0].Transform.GetLocation(),
			InPoints[0].Transform.GetLocation());

		Apex.Scale(Scale);
		OutArrive = Apex.TowardStart;
		OutLeave = Apex.TowardEnd * -1;
	}

	FORCEINLINE virtual void ProcessLastPoint(const TArray<FPCGPoint>& InPoints, FVector& OutArrive, FVector& OutLeave) const override
	{
		const int32 Index = InPoints.Num() - 1;
		const int32 PrevIndex = Index - 1;
		PCGExGeo::FApex Apex = PCGExGeo::FApex::FromEndOnly(
			InPoints[PrevIndex].Transform.GetLocation(),
			InPoints[Index].Transform.GetLocation());

		Apex.Scale(Scale);
		OutArrive = Apex.TowardStart;
		OutLeave = Apex.TowardEnd * -1;
	}

	FORCEINLINE virtual void ProcessPoint(const TArray<FPCGPoint>& InPoints, const int32 Index, const int32 NextIndex, const int32 PrevIndex, FVector& OutArrive, FVector& OutLeave) const override
	{
		PCGExGeo::FApex Apex = PCGExGeo::FApex(
			InPoints[PrevIndex].Transform.GetLocation(),
			InPoints[NextIndex].Transform.GetLocation(),
			InPoints[Index].Transform.GetLocation());

		Apex.Scale(Scale);
		OutArrive = Apex.TowardStart;
		OutLeave = Apex.TowardEnd * -1;
	}

protected:
	virtual void ApplyOverrides() override
	{
		Super::ApplyOverrides();
		PCGEX_OVERRIDE_OP_PROPERTY(Scale, FName(TEXT("Tangents/Scale")), EPCGMetadataTypes::Double);
	}
};
