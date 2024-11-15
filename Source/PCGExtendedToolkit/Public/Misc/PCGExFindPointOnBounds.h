// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#include "PCGExFindPointOnBounds.generated.h"

class FPCGExComputeIOBounds;

UENUM()
enum class EPCGExPointOnBoundsOutputMode : uint8
{
	Merged     = 0 UMETA(DisplayName = "Merged Points", Tooltip="..."),
	Individual = 1 UMETA(DisplayName = "Per-point dataset", Tooltip="..."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFindPointOnBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindPointOnBounds, "Find point on Bounds", "Find the closest point on the dataset bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Data output mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointOnBoundsOutputMode OutputMode = EPCGExPointOnBoundsOutputMode::Merged;

	/** UVW position of the target within bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector UVW = FVector::OneVector;

	/** Offset to apply to the closest point, away from the bounds center. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Offset = 1;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bQuietAttributeMismatchWarning = false;

private:
	friend class FPCGExFindPointOnBoundsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindPointOnBoundsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFindPointOnBoundsElement;

	FPCGExCarryOverDetails CarryOverDetails;

	TArray<int32> BestIndices;
	TSharedPtr<PCGExData::FPointIO> MergedOut;
	TSharedPtr<PCGEx::FAttributesInfos> MergedAttributesInfos;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindPointOnBoundsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExFindPointOnBounds
{
	static void MergeBestCandidatesAttributes(
		const TSharedPtr<PCGExData::FPointIO>& Target,
		const TArray<TSharedPtr<PCGExData::FPointIO>>& Collections,
		const TArray<int32>& BestIndices,
		const PCGEx::FAttributesInfos& InAttributesInfos)
	{
		UPCGMetadata* OutMetadata = Target->GetOut()->Metadata;

		for (int i = 0; i < BestIndices.Num(); i++)
		{
			const TSharedPtr<PCGExData::FPointIO> IO = Collections[i];

			if (BestIndices[i] == -1 || !IO) { continue; }

			PCGMetadataEntryKey InKey = IO->GetInPoint(BestIndices[i]).MetadataEntry;
			PCGMetadataEntryKey OutKey = Target->GetOutPoint(i).MetadataEntry;
			UPCGMetadata* InMetadata = IO->GetIn()->Metadata;
			for (const PCGEx::FAttributeIdentity& Identity : InAttributesInfos.Identities)
			{
				PCGMetadataAttribute::CallbackWithRightType(
					static_cast<uint16>(Identity.GetTypeId()), [&](auto DummyValue)
					{
						using T = decltype(DummyValue);
						const FPCGMetadataAttribute<T>* InAttribute = InMetadata->GetConstTypedAttribute<T>(Identity.Name);
						const FPCGMetadataAttributeBase* OutAttributeBase = OutMetadata->GetMutableAttribute(Identity.Name);
						FPCGMetadataAttribute<T>* OutAttribute = nullptr;

						if (!OutAttributeBase)
						{
							OutAttribute = OutMetadata->FindOrCreateAttribute<T>(
								Identity.Name,
								InAttribute->GetValueFromItemKey(PCGDefaultValueKey),
								InAttribute->AllowsInterpolation());
						}
						else
						{
							OutAttribute = OutMetadata->GetMutableTypedAttribute<T>(Identity.Name);
						}

						if (!OutAttribute) { return; }

						OutAttribute->SetValue(OutKey, InAttribute->GetValueFromItemKey(InKey));
					});
			}
		}
	}

	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExFindPointOnBoundsContext, UPCGExFindPointOnBoundsSettings>
	{
		mutable FRWLock BestIndexLock;

		FVector SearchPosition = FVector::ZeroVector;
		FVector BestPosition = FVector::ZeroVector;
		int32 BestIndex = -1;
		double BestDistance = MAX_dbl;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
	};
}
