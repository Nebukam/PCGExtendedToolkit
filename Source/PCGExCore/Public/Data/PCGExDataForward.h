// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataFilter.h"
#include "Types/PCGExAttributeIdentity.h"
#include "UObject/Object.h"

#include "PCGExDataForward.generated.h"


namespace PCGExData
{
	struct FConstPoint;
	class IBuffer;
	class FDataForwardHandler;
	class IAttributeBroadcaster;
}

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExForwardDetails : public FPCGExNameFiltersDetails
{
	GENERATED_BODY()

	FPCGExForwardDetails()
	{
	}

	explicit FPCGExForwardDetails(const bool InEnabled)
		: bEnabled(InEnabled)
	{
	}

	/** Is forwarding enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0))
	bool bEnabled = false;

	/** If enabled, will preserve the initial attribute default value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0, EditCondition="bEnabled"))
	bool bPreserveAttributesDefaultValue = false;

	void Filter(TArray<PCGExData::FAttributeIdentity>& Identities) const;

	TSharedPtr<PCGExData::FDataForwardHandler> GetHandler(const TSharedPtr<PCGExData::FFacade>& InSourceDataFacade, bool bForwardToDataDomain = true) const;
	TSharedPtr<PCGExData::FDataForwardHandler> GetHandler(const TSharedPtr<PCGExData::FFacade>& InSourceDataFacade, const TSharedPtr<PCGExData::FFacade>& InTargetDataFacade, bool bForwardToDataDomain = true) const;
	TSharedPtr<PCGExData::FDataForwardHandler> TryGetHandler(const TSharedPtr<PCGExData::FFacade>& InSourceDataFacade, bool bForwardToDataDomain = true) const;
	TSharedPtr<PCGExData::FDataForwardHandler> TryGetHandler(const TSharedPtr<PCGExData::FFacade>& InSourceDataFacade, const TSharedPtr<PCGExData::FFacade>& InTargetDataFacade, bool bForwardToDataDomain = true) const;
};

namespace PCGExData
{
	class PCGEXCORE_API FDataForwardHandler
	{
		FPCGExForwardDetails Details;
		TSharedPtr<FFacade> SourceDataFacade;
		TSharedPtr<FFacade> TargetDataFacade;
		TArray<PCGExData::FAttributeIdentity> Identities;
		TArray<TSharedPtr<IBuffer>> Readers;
		TArray<TSharedPtr<IBuffer>> Writers;
		bool bElementDomainToDataDomain = false;

	public:
		~FDataForwardHandler() = default;
		FDataForwardHandler(const FPCGExForwardDetails& InDetails, const TSharedPtr<FFacade>& InSourceDataFacade, const bool ElementDomainToDataDomain = false);
		FDataForwardHandler(const FPCGExForwardDetails& InDetails, const TSharedPtr<FFacade>& InSourceDataFacade, const TSharedPtr<FFacade>& InTargetDataFacade, const bool ElementDomainToDataDomain = false);
		bool IsEmpty() const { return Identities.IsEmpty(); }
		void Forward(const int32 SourceIndex, const int32 TargetIndex);
		void Forward(const int32 SourceIndex, const TSharedPtr<FFacade>& InTargetDataFacade);
		void Forward(const int32 SourceIndex, const TSharedPtr<FFacade>& InTargetDataFacade, const TArray<int32>& Indices);
		void Forward(const int32 SourceIndex, UPCGMetadata* InTargetMetadata);
	};
}

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExAttributeToTagDetails
{
	GENERATED_BODY()

	FPCGExAttributeToTagDetails()
	{
	}

	/** Use reference point index to tag output data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bAddIndexTag = false;

	/** Prefix added to the reference point index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bAddIndexTag"))
	FString IndexTagPrefix = TEXT("IndexTag");

	/** If enabled, prefix the attribute value with the attribute name  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bPrefixWithAttributeName = true;

	/** Attributes which value will be used as tags. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGAttributePropertyInputSelector> Attributes;

	/** A list of selectors separated by a comma, for easy overrides. Will be appended to the existing array. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString CommaSeparatedAttributeSelectors;

	TSharedPtr<PCGExData::FFacade> SourceDataFacade;
	TArray<TSharedPtr<PCGExData::IAttributeBroadcaster>> Getters;

	bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSourceFacade);
	void Tag(const PCGExData::FConstPoint& TagSource, TSet<FString>& InTags) const;
	void Tag(const PCGExData::FConstPoint& TagSource, const TSharedPtr<PCGExData::FPointIO>& PointIO) const;
	void Tag(const PCGExData::FConstPoint& TagSource, UPCGMetadata* InMetadata) const;
};
