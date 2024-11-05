// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOrientOperation.h"
#include "PCGExOrientLookAtMode.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Orient Look At Mode")--E*/)
enum class EPCGExOrientLookAtMode : uint8
{
	NextPoint     = 0 UMETA(DisplayName = "Next Point", ToolTip="Look at next point in path"),
	PreviousPoint = 1 UMETA(DisplayName = "Previous Point", ToolTip="Look at previous point in path"),
	Direction     = 2 UMETA(DisplayName = "Direction", ToolTip="Use a local vector attribute as a direction to look at"),
	Position      = 3 UMETA(DisplayName = "Position", ToolTip="Use a local vector attribtue as a world position to look at"),
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Look At")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOrientLookAt : public UPCGExOrientOperation
{
	GENERATED_BODY()

public:
	/** Look at method */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOrientLookAtMode LookAt = EPCGExOrientLookAtMode::NextPoint;

	/** Vector attribute representing either a direction or world position, depending on selected mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="LookAt==EPCGExOrientLookAtMode::Direction || LookAt==EPCGExOrientLookAtMode::Position", EditConditionHides))
	FPCGAttributePropertyInputSelector LookAtAttribute;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExOrientLookAt* TypedOther = Cast<UPCGExOrientLookAt>(Other))
		{
			LookAt = TypedOther->LookAt;
			LookAtAttribute = TypedOther->LookAtAttribute;
		}
	}

	virtual bool PrepareForData(const TSharedRef<PCGExData::FFacade>& InDataFacade, const TSharedRef<PCGExPaths::FPath>& InPath) override
	{
		if (!Super::PrepareForData(InDataFacade, InPath)) { return false; }

		if (LookAt == EPCGExOrientLookAtMode::Direction || LookAt == EPCGExOrientLookAtMode::Position)
		{
			LookAtGetter = InDataFacade->GetScopedBroadcaster<FVector>(LookAtAttribute);
			if (!LookAtGetter)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("LookAt Attribute ({0}) is not valid."), FText::FromString(PCGEx::GetSelectorDisplayName(LookAtAttribute))));
				return false;
			}
		}

		return true;
	}

	virtual FTransform ComputeOrientation(const PCGExData::FPointRef& Point, const double DirectionMultiplier) const override
	{
		switch (LookAt)
		{
		default: ;
		case EPCGExOrientLookAtMode::NextPoint:
			return LookAtWorldPos(Point.Point->Transform, Path->GetPos(Point.Index + 1), DirectionMultiplier);
		case EPCGExOrientLookAtMode::PreviousPoint:
			return LookAtWorldPos(Point.Point->Transform, Path->GetPos(Point.Index - 1), DirectionMultiplier);
		case EPCGExOrientLookAtMode::Direction:
			return LookAtDirection(Point.Point->Transform, Point.Index, DirectionMultiplier);
		case EPCGExOrientLookAtMode::Position:
			return LookAtPosition(Point.Point->Transform, Point.Index, DirectionMultiplier);
		}
	}

	virtual FTransform LookAtWorldPos(FTransform InT, const FVector& WorldPos, const double DirectionMultiplier) const
	{
		FTransform OutT = InT;
		OutT.SetRotation(
			PCGExMath::MakeDirection(
				OrientAxis,
				(InT.GetLocation() - WorldPos).GetSafeNormal() * DirectionMultiplier,
				PCGExMath::GetDirection(UpAxis)));
		return OutT;
	}

	virtual FTransform LookAtDirection(FTransform InT, const int32 Index, const double DirectionMultiplier) const
	{
		FTransform OutT = InT;
		OutT.SetRotation(
			PCGExMath::MakeDirection(
				OrientAxis, LookAtGetter->Read(Index).GetSafeNormal() * DirectionMultiplier, PCGExMath::GetDirection(UpAxis)));
		return OutT;
	}

	virtual FTransform LookAtPosition(FTransform InT, const int32 Index, const double DirectionMultiplier) const
	{
		FTransform OutT = InT;
		const FVector Current = OutT.GetLocation();
		const FVector Position = LookAtGetter->Read(Index);
		OutT.SetRotation(
			PCGExMath::MakeDirection(
				OrientAxis, (Position - Current).GetSafeNormal() * DirectionMultiplier, PCGExMath::GetDirection(UpAxis)));
		return OutT;
	}

	virtual void Cleanup() override
	{
		LookAtGetter.Reset();
		Super::Cleanup();
	}

protected:
	TSharedPtr<PCGExData::TBuffer<FVector>> LookAtGetter;
};
