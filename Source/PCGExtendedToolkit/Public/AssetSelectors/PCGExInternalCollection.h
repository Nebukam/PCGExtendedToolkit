// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAssetCollection.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Engine/DataAsset.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGExInternalCollection.generated.h"

class UPCGExInternalCollection;

USTRUCT(NotBlueprintable, DisplayName="[PCGEx] Untyped Collection Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExInternalCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExInternalCollectionEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FSoftObjectPath Object;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UPCGExInternalCollection> SubCollection;

	TObjectPtr<UPCGExInternalCollection> SubCollectionPtr;

	bool SameAs(const FPCGExInternalCollectionEntry& Other) const
	{
		return
			SubCollectionPtr == Other.SubCollectionPtr &&
			Weight == Other.Weight &&
			Category == Other.Category &&
			Object == Other.Object;
	}

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive) override;

protected:
	virtual void OnSubCollectionLoaded() override;
};

UCLASS(NotBlueprintable, DisplayName="[PCGEx] Untyped Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExInternalCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExInternalCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
	virtual void RebuildStagingData(const bool bRecursive) override;

#if WITH_EDITOR
	virtual bool EDITOR_IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	FORCEINLINE virtual bool GetStaging(FPCGExAssetStagingData& OutStaging, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode) const override
	{
		return GetStagingTpl(OutStaging, Entries, Index, Seed, PickMode);
	}

	FORCEINLINE virtual bool GetStagingRandom(FPCGExAssetStagingData& OutStaging, const int32 Seed) const override
	{
		return GetStagingRandomTpl(OutStaging, Entries, Seed);
	}

	FORCEINLINE virtual bool GetStagingWeightedRandom(FPCGExAssetStagingData& OutStaging, const int32 Seed) const override
	{
		return GetStagingWeightedRandomTpl(OutStaging, Entries, Seed);
	}

	virtual void BuildCache() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExInternalCollectionEntry> Entries;
};

namespace PCGExAssetCollection
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
	const TSet<EPCGMetadataTypes> SupportedPathTypes = {
		EPCGMetadataTypes::SoftObjectPath,
		EPCGMetadataTypes::String,
		EPCGMetadataTypes::Name
	};
#else
	const TSet<EPCGMetadataTypes> SupportedPathTypes = {
		EPCGMetadataTypes::String,
		EPCGMetadataTypes::Name
	};
#endif


	const TSet<EPCGMetadataTypes> SupportedWeightTypes = {
		EPCGMetadataTypes::Float,
		EPCGMetadataTypes::Double,
		EPCGMetadataTypes::Integer32,
		EPCGMetadataTypes::Integer64,
	};

	const TSet<EPCGMetadataTypes> SupportedCategoryTypes = {
		EPCGMetadataTypes::String,
		EPCGMetadataTypes::Name
	};

	static UPCGExInternalCollection* GetCollectionFromAttributeSet(
		const FPCGContext* InContext,
		const UPCGParamData* InAttributeSet,
		const FPCGExAssetAttributeSetDetails& Details)
	{
		PCGEX_NEW_TRANSIENT(UPCGExInternalCollection, Collection)
		FPCGAttributeAccessorKeysEntries* Keys = nullptr;

		PCGEx::FAttributesInfos* Infos = nullptr;

		auto Cleanup = [&]()
		{
			PCGEX_DELETE(Infos)
			PCGEX_DELETE(Keys)
		};

		auto CreationFailed = [&]()
		{
			Cleanup();
			PCGEX_DELETE_UOBJECT(Collection)
			return nullptr;
		};

		const UPCGMetadata* Metadata = InAttributeSet->Metadata;

		Infos = PCGEx::FAttributesInfos::Get(Metadata);
		if (Infos->Attributes.IsEmpty()) { return CreationFailed(); }

		const PCGEx::FAttributeIdentity* PathIdentity = Infos->Find(Details.AssetPathSourceAttribute);
		const PCGEx::FAttributeIdentity* WeightIdentity = Infos->Find(Details.WeightSourceAttribute);
		const PCGEx::FAttributeIdentity* CategoryIdentity = Infos->Find(Details.CategorySourceAttribute);

		if (!PathIdentity || !SupportedPathTypes.Contains(PathIdentity->UnderlyingType))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Path attribute '{0}' is either of unsupported type or not in the metadata. Expecting SoftObjectPath/String/Name"), FText::FromName(Details.AssetPathSourceAttribute)));
			return CreationFailed();
		}

		if (WeightIdentity)
		{
			if (!SupportedWeightTypes.Contains(WeightIdentity->UnderlyingType))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Weight attribute '{0}' is of unsupported type. Expecting Float/Double/int32/int64"), FText::FromName(Details.WeightSourceAttribute)));
				WeightIdentity = nullptr;
			}
			else if (Details.WeightSourceAttribute.IsNone())
			{
				CategoryIdentity = nullptr;
			}
		}

		if (CategoryIdentity)
		{
			if (!SupportedCategoryTypes.Contains(CategoryIdentity->UnderlyingType))
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Category attribute '{0}' is of unsupported type. Expecting String/Name"), FText::FromName(Details.CategorySourceAttribute)));
				CategoryIdentity = nullptr;
			}
			else if (Details.CategorySourceAttribute.IsNone())
			{
				CategoryIdentity = nullptr;
			}
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		Keys = new FPCGAttributeAccessorKeysEntries(Metadata);
#else
		Keys = new FPCGAttributeAccessorKeysEntries(Infos->Attributes[0]); // Probably not reliable, but make 5.3 compile -_-
