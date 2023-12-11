// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGEx.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"

#include "PCGExDataBlending.generated.h"

UENUM(BlueprintType)
enum class EPCGExDataBlendingType : uint8
{
	None    = 0 UMETA(DisplayName = "None", ToolTip="TBD"),
	Average = 1 UMETA(DisplayName = "Average", ToolTip="Average values"),
	Weight  = 2 UMETA(DisplayName = "Weight", ToolTip="Translates to basic interpolation in most use cases."),
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
	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FDataBlendingOperationBase
	{
	public:
		virtual ~FDataBlendingOperationBase();

		void SetAttributeName(FName InName) { AttributeName = InName; }
		FName GetAttributeName() const { return AttributeName; }

		virtual void PrepareForData(UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData, FPCGAttributeAccessorKeysPoints* InPrimaryKeys, FPCGAttributeAccessorKeysPoints* InSecondaryKeys);

		virtual bool GetRequiresPreparation() const;
		virtual bool GetRequiresFinalization() const;

		virtual void PrepareOperation(const int32 WriteIndex) const;
		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Alpha = 0) const;
		virtual void FinalizeOperation(const int32 WriteIndex, double Alpha) const;

		virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Count) const = 0;
		virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const = 0;
		virtual void FinalizeRangeOperation(const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const = 0;

		virtual void ResetToDefault(int32 WriteIndex) const;
		virtual void ResetRangeToDefault(int32 StartIndex, int32 Count) const;

	protected:
		bool bInterpolationAllowed = true;
		FName AttributeName = NAME_None;
		FPCGMetadataAttributeBase* PrimaryBaseAttribute = nullptr;
		FPCGMetadataAttributeBase* SecondaryBaseAttribute = nullptr;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingOperation : public FDataBlendingOperationBase
	{
	public:
		virtual ~FDataBlendingOperation() override
		{
			PCGEX_DELETE(PrimaryAccessor)
			PCGEX_DELETE(SecondaryAccessor)
		}

		virtual void PrepareForData(
			UPCGPointData* InPrimaryData,
			const UPCGPointData* InSecondaryData,
			FPCGAttributeAccessorKeysPoints* InPrimaryKeys,
			FPCGAttributeAccessorKeysPoints* InSecondaryKeys) override
		{
			FDataBlendingOperationBase::PrepareForData(InPrimaryData, InSecondaryData, InPrimaryKeys, InSecondaryKeys);

			if (PrimaryAccessor)
			{
				if (SecondaryAccessor == PrimaryAccessor) { SecondaryAccessor = nullptr; }
			}

			delete PrimaryAccessor;
			PrimaryAccessor = new PCGEx::FAttributeAccessor<T>(InPrimaryData, PrimaryBaseAttribute, InPrimaryKeys);

			//TODO: Reuse first dataset if ==
			delete SecondaryAccessor;
			SecondaryAccessor = new PCGEx::FConstAttributeAccessor<T>(InSecondaryData, SecondaryBaseAttribute, InSecondaryKeys);
		}

#define PCGEX_TEMP_VALUES TArray<T> Values; Values.SetNum(Count); TArrayView<T> View(Values);

		virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Count) const override
		{
			PCGEX_TEMP_VALUES
			PrimaryAccessor->GetRange(View, StartIndex);
			PrepareValuesRangeOperation(View, StartIndex);
		}

		virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const override
		{
			PCGEX_TEMP_VALUES
			DoValuesRangeOperation(PrimaryReadIndex, SecondaryReadIndex, View, StartIndex, Alphas);
		}

		virtual void FinalizeRangeOperation(const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const override
		{
			PCGEX_TEMP_VALUES
			PrimaryAccessor->GetRange(View, StartIndex);
			FinalizeValuesRangeOperation(View, StartIndex, Alphas);
		}

#undef PCGEX_TEMP_VALUES

		virtual void PrepareValuesRangeOperation(TArrayView<T>& Values, const int32 StartIndex) const
		{
			for (int i = 0; i < Values.Num(); i++) { SinglePrepare(Values[i]); }
			PrimaryAccessor->SetRange(Values, StartIndex);
		}

		virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const int32 StartIndex, const TArrayView<double>& Alphas) const
		{
			if (bInterpolationAllowed)
			{
				const T A = PrimaryAccessor->Get(PrimaryReadIndex);
				const T B = SecondaryAccessor->Get(PrimaryReadIndex);
				for (int i = 0; i < Values.Num(); i++) { Values[i] = SingleOperation(A, B, Alphas[i]); }
				PrimaryAccessor->SetRange(Values, StartIndex);
			}
			else
			{
				const T A = PrimaryAccessor->Get(PrimaryReadIndex);
				for (int i = 0; i < Values.Num(); i++) { Values[i] = A; }
				PrimaryAccessor->SetRange(Values, StartIndex);
			}
		}

		virtual void FinalizeValuesRangeOperation(TArrayView<T>& Values, const int32 StartIndex, const TArrayView<double>& Alphas) const
		{
			if (bInterpolationAllowed)
			{
				for (int i = 0; i < Values.Num(); i++) { SingleFinalize(Values[i], Alphas[i]); }
				PrimaryAccessor->SetRange(Values, StartIndex);
			}
		}

		virtual void SinglePrepare(T& A) const
		{
		};

		virtual T SingleOperation(T A, T B, double Alpha) const = 0;

		virtual void SingleFinalize(T& A, double Alpha) const
		{
		};

		virtual void ResetToDefault(int32 WriteIndex) const override { ResetRangeToDefault(WriteIndex, 1); }

		virtual void ResetRangeToDefault(int32 StartIndex, int32 Count) const override
		{
			TArray<T> Defaults;
			Defaults.SetNum(Count);
			const T DefaultValue = PrimaryAccessor->GetDefaultValue();
			for (int i = 0; i < Count; i++) { Defaults[i] = DefaultValue; }
			PrimaryAccessor->SetRange(Defaults, StartIndex);
		}

		virtual T GetPrimaryValue(const int32 Index) const
		{
			if (T OutValue; PrimaryAccessor->Get(OutValue, Index)) { return OutValue; }
			return PrimaryAccessor->GetDefaultValue();
		}

		virtual T GetSecondaryValue(const int32 Index) const
		{
			if (T OutValue; SecondaryAccessor->Get(OutValue, Index)) { return OutValue; }
			return SecondaryAccessor->GetDefaultValue();
		}

	protected:
		PCGEx::FAttributeAccessorBase<T>* PrimaryAccessor = nullptr;
		PCGEx::FAttributeAccessorBase<T>* SecondaryAccessor = nullptr;
	};

