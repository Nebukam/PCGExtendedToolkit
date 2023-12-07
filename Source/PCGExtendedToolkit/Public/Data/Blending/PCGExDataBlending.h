// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGEx.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"

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
	inline static FString LerpString(const FString& A, const FString& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

	template <typename dummy = void>
	inline static FName LerpName(const FName& A, const FName& B, const double& Alpha = 0) { return Alpha > 0.5 ? B : A; }

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

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FAttributeAccessor
	{
		FPCGMetadataAttribute<T>* Attribute = nullptr;
		int32 NumEntries = -1;
		TUniquePtr<FPCGAttributeAccessor<T>> Accessor;
		FPCGAttributeAccessorKeysPoints* InternalKeys = nullptr;
		FPCGAttributeAccessorKeysPoints* Keys = nullptr;

		FAttributeAccessor()
		{
		}

		FAttributeAccessor(UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute)
		{
			Init(InData, InAttribute);
		}

		FAttributeAccessor(UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
		{
			Init(InData, InAttribute, InKeys);
		}

		void Init(UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute)
		{
			Flush();
			Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			Accessor = MakeUnique<FPCGAttributeAccessor<T>>(Attribute, InData->Metadata);

			const TArrayView<FPCGPoint> View(InData->GetMutablePoints());
			NumEntries = View.Num();
			InternalKeys = new FPCGAttributeAccessorKeysPoints(View);

			Keys = InternalKeys;
		}

		void Init(UPCGPointData* InData, FPCGMetadataAttributeBase* InAttribute, FPCGAttributeAccessorKeysPoints* InKeys)
		{
			Flush();
			Attribute = static_cast<FPCGMetadataAttribute<T>*>(InAttribute);
			Accessor = MakeUnique<FPCGAttributeAccessor<T>>(Attribute, InData->Metadata);
			NumEntries = InKeys->GetNum();
			Keys = InKeys;
		}

		T Get(const int32 Index)
		{
			if (T OutValue; Get(OutValue, Index)) { return OutValue; }
			return Attribute->DefaultValue;
		}

		bool Get(T& OutValue, int32 Index) const { return Accessor->Get(OutValue, Index, Keys, EPCGAttributeAccessorFlags::StrictType); }

		bool GetRange(TArrayView<T> OutValues, int32 Index) const { return Accessor->GetRange(OutValues, Index, Keys, EPCGAttributeAccessorFlags::StrictType); }

		bool GetRange(TArray<T>& OutValues, int32 Index = 0, int32 Count = -1) const
		{
			if (Count == -1) { Count = NumEntries - Index; }
			OutValues.SetNumUninitialized(Count, true);
			TArrayView<T> View(OutValues);
			return GetRange(View, Index);
		}

		bool Set(const T& InValue, int32 Index) { return Accessor->Set(InValue, Index, Keys, EPCGAttributeAccessorFlags::StrictType); }

		bool SetRange(TArrayView<const T> InValues, int32 Index) { return Accessor->SetRange(InValues, Index, Keys, EPCGAttributeAccessorFlags::StrictType); }

		bool SetRange(TArray<T>& InValues, int32 Index = 0) const
		{
			TArrayView<T> View(InValues);
			return SetRange(View, Index);
		}

		~FAttributeAccessor()
		{
			Flush();
		}

	protected:
		void Flush()
		{
			if (Accessor) { Accessor.Reset(); }
			if (InternalKeys) { delete InternalKeys; }
			Keys = nullptr;
			Attribute = nullptr;
		}
	};

	/**
	 * 
	 */
	class PCGEXTENDEDTOOLKIT_API FDataBlendingOperationBase
	{
	public:
		virtual ~FDataBlendingOperationBase();

		void SetAttributeName(FName InName) { AttributeName = InName; }
		FName GetAttributeName() const { return AttributeName; }

		virtual void PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData, FPCGAttributeAccessorKeysPoints* InPrimaryKeys, FPCGAttributeAccessorKeysPoints* InSecondaryKeys);

		virtual bool GetRequiresPreparation() const;
		virtual bool GetRequiresFinalization() const;

		virtual void PrepareOperation(const int32 WriteIndex) const;
		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const PCGMetadataEntryKey WriteIndex, const double Alpha = 0) const;
		virtual void FinalizeOperation(const int32 WriteIndex, double Alpha) const;
		virtual void ResetToDefault(int32 WriteIndex) const;

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
			if (PrimaryAccessor) { delete PrimaryAccessor; }
			if (SecondaryAccessor) { delete SecondaryAccessor; }
		}

		virtual void PrepareForData(const UPCGPointData* InPrimaryData, const UPCGPointData* InSecondaryData, FPCGAttributeAccessorKeysPoints* InPrimaryKeys, FPCGAttributeAccessorKeysPoints* InSecondaryKeys) override
		{
			FDataBlendingOperationBase::PrepareForData(InPrimaryData, InSecondaryData, InPrimaryKeys, InSecondaryKeys);

			if (PrimaryAccessor) { delete PrimaryAccessor; }
			PrimaryAccessor = new FAttributeAccessor<T>(InPrimaryData, static_cast<FPCGMetadataAttribute<T>*>(PrimaryBaseAttribute), InPrimaryKeys);

			//TODO: Reuse first dataset
			if (SecondaryAccessor) { delete SecondaryAccessor; }
			SecondaryAccessor = new FAttributeAccessor<T>(InSecondaryData, static_cast<FPCGMetadataAttribute<T>*>(SecondaryBaseAttribute), InSecondaryKeys);
		}

		virtual void ResetToDefault(int32 WriteIndex) const override { PrimaryAccessor.Set(PrimaryAccessor->Attribute->DefaultValue, WriteIndex); }

		virtual T GetPrimaryValue(const int32 Index) const
		{
			if (T OutValue; PrimaryAccessor.Get(OutValue, Index)) { return OutValue; }
			return PrimaryAccessor->Attribute->DefaultValue;
		}

		virtual T GetSecondaryValue(const int32 Index) const
		{
			if (T OutValue; SecondaryAccessor.Get(OutValue, Index)) { return OutValue; }
			return SecondaryAccessor->Attribute->DefaultValue;
		}

	protected:
		FAttributeAccessor<T>* PrimaryAccessor = nullptr;
		FAttributeAccessor<T>* SecondaryAccessor = nullptr;
	};
}
