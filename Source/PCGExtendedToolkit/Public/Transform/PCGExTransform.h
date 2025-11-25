// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "Engine/EngineTypes.h"

#include "PCGExMathBounds.h"
#include "Details/PCGExSettingsMacros.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExTransform.generated.h"

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

UENUM()
enum class EPCGExTransformMode : uint8
{
	Absolute = 0 UMETA(DisplayName = "Absolute", ToolTip="Absolute, ignores source transform."),
	Relative = 1 UMETA(DisplayName = "Relative", ToolTip="Relative to source transform."),
};

UENUM()
enum class EPCGExTransformAlphaUsage : uint8
{
	StartAndEnd   = 0 UMETA(DisplayName = "Start & End", Tooltip="First alpha is to be used as start % along the axis, and second alpha is the end % along that same axis."),
	StartAndSize  = 1 UMETA(DisplayName = "Start & Size", Tooltip="First alpha is to be used as start % along the axis, and second alpha is a % of the axis length, from first alpha."),
	CenterAndSize = 2 UMETA(DisplayName = "Center & Size", Tooltip="First alpha is to be used as center % along the axis, and second alpha is a % of the axis length, before and after the center.")
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttachmentRules
{
	GENERATED_BODY()

	FPCGExAttachmentRules() = default;
	explicit FPCGExAttachmentRules(EAttachmentRule InLoc, EAttachmentRule InRot = EAttachmentRule::KeepWorld, EAttachmentRule InScale = EAttachmentRule::KeepWorld);
	~FPCGExAttachmentRules() = default;

	/** The rule to apply to location when attaching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EAttachmentRule LocationRule = EAttachmentRule::KeepWorld;

	/** The rule to apply to rotation when attaching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EAttachmentRule RotationRule = EAttachmentRule::KeepWorld;

	/** The rule to apply to scale when attaching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EAttachmentRule ScaleRule = EAttachmentRule::KeepWorld;

	/** Whether to weld simulated bodies together when attaching */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bWeldSimulatedBodies = false;

	FAttachmentTransformRules GetRules() const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocket
{
	GENERATED_BODY()

	FPCGExSocket() = default;
	FPCGExSocket(const FName& InSocketName, const FVector& InRelativeLocation, const FRotator& InRelativeRotation, const FVector& InRelativeScale, FString InTag);
	FPCGExSocket(const FName& InSocketName, const FTransform& InRelativeTransform, const FString& InTag);
	~FPCGExSocket() = default;

	UPROPERTY(meta=(PCG_NotOverridable, EditCondition="false", EditConditionHides))
	bool bManaged = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FName SocketName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bManaged"))
	FString Tag = TEXT("");
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSocketFitDetails
{
	GENERATED_BODY()

	FPCGExSocketFitDetails() = default;

	/** Whether socket fit is enabled or not */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bEnabled = false;

	/** Type of Socket name input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bEnabled"))
	EPCGExInputValueType SocketNameInput = EPCGExInputValueType::Attribute;

	/** Attribute to read socket name from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Socket Name (Attr)", EditCondition="bEnabled && SocketNameInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName SocketNameAttribute = NAME_None;

	/** Socket name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Socket Name", EditCondition="bEnabled && SocketNameInput == EPCGExInputValueType::Constant", EditConditionHides))
	FName SocketName = NAME_None;

	PCGEX_SETTING_VALUE_DECL(SocketName, FName)

	/** Type of Socket name input */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//EPCGExInputValueType SocketTagInput = EPCGExInputValueType::Attribute;

	/** Attribute to read socket name from. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Socket Tag (Attr)", EditCondition="SocketTagInput != EPCGExInputValueType::Constant", EditConditionHides))
	//FName SocketTagttribute = NAME_None;

	/** Socket name */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Socket Tag", EditCondition="SocketTagInput == EPCGExInputValueType::Constant", EditConditionHides))
	//FString SocketTag = TEXT("");

	//PCGEX_SETTING_VALUE_GET(SocketTag, FString, SocketTagInput, SocketTagAttribute, SocketTag)

	bool Init(const TSharedPtr<PCGExData::FFacade>& InFacade);
	void Mutate(const int32 Index, const TArray<FPCGExSocket>& InSockets, FTransform& InOutTransform) const;

protected:
	bool bMutate = false;
	TSharedPtr<PCGExDetails::TSettingValue<FName>> SocketNameBuffer;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExUVW
{
	GENERATED_BODY()

	FPCGExUVW()
	{
	}

	explicit FPCGExUVW(const double DefaultW)
		: WConstant(DefaultW)
	{
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsReference = EPCGExPointBoundsSource::ScaledBounds;

	/** U Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType UInput = EPCGExInputValueType::Constant;

	/** U Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U (Attr)", EditCondition="UInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector UAttribute;

	/** U Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U", EditCondition="UInput == EPCGExInputValueType::Constant", EditConditionHides))
	double UConstant = 0;


	/** V Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType VInput = EPCGExInputValueType::Constant;

	/** V Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V (Attr)", EditCondition="VInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector VAttribute;

	/** V Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V", EditCondition="VInput == EPCGExInputValueType::Constant", EditConditionHides))
	double VConstant = 0;


	/** W Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType WInput = EPCGExInputValueType::Constant;

	/** W Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W (Attr)", EditCondition="WInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector WAttribute;

	/** W Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W", EditCondition="WInput == EPCGExInputValueType::Constant", EditConditionHides))
	double WConstant = 0;


	PCGEX_SETTING_VALUE_DECL(U, double)
	PCGEX_SETTING_VALUE_DECL(V, double)
	PCGEX_SETTING_VALUE_DECL(W, double)


	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade);

	// Without axis

	FVector GetUVW(const int32 PointIndex) const;

	FVector GetPosition(const int32 PointIndex) const;

	FVector GetPosition(const int32 PointIndex, FVector& OutOffset) const;

	// With axis

	FVector GetUVW(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

	FVector GetPosition(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

	FVector GetPosition(const int32 PointIndex, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> UGetter;
	TSharedPtr<PCGExDetails::TSettingValue<double>> VGetter;
	TSharedPtr<PCGExDetails::TSettingValue<double>> WGetter;

	const UPCGBasePointData* PointData = nullptr;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAxisDeformDetails
{
	GENERATED_BODY()

	FPCGExAxisDeformDetails() = default;
	FPCGExAxisDeformDetails(const FString InFirst, const FString InSecond, const double InFirstValue = 0, const double InSecondValue = 1);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExTransformAlphaUsage Usage = EPCGExTransformAlphaUsage::StartAndEnd;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSampleSource FirstAlphaInput = EPCGExSampleSource::Constant;

	/** Attribute to read start value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="First Alpha (Attr)", EditCondition="FirstAlphaInput != EPCGExSampleSource::Constant", EditConditionHides))
	FName FirstAlphaAttribute = FName("@Data.FirstAlpha");

	/** Constant start value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="First Alpha", EditCondition="FirstAlphaInput == EPCGExSampleSource::Constant", EditConditionHides))
	double FirstAlphaConstant = 0;

	PCGEX_SETTING_DATA_VALUE_DECL(FirstAlpha, double)
	PCGEX_SETTING_VALUE_DECL(FirstAlpha, double)

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSampleSource SecondAlphaInput = EPCGExSampleSource::Constant;

	/** Attribute to read end value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Second Alpha (Attr)", EditCondition="SecondAlphaInput != EPCGExSampleSource::Constant", EditConditionHides))
	FName SecondAlphaAttribute = FName("@Data.SecondAlpha");

	/** Constant end value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Second Alpha", EditCondition="SecondAlphaInput == EPCGExSampleSource::Constant", EditConditionHides))
	double SecondAlphaConstant = 1;

	PCGEX_SETTING_DATA_VALUE_DECL(SecondAlpha, double)
	PCGEX_SETTING_VALUE_DECL(SecondAlpha, double)

	bool Validate(FPCGExContext* InContext, const bool bSupportPoints = false) const;

	bool Init(FPCGExContext* InContext, const TArray<PCGExData::FTaggedData>& InTargets);
	bool Init(FPCGExContext* InContext, const FPCGExAxisDeformDetails& Parent, const TSharedRef<PCGExData::FFacade>& InDataFacade, const int32 InTargetIndex, const bool bSupportPoint = false);

	void GetAlphas(const int32 Index, double& OutFirst, double& OutSecond, const bool bSort = true) const;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> FirstValueGetter;
	TSharedPtr<PCGExDetails::TSettingValue<double>> SecondValueGetter;

	TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>> TargetsFirstValueGetter;
	TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>> TargetsSecondValueGetter;
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAxisTwistDetails
{
	GENERATED_BODY()

	// Start/End
	// or
	// Per-point angle
};

namespace PCGExTransform
{
	const FName SourceDeformersLabel = TEXT("Deformers");
	const FName SourceDeformersBoundsLabel = TEXT("Bounds");

	static void SanitizeBounds(FBox& InBox)
	{
		FVector Size = InBox.GetSize();
		for (int i = 0; i < 3; i++)
		{
			if (FMath::IsNaN(Size[i]) || FMath::IsNearlyZero(Size[i])) { InBox.Min[i] -= UE_SMALL_NUMBER; }
		}
	}

	PCGEXTENDEDTOOLKIT_API
	FBox GetBounds(const TArrayView<FVector> InPositions);

	PCGEXTENDEDTOOLKIT_API
	FBox GetBounds(const TConstPCGValueRange<FTransform>& InTransforms);

	template <EPCGExPointBoundsSource Source>
	static FBox GetBounds(const UPCGBasePointData* InPointData)
	{
		FBox Bounds = FBox(ForceInit);
		FTransform Transform;

		const int32 NumPoints = InPointData->GetNumPoints();
		for (int i = 0; i < NumPoints; i++)
		{
			PCGExData::FConstPoint Point(InPointData, i);
			if constexpr (Source == EPCGExPointBoundsSource::Center)
			{
				Bounds += Point.GetLocation();
			}
			else
			{
				Point.GetTransformNoScale(Transform);
				Bounds += PCGExMath::GetLocalBounds<Source>(Point).TransformBy(Transform);
			}
		}

		SanitizeBounds(Bounds);
		return Bounds;
	}

	PCGEXTENDEDTOOLKIT_API
	FBox GetBounds(const UPCGBasePointData* InPointData, const EPCGExPointBoundsSource Source);

	struct FPCGExConstantUVW
	{
		FPCGExConstantUVW()
		{
		}

		EPCGExPointBoundsSource BoundsReference = EPCGExPointBoundsSource::ScaledBounds;
		double U = 0;
		double V = 0;
		double W = 0;

		FORCEINLINE FVector GetUVW() const { return FVector(U, V, W); }

		FVector GetPosition(const PCGExData::FConstPoint& Point) const;

		FVector GetPosition(const PCGExData::FConstPoint& Point, FVector& OutOffset) const;

		// With axis

		FVector GetUVW(const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

		FVector GetPosition(const PCGExData::FConstPoint& Point, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

		FVector GetPosition(const PCGExData::FConstPoint& Point, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;
	};
}
