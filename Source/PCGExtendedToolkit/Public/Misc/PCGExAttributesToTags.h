// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"
#include "Pickers/PCGExPickerFactoryProvider.h"


#include "PCGExAttributesToTags.generated.h"

UENUM()
enum class EPCGExAttributeToTagsAction : uint8
{
	AddTags   = 0 UMETA(DisplayName = "Add Tags", ToolTip="Add tags to the collection"),
	Attribute = 1 UMETA(DisplayName = "Create attribute", ToolTip="Output an attribute set with the tag values"),
};

UENUM()
enum class EPCGExAttributeToTagsResolution : uint8
{
	Self                   = 0 UMETA(DisplayName = "Self", ToolTip="Matches a single entry to each input collection, from itself"),
	EntryToCollection      = 1 UMETA(DisplayName = "Entry to Collection", ToolTip="Matches a Source entries to each input collection"),
	CollectionToCollection = 2 UMETA(DisplayName = "Collection to Collection", ToolTip="Matches a single entry per source to matching collection (requires the same number of collections in both pins)"),
};

UENUM()
enum class EPCGExCollectionEntrySelection : uint8
{
	FirstIndex  = 0 UMETA(DisplayName = "First Entry", ToolTip="Uses the first entry in the matching collection"),
	LastIndex   = 1 UMETA(DisplayName = "Last Entry", ToolTip="Uses the last entry in the matching collection"),
	RandomIndex = 2 UMETA(DisplayName = "Random Entry", ToolTip="Uses a random entry in the matching collection"),
	Picker      = 3 UMETA(DisplayName = "Picker", ToolTip="Uses pickers to select indices that will be turned into tags"),
	PickerFirst = 4 UMETA(DisplayName = "Picker (First)", ToolTip="Uses the first valid index using pickers"),
	PickerLast  = 5 UMETA(DisplayName = "Picker (Last)", ToolTip="Uses the last valid index using pickers")
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/attributes-to-tags"))
class UPCGExAttributesToTagsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AttributesToTags, "Attributes to Tags", "Use point attributes or set to tag the data.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite); }
	virtual bool HasDynamicPins() const override { return true; }
#endif

	virtual bool GetIsMainTransactional() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Action. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExAttributeToTagsAction Action = EPCGExAttributeToTagsAction::AddTags;

	/** Resolution mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExAttributeToTagsResolution Resolution = EPCGExAttributeToTagsResolution::Self;

	/** Selection mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Resolution != EPCGExAttributeToTagsResolution::EntryToCollection"))
	EPCGExCollectionEntrySelection Selection = EPCGExCollectionEntrySelection::FirstIndex;

	/** If enabled, prefix the attribute value with the attribute name  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bPrefixWithAttributeName = true;

	/** Attributes which value will be used as tags. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGAttributePropertyInputSelector> Attributes;

	/** A list of selectors separated by a comma, for easy overrides. Will be appended to the existing array.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString CommaSeparatedAttributeSelectors;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietTooManyCollectionsWarning = false;
};

struct FPCGExAttributesToTagsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExAttributesToTagsElement;
	TArray<TObjectPtr<const UPCGExPickerFactoryData>> PickerFactories;
	TArray<FPCGAttributePropertyInputSelector> Attributes;
	TArray<TSharedPtr<PCGExData::FFacade>> SourceDataFacades;
	TArray<FPCGExAttributeToTagDetails> Details;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExAttributesToTagsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(AttributesToTags)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExAttributesToTags
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExAttributesToTagsContext, UPCGExAttributesToTagsSettings>
	{
		UPCGParamData* OutputSet = nullptr;
		TArray<int32> PickedIndices;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		void Tag(const FPCGExAttributeToTagDetails& InDetails, const int32 Index) const;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		void TagWithPickers(const FPCGExAttributeToTagDetails& InDetails);
		virtual void Output() override;
	};
}
