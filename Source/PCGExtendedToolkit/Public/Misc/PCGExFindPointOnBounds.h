﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataFilter.h"
#include "Data/PCGExPointIO.h"


#include "PCGExFindPointOnBounds.generated.h"

class FPCGExComputeIOBounds;

UENUM()
enum class EPCGExPointOnBoundsOutputMode : uint8
{
	Merged     = 0 UMETA(DisplayName = "Merged Points", Tooltip="..."),
	Individual = 1 UMETA(DisplayName = "Per-point dataset", Tooltip="..."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/find-point-on-bounds"))
class UPCGExFindPointOnBoundsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindPointOnBounds, "Find point on Bounds", "Find the closest point on the dataset bounds.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorMiscAdd); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
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

struct FPCGExFindPointOnBoundsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFindPointOnBoundsElement;

	FPCGExCarryOverDetails CarryOverDetails;

	TArray<int32> BestIndices;
	TSharedPtr<PCGExData::FPointIO> MergedOut;
	TSharedPtr<PCGEx::FAttributesInfos> MergedAttributesInfos;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExFindPointOnBoundsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FindPointOnBounds)

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

			PCGMetadataEntryKey InKey = IO->GetIn()->GetMetadataEntry(BestIndices[i]);
			PCGMetadataEntryKey OutKey = Target->GetOut()->GetMetadataEntry(i);
			UPCGMetadata* InMetadata = IO->GetIn()->Metadata;

			for (const PCGEx::FAttributeIdentity& Identity : InAttributesInfos.Identities)
			{
				PCGEx::ExecuteWithRightType(
					Identity.GetTypeId(), [&](auto DummyValue)
					{
						using T = decltype(DummyValue);
						const FPCGMetadataAttribute<T>* InAttribute = InMetadata->GetConstTypedAttribute<T>(Identity.Identifier);
						FPCGMetadataAttribute<T>* OutAttribute = PCGEx::TryGetMutableAttribute<T>(OutMetadata, Identity.Identifier);

						if (!OutAttribute)
						{
							OutAttribute = Target->FindOrCreateAttribute<T>(
								Identity.Identifier,
								InAttribute->GetValueFromItemKey(PCGDefaultValueKey),
								InAttribute->AllowsInterpolation());
						}

						if (!OutAttribute) { return; }

						OutAttribute->SetValue(OutKey, InAttribute->GetValueFromItemKey(InKey));
					});
			}
		}
	}

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExFindPointOnBoundsContext, UPCGExFindPointOnBoundsSettings>
	{
		mutable FRWLock BestIndexLock;

		double BestDistance = MAX_dbl;
		FVector SearchPosition = FVector::ZeroVector;
		FVector BestPosition = FVector::ZeroVector;
		int32 BestIndex = -1;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
