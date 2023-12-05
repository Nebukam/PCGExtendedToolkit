// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGEx.h"

#include "PCGExDataBlending.generated.h"

UENUM(BlueprintType)
enum class EPCGExDataBlendingType : uint8
{
	None    = 0 UMETA(DisplayName = "None", ToolTip="TBD"),
	Average = 1 UMETA(DisplayName = "Average", ToolTip="Average values"),
	Weight  = 2 UMETA(DisplayName = "Weight", ToolTip="TBD"),
	Min     = 3 UMETA(DisplayName = "Min", ToolTip="TBD"),
	Max     = 4 UMETA(DisplayName = "Max", ToolTip="TBD"),
	Copy    = 5 UMETA(DisplayName = "Copy", ToolTip = "TBD"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPointPropertyBlendingOverrides
{
	GENERATED_BODY()

#pragma region Property overrides

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideDensity = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Density", EditCondition="bOverrideDensity"))
	EPCGExDataBlendingType DensityBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideBoundsMin = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMin", EditCondition="bOverrideBoundsMin"))
	EPCGExDataBlendingType BoundsMinBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideBoundsMax = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="BoundsMax", EditCondition="bOverrideBoundsMax"))
	EPCGExDataBlendingType BoundsMaxBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideColor = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Color", EditCondition="bOverrideColor"))
	EPCGExDataBlendingType ColorBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverridePosition = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Position", EditCondition="bOverridePosition"))
	EPCGExDataBlendingType PositionBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideRotation = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Rotation", EditCondition="bOverrideRotation"))
	EPCGExDataBlendingType RotationBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Scale", EditCondition="bOverrideScale"))
	EPCGExDataBlendingType ScaleBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideSteepness = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Steepness", EditCondition="bOverrideSteepness"))
	EPCGExDataBlendingType SteepnessBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bOverrideSeed = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName="Seed", EditCondition="bOverrideSeed"))
	EPCGExDataBlendingType SeedBlending = EPCGExDataBlendingType::Weight;

#pragma endregion
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBlendingSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExDataBlendingType DefaultBlending = EPCGExDataBlendingType::Weight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Setting)
	FPCGExPointPropertyBlendingOverrides PropertiesOverrides;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TMap<FName, EPCGExDataBlendingType> AttributesOverrides;
};


namespace PCGExDataBlending
{
	// Overloads > template<T> for performance in tight loops

#pragma region Add
	inline static bool AddBoolean(const bool& A, const bool& B, const double& Alpha = 0) { return B ? true : A; }
	inline static int32 AddInteger32(const int32& A, const int32& B, const double& Alpha = 0) { return A + B; }
	inline static int64 AddInteger64(const int64& A, const int64& B, const double& Alpha = 0) { return A + B; }
	inline static float AddFloat(const float& A, const float& B, const double& Alpha = 0) { return A + B; }
	inline static double AddDouble(const double& A, const double& B, const double& Alpha = 0) { return A + B; }
	inline static FVector2D AddVector2(const FVector2D& A, const FVector2D& B, const double& Alpha = 0) { return A + B; }
	inline static FVector AddVector(const FVector& A, const FVector& B, const double& Alpha = 0) { return A + B; }
	inline static FVector4 AddVector4(const FVector4& A, const FVector4& B, const double& Alpha = 0) { return A + B; }
	inline static FQuat AddQuaternion(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return A + B; }
	inline static FRotator AddRotator(const FRotator& A, const FRotator& B, const double& Alpha = 0) { return A + B; }

	inline static FTransform AddTransform(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			A.GetRotation() + B.GetRotation(),
			A.GetLocation() + B.GetLocation(),
			A.GetScale3D() + B.GetScale3D());
	}

	inline static FString AddString(const FString& A, const FString& B, const double& Alpha = 0) { return A < B ? B : A; }
	inline static FName AddName(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() < B.ToString() ? B : A; }

#pragma endregion

