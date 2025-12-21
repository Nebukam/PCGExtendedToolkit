// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"
#include "Details/PCGExSettingsMacros.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Misc/PCGExModularPartitionByValues.h"

#include "PCGExDetailsSplineMesh.generated.h"

struct FPCGExMeshCollectionEntry;

namespace PCGExData
{
	class FFacade;
}

struct FPCGExStaticMeshComponentDescriptor;

namespace PCGExPaths
{
	PCGEXTENDEDTOOLKIT_API void GetAxisForEntry(const FPCGExStaticMeshComponentDescriptor& InDescriptor, ESplineMeshAxis::Type& OutAxis, int32& OutC1, int32& OutC2, const EPCGExSplineMeshAxis Default = EPCGExSplineMeshAxis::X);

	struct PCGEXTENDEDTOOLKIT_API FSplineMeshSegment
	{
		FSplineMeshSegment()
		{
		}

		bool bSetMeshWithSettings = false;
		bool bSmoothInterpRollScale = true;
		bool bUseDegrees = true;
		FVector UpVector = FVector::UpVector;
		TSet<FName> Tags;

		ESplineMeshAxis::Type SplineMeshAxis = ESplineMeshAxis::Type::X;

		const FPCGExMeshCollectionEntry* MeshEntry = nullptr;
		int16 MaterialPick = -1;
		FSplineMeshParams Params;

		void ComputeUpVectorFromTangents();

		void ApplySettings(USplineMeshComponent* Component) const;

		bool ApplyMesh(USplineMeshComponent* Component) const;
	};
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSplineMeshMutationDetails
{
	GENERATED_BODY()

	FPCGExSplineMeshMutationDetails() = default;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPushStart = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bPushStart", EditConditionHides))
	EPCGExInputValueType StartPushInput = EPCGExInputValueType::Constant;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Amount (Attr)", EditCondition="bPushStart && StartPushInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector StartPushInputAttribute;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Amount", EditCondition="bPushStart && StartPushInput == EPCGExInputValueType::Constant", EditConditionHides))
	double StartPushConstant = 0.1;

	PCGEX_SETTING_VALUE_DECL(StartPush, double);

	/** If enabled, value will relative to the size of the segment */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Relative", EditCondition="bPushStart", EditConditionHides))
	bool bRelativeStart = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPushEnd = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bPushEnd", EditConditionHides))
	EPCGExInputValueType EndPushInput = EPCGExInputValueType::Constant;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Amount (Attr)", EditCondition="bPushEnd && EndPushInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EndPushInputAttribute;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Amount", EditCondition="bPushEnd && EndPushInput == EPCGExInputValueType::Constant", EditConditionHides))
	double EndPushConstant = 0.1;

	PCGEX_SETTING_VALUE_DECL(EndPush, double);

	/** If enabled, value will relative to the size of the segment */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Relative", EditCondition="bPushEnd", EditConditionHides))
	bool bRelativeEnd = true;

	bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
	void Mutate(const int32 PointIndex, PCGExPaths::FSplineMeshSegment& InSegment);

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> StartAmount;
	TSharedPtr<PCGExDetails::TSettingValue<double>> EndAmount;
};