#pragma region Add

	template <typename T, typename dummy = void>
	inline static T Add(const T& A, const T& B, const double& Alpha = 0) { return A + B; }

	template <typename dummy = void>
	inline static bool Add(const bool& A, const bool& B, const double& Alpha = 0) { return B ? true : A; }

	template <typename dummy = void>
	inline static FTransform Add(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			A.GetRotation() + B.GetRotation(),
			A.GetLocation() + B.GetLocation(),
			A.GetScale3D() + B.GetScale3D());
	}

	template <typename dummy = void>
	inline static FString Add(const FString& A, const FString& B, const double& Alpha = 0) { return A < B ? B : A; }

	template <typename dummy = void>
	inline static FName Add(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() < B.ToString() ? B : A; }

#pragma endregion

#pragma region Min

	template <typename T, typename dummy = void>
	inline static T Min(const T& A, const T& B, const double& Alpha = 0) { return FMath::Min(A, B); }

	template <typename dummy = void>
	inline static FVector2D Min(const FVector2D& A, const FVector2D& B, const double& Alpha = 0)
	{
		return FVector2D(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y));
	}

	template <typename dummy = void>
	inline static FVector Min(const FVector& A, const FVector& B, const double& Alpha = 0)
	{
		return FVector(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z));
	}

	template <typename dummy = void>
	inline static FVector4 Min(const FVector4& A, const FVector4& B, const double& Alpha = 0)
	{
		return FVector4(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z),
			FMath::Min(A.W, B.W));
	}

	template <typename dummy = void>
	inline static FRotator Min(const FRotator& A, const FRotator& B, const double& Alpha = 0)
	{
		return FRotator(
			FMath::Min(A.Pitch, B.Pitch),
			FMath::Min(A.Yaw, B.Yaw),
			FMath::Min(A.Roll, B.Roll));
	}

	template <typename dummy = void>
	inline static FQuat Min(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return Min(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename dummy = void>
	inline static FTransform Min(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Min(A.GetRotation(), B.GetRotation()),
			Min(A.GetLocation(), B.GetLocation()),
			Min(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename dummy = void>
	inline static FString Min(const FString& A, const FString& B, const double& Alpha = 0) { return A > B ? B : A; }

	template <typename dummy = void>
	inline static FName Min(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() > B.ToString() ? B : A; }

#pragma endregion

#pragma region Max

	template <typename T, typename dummy = void>
	inline static T Max(const T& A, const T& B, const double& Alpha = 0) { return FMath::Max(A, B); }

	template <typename dummy = void>
	inline static FVector2D Max(const FVector2D& A, const FVector2D& B, const double& Alpha = 0)
	{
		return FVector2D(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y));
	}

	template <typename dummy = void>
	inline static FVector Max(const FVector& A, const FVector& B, const double& Alpha = 0)
	{
		return FVector(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	template <typename dummy = void>
	inline static FVector4 Max(const FVector4& A, const FVector4& B, const double& Alpha = 0)
	{
		return FVector4(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z),
			FMath::Max(A.W, B.W));
	}

	template <typename dummy = void>
	inline static FRotator Max(const FRotator& A, const FRotator& B, const double& Alpha = 0)
	{
		return FRotator(
			FMath::Max(A.Pitch, B.Pitch),
			FMath::Max(A.Yaw, B.Yaw),
			FMath::Max(A.Roll, B.Roll));
	}

	template <typename dummy = void>
	inline static FQuat Max(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return Max(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename dummy = void>
	inline static FTransform Max(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Max(A.GetRotation(), B.GetRotation()),
			Max(A.GetLocation(), B.GetLocation()),
			Max(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename dummy = void>
	inline static FString Max(const FString& A, const FString& B, const double& Alpha = 0) { return A > B ? A : B; }

	template <typename dummy = void>
	inline static FName Max(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() > B.ToString() ? A : B; }

#pragma endregion

#pragma region Lerp

	template <typename T, typename dummy = void>
	inline static T Lerp(const T& A, const T& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }

	template <typename dummy = void>
	inline static FQuat Lerp(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return FQuat::Slerp(A, B, Alpha); }

	template <typename dummy = void>
	inline static FTransform Lerp(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Lerp(A.GetRotation(), B.GetRotation(), Alpha),
			Lerp(A.GetLocation(), B.GetLocation(), Alpha),
			Lerp(A.GetScale3D(), B.GetScale3D(), Alpha));
	}

	template <typename dummy = void>
	inline static FString Lerp(const FString& A, const FString& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

	template <typename dummy = void>
	inline static FName Lerp(const FName& A, const FName& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

#pragma endregion

#pragma region Divide

	template <typename T, typename dummy = void>
	inline static T Div(const T& A, const double Divider) { return A / Divider; }

	template <typename dummy = void>
	inline static bool Div(const bool& A, const double Divider) { return A; }

	template <typename dummy = void>
	inline static FRotator Div(const FRotator& A, const double Divider)
	{
		return FRotator(
			A.Pitch / Divider,
			A.Yaw / Divider,
			A.Roll / Divider);
	}

	template <typename dummy = void>
	inline static FTransform Div(const FTransform& A, const double Divider)
	{
		return FTransform(
			A.GetRotation() / Divider,
			A.GetLocation() / Divider,
			A.GetScale3D() / Divider);
	}

	template <typename dummy = void>
	inline static FString Div(const FString& A, const double Divider) { return A; }

	template <typename dummy = void>
	inline static FName Div(const FName& A, const double Divider) { return A; }

#pragma endregion

#pragma region Copy

	template <typename T>
	inline static T Copy(const T& A, const T& B, const double& Alpha = 0) { return B; }

#pragma endregion

#pragma region NoBlend

	template <typename T>
	inline static T NoBlend(const T& A, const T& B, const double& Alpha = 0) { return A; }

#pragma endregion
}