#pragma region Min

	inline static bool MinBoolean(const bool& A, const bool& B, const double& Alpha = 0) { return FMath::Min(A, B); }
	inline static int32 MinInteger32(const int32& A, const int32& B, const double& Alpha = 0) { return FMath::Min(A, B); }
	inline static int64 MinInteger64(const int64& A, const int64& B, const double& Alpha = 0) { return FMath::Min(A, B); }
	inline static float MinFloat(const float& A, const float& B, const double& Alpha = 0) { return FMath::Min(A, B); }
	inline static double MinDouble(const double& A, const double& B, const double& Alpha = 0) { return FMath::Min(A, B); }

	inline static FVector2D MinVector2(const FVector2D& A, const FVector2D& B, const double& Alpha = 0)
	{
		return FVector2D(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y));
	}

	inline static FVector MinVector(const FVector& A, const FVector& B, const double& Alpha = 0)
	{
		return FVector(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z));
	}

	inline static FVector4 MinVector4(const FVector4& A, const FVector4& B, const double& Alpha = 0)
	{
		return FVector4(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z),
			FMath::Min(A.W, B.W));
	}

	inline static FRotator MinRotator(const FRotator& A, const FRotator& B, const double& Alpha = 0)
	{
		return FRotator(
			FMath::Min(A.Pitch, B.Pitch),
			FMath::Min(A.Yaw, B.Yaw),
			FMath::Min(A.Roll, B.Roll));
	}

	inline static FQuat MinQuaternion(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return MinRotator(A.Rotator(), B.Rotator()).Quaternion(); }

	inline static FTransform MinTransform(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			MinQuaternion(A.GetRotation(), B.GetRotation()),
			MinVector(A.GetLocation(), B.GetLocation()),
			MinVector(A.GetScale3D(), B.GetScale3D()));
	}

	inline static FString MinString(const FString& A, const FString& B, const double& Alpha = 0) { return A > B ? B : A; }
	inline static FName MinName(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() > B.ToString() ? B : A; }

#pragma endregion

#pragma region Max

	inline static bool MaxBoolean(const bool& A, const bool& B, const double& Alpha = 0) { return FMath::Max(A, B); }
	inline static int32 MaxInteger32(const int32& A, const int32& B, const double& Alpha = 0) { return FMath::Max(A, B); }
	inline static int64 MaxInteger64(const int64& A, const int64& B, const double& Alpha = 0) { return FMath::Max(A, B); }
	inline static float MaxFloat(const float& A, const float& B, const double& Alpha = 0) { return FMath::Max(A, B); }
	inline static double MaxDouble(const double& A, const double& B, const double& Alpha = 0) { return FMath::Max(A, B); }

	inline static FVector2D MaxVector2(const FVector2D& A, const FVector2D& B, const double& Alpha = 0)
	{
		return FVector2D(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y));
	}

	inline static FVector MaxVector(const FVector& A, const FVector& B, const double& Alpha = 0)
	{
		return FVector(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	inline static FVector4 MaxVector4(const FVector4& A, const FVector4& B, const double& Alpha = 0)
	{
		return FVector4(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z),
			FMath::Max(A.W, B.W));
	}

	inline static FRotator MaxRotator(const FRotator& A, const FRotator& B, const double& Alpha = 0)
	{
		return FRotator(
			FMath::Max(A.Pitch, B.Pitch),
			FMath::Max(A.Yaw, B.Yaw),
			FMath::Max(A.Roll, B.Roll));
	}

	inline static FQuat MaxQuaternion(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return MaxRotator(A.Rotator(), B.Rotator()).Quaternion(); }

	inline static FTransform MaxTransform(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			MaxQuaternion(A.GetRotation(), B.GetRotation()),
			MaxVector(A.GetLocation(), B.GetLocation()),
			MaxVector(A.GetScale3D(), B.GetScale3D()));
	}

	inline static FString MaxString(const FString& A, const FString& B, const double& Alpha = 0) { return A > B ? A : B; }
	inline static FName MaxName(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() > B.ToString() ? A : B; }

#pragma endregion

