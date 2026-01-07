// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Metadata/PCGAttributePropertySelector.h"

struct FPCGContext;

namespace PCGExData
{
	class FPointIOCollection;
}

struct FPCGExAttributeGatherDetails;
class FPCGMetadataAttributeBase;
class UPCGMetadata;


namespace PCGExData
{
	struct PCGEXCORE_API FAttributeIdentity
	{
		FPCGAttributeIdentifier Identifier = NAME_None;
		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;
		bool bAllowsInterpolation = true;

		FAttributeIdentity() = default;

		FAttributeIdentity(const FPCGAttributeIdentifier& InIdentifier, const EPCGMetadataTypes InUnderlyingType, const bool InAllowsInterpolation)
			: Identifier(InIdentifier), UnderlyingType(InUnderlyingType), bAllowsInterpolation(InAllowsInterpolation)
		{
		}

		bool InDataDomain() const;
		int16 GetTypeId() const { return static_cast<int16>(UnderlyingType); }
		bool IsA(const int16 InType) const { return GetTypeId() == InType; }
		bool IsA(const EPCGMetadataTypes InType) const { return UnderlyingType == InType; }

		FString GetDisplayName() const;
		bool operator==(const FAttributeIdentity& Other) const;

		static void Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities, const TSet<FName>* OptionalIgnoreList = nullptr);
		static void Get(const UPCGMetadata* InMetadata, TArray<FPCGAttributeIdentifier>& OutIdentifiers, TMap<FPCGAttributeIdentifier, FAttributeIdentity>& OutIdentities, const TSet<FName>* OptionalIgnoreList = nullptr);
		static bool Get(const UPCGData* InData, const FPCGAttributePropertyInputSelector& InSelector, FAttributeIdentity& OutIdentity);

		using FForEachFunc = std::function<void (const FAttributeIdentity&, const int32)>;
		static int32 ForEach(const UPCGMetadata* InMetadata, FForEachFunc&& Func);
	};

	class PCGEXCORE_API FAttributesInfos : public TSharedFromThis<FAttributesInfos>
	{
	public:
		TMap<FPCGAttributeIdentifier, int32> Map;
		TArray<FAttributeIdentity> Identities;
		TArray<FPCGMetadataAttributeBase*> Attributes;

		bool Contains(FName AttributeName, EPCGMetadataTypes Type);
		bool Contains(FName AttributeName);
		FAttributeIdentity* Find(FName AttributeName);

		bool FindMissing(const TSet<FName>& Checklist, TSet<FName>& OutMissing);
		bool FindMissing(const TArray<FName>& Checklist, TSet<FName>& OutMissing);

		void Append(const TSharedPtr<FAttributesInfos>& Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch);
		void Append(const TSharedPtr<FAttributesInfos>& Other, TSet<FName>& OutTypeMismatch, const TSet<FName>* InIgnoredAttributes = nullptr);
		void Update(const FAttributesInfos* Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch);

		using FilterCallback = std::function<bool(const FName&)>;

		void Filter(const FilterCallback& FilterFn);

		~FAttributesInfos() = default;

		static TSharedPtr<FAttributesInfos> Get(const UPCGMetadata* InMetadata, const TSet<FName>* IgnoredAttributes = nullptr);
		static TSharedPtr<FAttributesInfos> Get(const TSharedPtr<FPointIOCollection>& InCollection, TSet<FName>& OutTypeMismatch, const TSet<FName>* IgnoredAttributes = nullptr);
	};

	PCGEXCORE_API
	void GatherAttributes(const TSharedPtr<FAttributesInfos>& OutInfos, const FPCGContext* InContext, const FName InputLabel, const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches);

	PCGEXCORE_API
	TSharedPtr<FAttributesInfos> GatherAttributes(const FPCGContext* InContext, const FName InputLabel, const FPCGExAttributeGatherDetails& InDetails, TSet<FName>& Mismatches);

	PCGEXCORE_API
	TSharedPtr<FAttributesInfos> GatherAttributeInfos(const FPCGContext* InContext, const FName InPinLabel, const FPCGExAttributeGatherDetails& InGatherDetails, const bool bThrowError);
}
