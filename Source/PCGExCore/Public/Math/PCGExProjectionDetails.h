// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <vector>
#include "CoreMinimal.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExProjectionDetails.generated.h"

template <typename ValueType, typename ViewType>
class TPCGValueRange;

template <typename ElementType>
using TConstPCGValueRange = TPCGValueRange<const ElementType, TConstStridedView<ElementType>>;

namespace PCGExMath
{
	struct FBestFitPlane;
}

namespace PCGExData
{
	class FFacade;
	class FPointIO;
}

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

UENUM()
enum class EPCGExProjectionMethod : uint8
{
	Normal  = 0 UMETA(DisplayName = "Normal", ToolTip="Uses a normal to project on a plane."),
	BestFit = 1 UMETA(DisplayName = "Best Fit", ToolTip="Compute eigen values to find the best-fit plane"),
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExGeo2DProjectionDetails
{
	GENERATED_BODY()

	FPCGExGeo2DProjectionDetails();
	explicit FPCGExGeo2DProjectionDetails(const bool InSupportLocalNormal);

	UPROPERTY()
	bool bSupportLocalNormal = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExProjectionMethod Method = EPCGExProjectionMethod::Normal;

	/** Normal vector of the 2D projection plane. Defaults to Up for XY projection. Used as fallback when using invalid local normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Method == EPCGExProjectionMethod::Normal", EditConditionHides, ShowOnlyInnerProperties))
	FVector ProjectionNormal = FVector::UpVector;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Method == EPCGExProjectionMethod::Normal && bSupportLocalNormal", EditConditionHides))
	bool bLocalProjectionNormal = false;

	/** Local attribute to fetch projection normal from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Method == EPCGExProjectionMethod::Normal && bSupportLocalNormal && bLocalProjectionNormal", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalNormal;

	TSharedPtr<PCGExDetails::TSettingValue<FVector>> NormalGetter;
	FQuat ProjectionQuat = FQuat::Identity;

	bool Init(const TSharedPtr<PCGExData::FFacade>& PointDataFacade);
	bool Init(const TSharedPtr<PCGExData::FPointIO>& PointIO);
	bool Init(const UPCGData* InData);
	void Init(const PCGExMath::FBestFitPlane& InFitPlane);

	~FPCGExGeo2DProjectionDetails() = default;
	FQuat GetQuat(const int32 PointIndex) const;

	FTransform Project(const FTransform& InTransform, const int32 PointIndex) const;
	void ProjectInPlace(FTransform& InTransform, const int32 PointIndex) const;
	FVector Project(const FVector& InPosition, const int32 PointIndex) const;
	FVector Project(const FVector& InPosition) const;
	FVector ProjectFlat(const FVector& InPosition) const;
	FVector ProjectFlat(const FVector& InPosition, const int32 PointIndex) const;
	FTransform ProjectFlat(const FTransform& InTransform) const;
	FTransform ProjectFlat(const FTransform& InTransform, const int32 PointIndex) const;

	template <typename T>
	void ProjectFlat(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<T>& OutPositions) const;

	void Project(const TArray<FVector>& InPositions, TArray<FVector>& OutPositions) const;
	void Project(const TArrayView<FVector>& InPositions, TArray<FVector2D>& OutPositions) const;
	void Project(const TConstPCGValueRange<FTransform>& InTransforms, TArray<FVector2D>& OutPositions) const;
	void Project(const TArrayView<FVector>& InPositions, std::vector<double>& OutPositions) const;
	void Project(const TConstPCGValueRange<FTransform>& InTransforms, std::vector<double>& OutPositions) const;

	FTransform Restore(const FTransform& InTransform, const int32 PointIndex) const;
	void RestoreInPlace(FTransform& InTransform, const int32 PointIndex) const;
	
protected:
	FVector WorldUp = FVector::UpVector;
	FVector WorldFwd = FVector::ForwardVector;
};

extern template void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector2D>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector2D>& OutPositions) const;
extern template void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector>& OutPositions) const;
extern template void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector4>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector4>& OutPositions) const;