#pragma region Lerp

	inline static bool LerpBoolean(const bool& A, const bool& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }
	inline static int32 LerpInteger32(const int32& A, const int32& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }
	inline static int64 LerpInteger64(const int64& A, const int64& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }
	inline static float LerpFloat(const float& A, const float& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }
	inline static double LerpDouble(const double& A, const double& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }
	inline static FVector2D LerpVector2(const FVector2D& A, const FVector2D& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }
	inline static FVector LerpVector(const FVector& A, const FVector& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }
	inline static FVector4 LerpVector4(const FVector4& A, const FVector4& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }
	inline static FQuat LerpQuaternion(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return FQuat::Slerp(A, B, Alpha); }
	inline static FRotator LerpRotator(const FRotator& A, const FRotator& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }

	inline static FTransform LerpTransform(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			LerpQuaternion(A.GetRotation(), B.GetRotation(), Alpha),
			LerpVector(A.GetLocation(), B.GetLocation(), Alpha),
			LerpVector(A.GetScale3D(), B.GetScale3D(), Alpha));
	}

	inline static FString LerpString(const FString& A, const FString& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }
	inline static FName LerpName(const FName& A, const FName& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

#pragma endregion

#pragma region Divide

	inline static bool DivBoolean(const bool& A, const double Divider) { return A; }
	inline static int32 DivInteger32(const int32& A, const double Divider) { return A / Divider; }
	inline static int64 DivInteger64(const int64& A, const double Divider) { return A / Divider; }
	inline static float DivFloat(const float& A, const double Divider) { return A / Divider; }
	inline static double DivDouble(const double& A, const double Divider) { return A / Divider; }
	inline static FVector2D DivVector2(const FVector2D& A, const double Divider) { return A / Divider; }
	inline static FVector DivVector(const FVector& A, const double Divider) { return A / Divider; }
	inline static FVector4 DivVector4(const FVector4& A, const double Divider) { return A / Divider; }
	inline static FQuat DivQuaternion(const FQuat& A, const double Divider) { return A / Divider; }

	inline static FRotator DivRotator(const FRotator& A, const double Divider)
	{
		return FRotator(
			A.Pitch / Divider,
			A.Yaw / Divider,
			A.Roll / Divider);
	}

	inline static FTransform DivTransform(const FTransform& A, const double Divider)
	{
		return FTransform(
			A.GetRotation() / Divider,
			A.GetLocation() / Divider,
			A.GetScale3D() / Divider);
	}

	inline static FString DivString(const FString& A, const double Divider) { return A; }
	inline static FName DivName(const FName& A, const double Divider) { return A; }

#pragma endregion

#pragma region Copy

	inline static bool CopyBoolean(const bool& A, const bool& B, const double& Alpha = 0) { return B; }
	inline static int32 CopyInteger32(const int32& A, const int32& B, const double& Alpha = 0) { return B; }
	inline static int64 CopyInteger64(const int64& A, const int64& B, const double& Alpha = 0) { return B; }
	inline static float CopyFloat(const float& A, const float& B, const double& Alpha = 0) { return B; }
	inline static double CopyDouble(const double& A, const double& B, const double& Alpha = 0) { return B; }
	inline static FVector2D CopyVector2(const FVector2D& A, const FVector2D& B, const double& Alpha = 0) { return B; }
	inline static FVector CopyVector(const FVector& A, const FVector& B, const double& Alpha = 0) { return B; }
	inline static FVector4 CopyVector4(const FVector4& A, const FVector4& B, const double& Alpha = 0) { return B; }
	inline static FQuat CopyQuaternion(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return B; }
	inline static FRotator CopyRotator(const FRotator& A, const FRotator& B, const double& Alpha = 0) { return B; }
	inline static FTransform CopyTransform(const FTransform& A, const FTransform& B, const double& Alpha = 0) { return B; }
	inline static FString CopyString(const FString& A, const FString& B, const double& Alpha = 0) { return B; }
	inline static FName CopyName(const FName& A, const FName& B, const double& Alpha = 0) { return B; }

#pragma endregion

#pragma region NoBlend

	inline static bool NoBlendBoolean(const bool& A, const bool& B, const double& Alpha = 0) { return A; }
	inline static int32 NoBlendInteger32(const int32& A, const int32& B, const double& Alpha = 0) { return A; }
	inline static int64 NoBlendInteger64(const int64& A, const int64& B, const double& Alpha = 0) { return A; }
	inline static float NoBlendFloat(const float& A, const float& B, const double& Alpha = 0) { return A; }
	inline static double NoBlendDouble(const double& A, const double& B, const double& Alpha = 0) { return A; }
	inline static FVector2D NoBlendVector2(const FVector2D& A, const FVector2D& B, const double& Alpha = 0) { return A; }
	inline static FVector NoBlendVector(const FVector& A, const FVector& B, const double& Alpha = 0) { return A; }
	inline static FVector4 NoBlendVector4(const FVector4& A, const FVector4& B, const double& Alpha = 0) { return A; }
	inline static FQuat NoBlendQuaternion(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return A; }
	inline static FRotator NoBlendRotator(const FRotator& A, const FRotator& B, const double& Alpha = 0) { return A; }
	inline static FTransform NoBlendTransform(const FTransform& A, const FTransform& B, const double& Alpha = 0) { return A; }
	inline static FString NoBlendString(const FString& A, const FString& B, const double& Alpha = 0) { return A; }
	inline static FName NoBlendName(const FName& A, const FName& B, const double& Alpha = 0) { return A; }

#pragma endregion
}
