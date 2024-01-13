// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExDataBlending.generated.h"

#define PCGEX_FOREACH_BLEND_POINTPROPERTY(MACRO)\
MACRO(float, Density, Float) \
MACRO(FVector, BoundsMin, Vector) \
MACRO(FVector, BoundsMax, Vector) \
MACRO(FVector4, Color, Vector4) \
MACRO(FVector, Position, Vector) \
MACRO(FQuat, Rotation, Quaternion) \
MACRO(FVector, Scale, Vector) \
MACRO(float, Steepness, Float) \
MACRO(int32, Seed, Integer32)

#define PCGEX_FOREACH_BLENDINIT_POINTPROPERTY(MACRO)\
MACRO(float, Density, Float, Density) \
MACRO(FVector, BoundsMin, Vector, BoundsMin) \
MACRO(FVector, BoundsMax, Vector, BoundsMax) \
MACRO(FVector4, Color, Vector4, Color) \
MACRO(FVector, Position, Vector,Transform.GetLocation()) \
MACRO(FQuat, Rotation, Quaternion,Transform.GetRotation()) \
MACRO(FVector, Scale, Vector, Transform.GetScale3D()) \
MACRO(float, Steepness, Float,Steepness) \
MACRO(int32, Seed, Integer32,Seed)

UENUM(BlueprintType)
enum class EPCGExDataBlendingType : uint8
{
	None    = 0 UMETA(DisplayName = "None", ToolTip="No blending is applied, keep the original value."),
	Average = 1 UMETA(DisplayName = "Average", ToolTip="Average all sampled values."),
	Weight  = 2 UMETA(DisplayName = "Weight", ToolTip="Translates to basic interpolation in most use cases."),
	Min     = 3 UMETA(DisplayName = "Min", ToolTip="Component-wise MIN operation"),
	Max     = 4 UMETA(DisplayName = "Max", ToolTip="Component-wise MAX operation"),
	Copy    = 5 UMETA(DisplayName = "Copy", ToolTip = "Copy incoming data"),
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

	FPCGExBlendingSettings()
	{
	}

	FPCGExBlendingSettings(const EPCGExDataBlendingType InDefaultBlending):
		DefaultBlending(InDefaultBlending)
	{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides._NAME##Blending = InDefaultBlending;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
	}

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

		void SetAttributeName(const FName InName) { AttributeName = InName; }
		FName GetAttributeName() const { return AttributeName; }

		virtual void PrepareForData(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, bool bSecondaryIn = true);

		virtual bool GetRequiresPreparation() const;
		virtual bool GetRequiresFinalization() const;

