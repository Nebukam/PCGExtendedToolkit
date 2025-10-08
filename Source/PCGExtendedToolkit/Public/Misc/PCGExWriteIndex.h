﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataHelpers.h"

#include "PCGExWriteIndex.generated.h"

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="metadata/write-index"))
class UPCGExWriteIndexSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		WriteIndex, "Write Index", "Write the current point index to an attribute.",
		FName(GetDisplayName()));
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorMiscWrite); }
#endif

protected:
	virtual bool HasDynamicPins() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Whether to write the index of the point on the point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOutputPointIndex = true;

	/** The name of the attribute to write its index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOutputPointIndex"))
	FName OutputAttributeName = "CurrentIndex";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ One Minus", EditCondition="bOutputPointIndex", HideEditConditionToggle))
	bool bOneMinus = false;

	/** Whether to write the index as a normalized output value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Normalized", EditCondition="bOutputPointIndex", HideEditConditionToggle))
	bool bNormalizedEntryIndex = false;

	/** Whether to output the collection index. .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOutputCollectionIndex = false;

	/** The name of the attribute/tag to write the collection index to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Collection Index", EditCondition="bOutputCollectionIndex"))
	FName CollectionIndexAttributeName = "@Data.CollectionIndex";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Type", EditCondition="bOutputCollectionIndex", HideEditConditionToggle))
	EPCGExNumericOutput CollectionIndexOutputType = EPCGExNumericOutput::Int32;

	/** If enabled, output the collection index as a tag */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Output to tags", EditCondition="bOutputCollectionIndex", HideEditConditionToggle))
	bool bOutputCollectionIndexToTags = false;


	/** Whether to output the collection number of entries .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bOutputCollectionNumEntries = false;

	/** The name of the attribute/tag to write the collection num entries to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Num Entries", EditCondition="bOutputCollectionNumEntries"))
	FName NumEntriesAttributeName = "@Data.NumEntries";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Type", EditCondition="bOutputCollectionNumEntries", HideEditConditionToggle))
	EPCGExNumericOutput NumEntriesOutputType = EPCGExNumericOutput::Int32;

	/** If enabled, output the normalized collection num entries to the points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Normalized", EditCondition="bOutputCollectionNumEntries", HideEditConditionToggle))
	bool bNormalizeNumEntries = false;

	/** If enabled, output the collection num entries as a tag */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Output to tags", EditCondition="bOutputCollectionNumEntries", HideEditConditionToggle))
	bool bOutputNumEntriesToTags = false;


	/** Whether the created attributes allows interpolation or not.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAllowInterpolation = true;

	void TagPointIO(const TSharedPtr<PCGExData::FPointIO>& InPointIO, double MaxNumEntries) const;
	void TagData(const int32 Index, FPCGTaggedData& InTaggedData, double NumEntries, double MaxNumEntries) const;

	bool CollectionLevelOutputOnly() const;

	virtual bool IsInputless() const override { return CollectionLevelOutputOnly(); }

#if WITH_EDITOR
	virtual FString GetDisplayName() const;
#endif
};

struct FPCGExWriteIndexContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExWriteIndexElement;
	bool bCollectionLevelOutputOnly = false;
	double MaxNumEntries = 0;

	TArray<FPCGTaggedData> WorkingData;
	TArray<double> NumEntries;

	FPCGAttributeIdentifier EntryIndexIdentifier;
	FPCGAttributeIdentifier NumEntriesIdentifier;
	FPCGAttributeIdentifier CollectionIndexIdentifier;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExWriteIndexElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WriteIndex)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExWriteIndex
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExWriteIndexContext, UPCGExWriteIndexSettings>
	{
		double NumPoints = 0;
		double MaxIndex = 0;
		TSharedPtr<PCGExData::TBuffer<int32>> IntWriter;
		TSharedPtr<PCGExData::TBuffer<double>> DoubleWriter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;
	};
}