#endif

		const int32 NumEntries = Keys->GetNum();
		if (NumEntries == 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute set is empty."));
			return CreationFailed();
		}

		PCGEX_SET_NUM(Collection->Entries, NumEntries);

		// Path value

		auto SetEntryPath = [&](const int32 Index, const FSoftObjectPath& Path) { Collection->Entries[Index].Object = Path; };

#define PCGEX_FOREACH_COLLECTION_ENTRY(_TYPE, _NAME, _BODY)\
		TArray<_TYPE> V; PCGEX_SET_NUM(V, NumEntries)\
		FPCGAttributeAccessor<_TYPE>* A = new FPCGAttributeAccessor<_TYPE>(Metadata->GetConstTypedAttribute<_TYPE>(_NAME), Metadata);\
		A->GetRange(MakeArrayView(V), 0, *Keys);\
		for (int i = 0; i < NumEntries; i++) { _BODY }\
		PCGEX_DELETE(A)


#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		if (PathIdentity->UnderlyingType == EPCGMetadataTypes::SoftObjectPath)
		{
			PCGEX_FOREACH_COLLECTION_ENTRY(FSoftObjectPath, PathIdentity->Name, { SetEntryPath(i, V[i]); })
		}
		else
#endif
		if (PathIdentity->UnderlyingType == EPCGMetadataTypes::String)
		{
			PCGEX_FOREACH_COLLECTION_ENTRY(FString, PathIdentity->Name, { SetEntryPath(i, FSoftObjectPath(V[i])); })
		}
		else
		{
			PCGEX_FOREACH_COLLECTION_ENTRY(FName, PathIdentity->Name, { SetEntryPath(i, FSoftObjectPath(V[i].ToString())); })
		}


		// Weight value
		if (WeightIdentity)
		{
#define PCGEX_ATT_TOINT32(_NAME, _TYPE)\
			if (WeightIdentity->UnderlyingType == EPCGMetadataTypes::_NAME){ \
				PCGEX_FOREACH_COLLECTION_ENTRY(int32, WeightIdentity->Name, {  Collection->Entries[i].Weight = static_cast<int32>(V[i]); }) }

			PCGEX_ATT_TOINT32(Integer32, int32)
			else
				PCGEX_ATT_TOINT32(Double, double)
				else
					PCGEX_ATT_TOINT32(Float, float)
					else
						PCGEX_ATT_TOINT32(Integer64, int64)

#undef PCGEX_ATT_TOINT32
		}

		// Category value
		if (CategoryIdentity)
		{
			if (CategoryIdentity->UnderlyingType == EPCGMetadataTypes::String)
			{
				PCGEX_FOREACH_COLLECTION_ENTRY(FString, WeightIdentity->Name, { Collection->Entries[i].Category = FName(V[i]); })
			}
			else if (CategoryIdentity->UnderlyingType == EPCGMetadataTypes::Name)
			{
				PCGEX_FOREACH_COLLECTION_ENTRY(FName, WeightIdentity->Name, { Collection->Entries[i].Category = V[i]; })
			}
		}

#undef PCGEX_FOREACH_COLLECTION_ENTRY

		Cleanup();
		Collection->RebuildStagingData(false);

		return Collection;
	}

	static UPCGExInternalCollection* GetCollectionFromAttributeSet(
		const FPCGContext* InContext,
		const FName InputPin,
		const FPCGExAssetAttributeSetDetails& Details)
	{
		const TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(InputPin);
		if (Inputs.IsEmpty()) { return nullptr; }
		for (const FPCGTaggedData& InData : Inputs)
		{
			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(InData.Data))
			{
				return GetCollectionFromAttributeSet(InContext, ParamData, Details);
			}
		}
		return nullptr;
	}
}