		virtual void PrepareOperation(const int32 WriteIndex) const;
		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Alpha = 0) const;
		virtual void FinalizeOperation(const int32 WriteIndex, double Alpha) const;

		virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Count) const = 0;
		virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const = 0;
		virtual void FinalizeRangeOperation(const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const = 0;

		virtual void FullBlendToOne(const TArrayView<double>& Alphas) const;

		virtual void ResetToDefault(int32 WriteIndex) const;
		virtual void ResetRangeToDefault(int32 StartIndex, int32 Count) const;

		virtual void Write() = 0;

	protected:
		bool bInterpolationAllowed = true;
		FName AttributeName = NAME_None;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingOperation : public FDataBlendingOperationBase
	{
	protected:
		void Cleanup()
		{
			if (Reader == Writer) { Reader = nullptr; }
			PCGEX_DELETE(Writer)
			PCGEX_DELETE(Reader)
		}

	public:
		virtual ~FDataBlendingOperation() override
		{
			Cleanup();
		}

		virtual void PrepareForData(PCGExData::FPointIO& InPrimaryData, const PCGExData::FPointIO& InSecondaryData, const bool bSecondaryIn) override
		{
			Cleanup();
			Writer = new PCGEx::TFAttributeWriter<T>(AttributeName);
			Writer->BindAndGet(InPrimaryData);

			if (&InPrimaryData == &InSecondaryData && !bSecondaryIn)
			{
				Reader = Writer;
			}
			else
			{
				Reader = new PCGEx::TFAttributeReader<T>(AttributeName);
				Reader->Bind(const_cast<PCGExData::FPointIO&>(InSecondaryData));
			}

			bInterpolationAllowed = Writer->GetAllowsInterpolation() && Reader->GetAllowsInterpolation();

			FDataBlendingOperationBase::PrepareForData(InPrimaryData, InSecondaryData);
		}

#define PCGEX_TEMP_VALUES TArrayView<T> View = MakeArrayView(Writer->Values.GetData() + StartIndex, Count);

		virtual void PrepareRangeOperation(const int32 StartIndex, const int32 Count) const override
		{
			PCGEX_TEMP_VALUES
			PrepareValuesRangeOperation(View, StartIndex);
		}

		virtual void DoRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const override
		{
			PCGEX_TEMP_VALUES
			DoValuesRangeOperation(PrimaryReadIndex, SecondaryReadIndex, View, Alphas);
		}

		virtual void FinalizeRangeOperation(const int32 StartIndex, const int32 Count, const TArrayView<double>& Alphas) const override
		{
			PCGEX_TEMP_VALUES
			FinalizeValuesRangeOperation(View, Alphas);
		}

#undef PCGEX_TEMP_VALUES

		virtual void PrepareValuesRangeOperation(TArrayView<T>& Values, const int32 StartIndex) const
		{
			for (int i = 0; i < Values.Num(); i++) { SinglePrepare(Values[i]); }
		}

		virtual void DoValuesRangeOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, TArrayView<T>& Values, const TArrayView<double>& Alphas) const
		{
			if (bInterpolationAllowed)
			{
				const T A = (*Writer)[PrimaryReadIndex];
				const T B = (*Reader)[SecondaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = SingleOperation(A, B, Alphas[i]); }
			}
			else
			{
				// Unecessary
				const T A = (*Writer)[PrimaryReadIndex];
				for (int i = 0; i < Values.Num(); i++) { Values[i] = A; }
			}
		}

		virtual void FinalizeValuesRangeOperation(TArrayView<T>& Values, const TArrayView<double>& Alphas) const
		{
			if (!bInterpolationAllowed) { return; }
			for (int i = 0; i < Values.Num(); i++) { SingleFinalize(Values[i], Alphas[i]); }
		}

		virtual void FullBlendToOne(const TArrayView<double>& Alphas) const override
		{
			if (!bInterpolationAllowed) { return; }
			for (int i = 0; i < Writer->Values.Num(); i++) { Writer->Values[i] = SingleOperation(Writer->Values[i], Reader->Values[i], Alphas[i]); }
		}

		virtual void SinglePrepare(T& A) const
		{
		};

		virtual T SingleOperation(T A, T B, double Alpha) const = 0;

		virtual void SingleFinalize(T& A, double Alpha) const
		{
		};

		virtual void ResetToDefault(const int32 WriteIndex) const override { ResetRangeToDefault(WriteIndex, 1); }

		virtual void ResetRangeToDefault(const int32 StartIndex, const int32 Count) const override
		{
			const T DefaultValue = Writer->GetDefaultValue();
			for (int i = 0; i < Count; i++) { Writer->Values[StartIndex + i] = DefaultValue; }
		}

		virtual T GetPrimaryValue(const int32 Index) const { return (*Writer)[Index]; }
		virtual T GetSecondaryValue(const int32 Index) const { return (*Reader)[Index]; }

		virtual void Write() override { Writer->Write(); }

	protected:
		PCGEx::TFAttributeWriter<T>* Writer = nullptr;
		PCGEx::FAttributeIOBase<T>* Reader = nullptr;
	};

#pragma region Add

	template <typename T, typename dummy = void>
	static T Add(const T& A, const T& B, const double& Alpha = 0) { return A + B; }

	template <typename dummy = void>
	static bool Add(const bool& A, const bool& B, const double& Alpha = 0) { return B ? true : A; }

	template <typename dummy = void>
	static FTransform Add(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			A.GetRotation() + B.GetRotation(),
			A.GetLocation() + B.GetLocation(),
			A.GetScale3D() + B.GetScale3D());
	}

	template <typename dummy = void>
	static FString Add(const FString& A, const FString& B, const double& Alpha = 0) { return A < B ? B : A; }

	template <typename dummy = void>
	static FName Add(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() < B.ToString() ? B : A; }

#pragma endregion

