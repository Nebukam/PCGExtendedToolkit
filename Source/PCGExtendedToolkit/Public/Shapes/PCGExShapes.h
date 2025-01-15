// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFitting.h"
#include "Data/PCGExData.h"
#include "PCGExShapes.generated.h"

UENUM()
enum class EPCGExShapeOutputMode : uint8
{
	PerDataset = 0 UMETA(DisplayName = "Per Dataset", ToolTip="Merge all shapes into the original dataset"),
	PerSeed    = 1 UMETA(DisplayName = "Per Seed", ToolTip="Create a new output per shape seed point"),
};


UENUM()
enum class EPCGExShapeResolutionMode : uint8
{
	Absolute = 0 UMETA(DisplayName = "Absolute", ToolTip="Resolution is absolute."),
	Scaled   = 1 UMETA(DisplayName = "Scaled", ToolTip="Resolution is scaled by the seed' scale"),
};

UENUM()
enum class EPCGExShapePointLookAt : uint8
{
	None = 0 UMETA(DisplayName = "None", ToolTip="Point look at will be as per 'canon' shape definition"),
	Seed = 1 UMETA(DisplayName = "Seed", ToolTip="Look At Seed"),
};

UENUM()
enum class EPCGExResolutionMode : uint8
{
	Distance = 0 UMETA(DisplayName = "Distance", ToolTip="Points-per-meter"),
	Fixed    = 1 UMETA(DisplayName = "Count", ToolTip="Fixed number of points"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShapeConfigBase
{
	GENERATED_BODY()

	FPCGExShapeConfigBase()
	{
	}

	virtual ~FPCGExShapeConfigBase()
	{
	}

	/** Resolution mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta = (PCG_NotOverridable))
	EPCGExResolutionMode ResolutionMode = EPCGExResolutionMode::Distance;

	/** Resolution input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta = (PCG_NotOverridable))
	EPCGExInputValueType ResolutionInput = EPCGExInputValueType::Constant;

	/** Resolution Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta=(PCG_Overridable, DisplayName="Resolution", EditCondition="ResolutionInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	double ResolutionConstant = 10;

	/** Resolution Attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta=(PCG_Overridable, DisplayName="Resolution", EditCondition="ResolutionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ResolutionAttribute;

	/** Fitting details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExFittingDetailsHandler Fitting;

	/** Axis on the source to remap to a target axis on the shape */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign SourceAxis = EPCGExAxisAlign::Forward;

	/** Shape axis to align to the source axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign TargetAxis = EPCGExAxisAlign::Forward;

	/** Points look at */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExShapePointLookAt PointsLookAt = EPCGExShapePointLookAt::None;

	/** Axis used to align the look at rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign LookAtAxis = EPCGExAxisAlign::Forward;


	/** Axis used to align the look at rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta=(PCG_Overridable))
	FVector DefaultExtents = FVector::OneVector * 0.5;

	/** Shape ID used to identify this specific shape' points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta=(PCG_Overridable))
	int32 ShapeId = 0;


	/** Don't output shape if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBelow = true;

	/** Discarded if point count is less than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta=(PCG_Overridable, EditCondition="bRemoveBelow", ClampMin=0))
	int32 MinPointCount = 2;

	/** Don't output shape if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveAbove = false;

	/** Discarded if point count is more than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta=(PCG_Overridable, EditCondition="bRemoveAbove", ClampMin=0))
	int32 MaxPointCount = 500;


	FTransform LocalTransform = FTransform::Identity;

	virtual void Init()
	{
		FQuat A = FQuat::Identity;
		FQuat B = FQuat::Identity;
		switch (SourceAxis)
		{
		case EPCGExAxisAlign::Forward:
			A = FRotationMatrix::MakeFromX(FVector::ForwardVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Backward:
			A = FRotationMatrix::MakeFromX(FVector::BackwardVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Right:
			A = FRotationMatrix::MakeFromX(FVector::RightVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Left:
			A = FRotationMatrix::MakeFromX(FVector::LeftVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Up:
			A = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat().Inverse();
			break;
		case EPCGExAxisAlign::Down:
			A = FRotationMatrix::MakeFromX(FVector::DownVector).ToQuat().Inverse();
			break;
		}

		switch (TargetAxis)
		{
		case EPCGExAxisAlign::Forward:
			B = FRotationMatrix::MakeFromX(FVector::ForwardVector).ToQuat();
			break;
		case EPCGExAxisAlign::Backward:
			B = FRotationMatrix::MakeFromX(FVector::BackwardVector).ToQuat();
			break;
		case EPCGExAxisAlign::Right:
			B = FRotationMatrix::MakeFromX(FVector::RightVector).ToQuat();
			break;
		case EPCGExAxisAlign::Left:
			B = FRotationMatrix::MakeFromX(FVector::LeftVector).ToQuat();
			break;
		case EPCGExAxisAlign::Up:
			B = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat();
			break;
		case EPCGExAxisAlign::Down:
			B = FRotationMatrix::MakeFromX(FVector::DownVector).ToQuat();
			break;
		}

		LocalTransform = FTransform(A * B, FVector::ZeroVector, FVector::OneVector);
	}
};

namespace PCGExShapes
{
	const FName OutputShapeBuilderLabel = TEXT("Shape Builder");
	const FName SourceShapeBuildersLabel = TEXT("Shape Builders");

	class FShape : public TSharedFromThis<FShape>
	{
	public:
		virtual ~FShape() = default;
		PCGExData::FPointRef Seed;

		int32 StartIndex = 0;
		int32 NumPoints = 0;
		int8 bValid = 1;

		FBox Fit = FBox(ForceInit);
		FVector Extents = FVector::OneVector * 0.5;

		bool IsValid() const { return bValid && Fit.IsValid && NumPoints > 0; }

		explicit FShape(const PCGExData::FPointRef& InPointRef)
			: Seed(InPointRef)
		{
		}

		virtual void ComputeFit(const FPCGExShapeConfigBase& Config)
		{
			Fit = FBox(FVector::OneVector * -0.5, FVector::OneVector * 0.5);
			FTransform OutTransform = FTransform::Identity;

			Config.Fitting.ComputeTransform<false>(Seed.Index, OutTransform, Fit);

			Fit = Fit.TransformBy(OutTransform);
			Fit = Fit.TransformBy(Config.LocalTransform);

			Extents = Config.DefaultExtents;
		}
	};
}