#pragma region Min

	template <typename T, typename dummy = void>
	static T Min(const T& A, const T& B, const double& Alpha = 0) { return FMath::Min(A, B); }

	template <typename dummy = void>
	static FVector2D Min(const FVector2D& A, const FVector2D& B, const double& Alpha = 0)
	{
		return FVector2D(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y));
	}

	template <typename dummy = void>
	static FVector Min(const FVector& A, const FVector& B, const double& Alpha = 0)
	{
		return FVector(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z));
	}

	template <typename dummy = void>
	static FVector4 Min(const FVector4& A, const FVector4& B, const double& Alpha = 0)
	{
		return FVector4(
			FMath::Min(A.X, B.X),
			FMath::Min(A.Y, B.Y),
			FMath::Min(A.Z, B.Z),
			FMath::Min(A.W, B.W));
	}

	template <typename dummy = void>
	static FRotator Min(const FRotator& A, const FRotator& B, const double& Alpha = 0)
	{
		return FRotator(
			FMath::Min(A.Pitch, B.Pitch),
			FMath::Min(A.Yaw, B.Yaw),
			FMath::Min(A.Roll, B.Roll));
	}

	template <typename dummy = void>
	static FQuat Min(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return Min(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename dummy = void>
	static FTransform Min(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Min(A.GetRotation(), B.GetRotation()),
			Min(A.GetLocation(), B.GetLocation()),
			Min(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename dummy = void>
	static FString Min(const FString& A, const FString& B, const double& Alpha = 0) { return A > B ? B : A; }

	template <typename dummy = void>
	static FName Min(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() > B.ToString() ? B : A; }

#pragma endregion

#pragma region Max

	template <typename T, typename dummy = void>
	static T Max(const T& A, const T& B, const double& Alpha = 0) { return FMath::Max(A, B); }

	template <typename dummy = void>
	static FVector2D Max(const FVector2D& A, const FVector2D& B, const double& Alpha = 0)
	{
		return FVector2D(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y));
	}

	template <typename dummy = void>
	static FVector Max(const FVector& A, const FVector& B, const double& Alpha = 0)
	{
		return FVector(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	template <typename dummy = void>
	static FVector4 Max(const FVector4& A, const FVector4& B, const double& Alpha = 0)
	{
		return FVector4(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z),
			FMath::Max(A.W, B.W));
	}

	template <typename dummy = void>
	static FRotator Max(const FRotator& A, const FRotator& B, const double& Alpha = 0)
	{
		return FRotator(
			FMath::Max(A.Pitch, B.Pitch),
			FMath::Max(A.Yaw, B.Yaw),
			FMath::Max(A.Roll, B.Roll));
	}

	template <typename dummy = void>
	static FQuat Max(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return Max(A.Rotator(), B.Rotator()).Quaternion(); }

	template <typename dummy = void>
	static FTransform Max(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Max(A.GetRotation(), B.GetRotation()),
			Max(A.GetLocation(), B.GetLocation()),
			Max(A.GetScale3D(), B.GetScale3D()));
	}

	template <typename dummy = void>
	static FString Max(const FString& A, const FString& B, const double& Alpha = 0) { return A > B ? A : B; }

	template <typename dummy = void>
	static FName Max(const FName& A, const FName& B, const double& Alpha = 0) { return A.ToString() > B.ToString() ? A : B; }

#pragma endregion

#pragma region Lerp

	template <typename T, typename dummy = void>
	static T Lerp(const T& A, const T& B, const double& Alpha = 0) { return FMath::Lerp(A, B, Alpha); }

	template <typename dummy = void>
	static FQuat Lerp(const FQuat& A, const FQuat& B, const double& Alpha = 0) { return FQuat::Slerp(A, B, Alpha); }

	template <typename dummy = void>
	static FTransform Lerp(const FTransform& A, const FTransform& B, const double& Alpha = 0)
	{
		return FTransform(
			Lerp(A.GetRotation(), B.GetRotation(), Alpha),
			Lerp(A.GetLocation(), B.GetLocation(), Alpha),
			Lerp(A.GetScale3D(), B.GetScale3D(), Alpha));
	}

	template <typename dummy = void>
	static FString Lerp(const FString& A, const FString& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

	template <typename dummy = void>
	static FName Lerp(const FName& A, const FName& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

#pragma endregion

#pragma region Divide

	template <typename T, typename dummy = void>
	static T Div(const T& A, const double Divider) { return A / Divider; }

	template <typename dummy = void>
	static bool Div(const bool& A, const double Divider) { return A; }

	template <typename dummy = void>
	static FRotator Div(const FRotator& A, const double Divider)
	{
		return FRotator(
			A.Pitch / Divider,
			A.Yaw / Divider,
			A.Roll / Divider);
	}

	template <typename dummy = void>
	static FQuat Div(const FQuat& A, const double Divider) { return Div(A.Rotator(), Divider).Quaternion(); }


	template <typename dummy = void>
	static FTransform Div(const FTransform& A, const double Divider)
	{
		return FTransform(
			Div(A.GetRotation(), Divider),
			A.GetLocation() / Divider,
			A.GetScale3D() / Divider);
	}

	template <typename dummy = void>
	static FString Div(const FString& A, const double Divider) { return A; }

	template <typename dummy = void>
	static FName Div(const FName& A, const double Divider) { return A; }

#pragma endregion

#pragma region Copy

	template <typename T>
	static T Copy(const T& A, const T& B, const double& Alpha = 0) { return B; }

#pragma endregion

#pragma region NoBlend

	template <typename T>
	static T NoBlend(const T& A, const T& B, const double& Alpha = 0) { return A; }

#pragma endregion
}
