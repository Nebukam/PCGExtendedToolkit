// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExData.h"
#include "PCGExDataFilter.h"
#include "PCGExMT.h"

#include "UObject/Object.h"

namespace PCGExPointIOMerger
{
	struct PCGEXTENDEDTOOLKIT_API FIdentityRef : PCGEx::FAttributeIdentity
	{
		const FPCGMetadataAttributeBase* Attribute = nullptr;
		FPCGAttributeIdentifier ElementsIdentifier;
		bool bInitDefault = false;

		FIdentityRef();
		FIdentityRef(const FIdentityRef& Other);
		FIdentityRef(const FAttributeIdentity& Other);
		FIdentityRef(const FName InName, const EPCGMetadataTypes InUnderlyingType, const bool InAllowsInterpolation);
	};
}

class PCGEXTENDEDTOOLKIT_API FPCGExPointIOMerger final : public TSharedFromThis<FPCGExPointIOMerger>
{
	friend class FPCGExAttributeMergeTask;

public:
	TArray<PCGExPointIOMerger::FIdentityRef> UniqueIdentities;
	TSharedRef<PCGExData::FFacade> UnionDataFacade;
	TArray<TSharedPtr<PCGExData::FPointIO>> IOSources;
	TArray<PCGExMT::FScope> Scopes;

	explicit FPCGExPointIOMerger(const TSharedRef<PCGExData::FFacade>& InUnionDataFacade);
	~FPCGExPointIOMerger();

	PCGExMT::FScope Append(const TSharedPtr<PCGExData::FPointIO>& InData);
	void Append(const TArray<TSharedPtr<PCGExData::FPointIO>>& InData);
	void Append(const TSharedRef<PCGExData::FPointIOCollection>& InCollection);
	void MergeAsync(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails, const TSet<FName>* InIgnoredAttributes = nullptr);

	bool WantsDataToElements() const { return bDataDomainToElements; }

protected:
	bool bDataDomainToElements = false;
	int32 NumCompositePoints = 0;
	EPCGPointNativeProperties AllocateProperties = EPCGPointNativeProperties::None;
};

namespace PCGExPointIOMerger
{
	template <typename T>
	static void ScopeMerge(const PCGExMT::FScope& Scope, const FIdentityRef& Identity, const TSharedPtr<PCGExData::FPointIO>& SourceIO, const TSharedPtr<PCGExData::TBuffer<T>>& OutBuffer)
	{
		UPCGMetadata* InMetadata = SourceIO->GetIn()->Metadata;

		const FPCGMetadataAttribute<T>* TypedInAttribute = PCGEx::TryGetConstAttribute<T>(InMetadata, Identity.Identifier);
		if (!TypedInAttribute) { return; }

		TSharedPtr<PCGExData::TArrayBuffer<T>> OutElementsBuffer = StaticCastSharedPtr<PCGExData::TArrayBuffer<T>>(OutBuffer);
		TSharedPtr<PCGExData::TSingleValueBuffer<T>> OutDataBuffer = StaticCastSharedPtr<PCGExData::TSingleValueBuffer<T>>(OutBuffer);

		if (OutElementsBuffer)
		{
			// We are writing to elements domain

			if (TypedInAttribute->GetMetadataDomain()->GetDomainID().Flag == EPCGMetadataDomainFlag::Data)
			{
				// From a data domain
				const T Value = PCGEX_READ_DATA_ENTRY(TypedInAttribute);
				PCGEX_SCOPE_LOOP(Index) { OutElementsBuffer->SetValue(Index, Value); }
			}
			else
			{
				// From elements domain
				TUniquePtr<const IPCGAttributeAccessor> InAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TypedInAttribute, InMetadata);

				if (!InAccessor.IsValid()) { return; }

				TArrayView<T> InRange = MakeArrayView(OutElementsBuffer->GetOutValues()->GetData() + Scope.Start, Scope.Count);
				InAccessor->GetRange<T>(InRange, 0, *SourceIO->GetInKeys());
			}
		}
		else if (OutDataBuffer)
		{
			// We are writing to data domain

			if (TypedInAttribute->GetMetadataDomain()->GetDomainID().Flag == EPCGMetadataDomainFlag::Data)
			{
				// From data domain
				OutDataBuffer->SetValue(0, PCGEX_READ_DATA_ENTRY(TypedInAttribute));
			}
			else
			{
				// From elements domain
				TUniquePtr<const IPCGAttributeAccessor> InAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TypedInAttribute, InMetadata);
				if (!InAccessor.IsValid()) { return; }
				if (T Value = T{}; InAccessor->Get(Value, *SourceIO->GetInKeys())) { OutDataBuffer->SetValue(0, Value); }
			}
		}
	}

	class PCGEXTENDEDTOOLKIT_API FCopyAttributeTask final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		FCopyAttributeTask(const int32 InTaskIndex, const TSharedPtr<FPCGExPointIOMerger>& InMerger);

		TSharedPtr<FPCGExPointIOMerger> Merger;
		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FWriteAttributeScopeTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FWriteAttributeScopeTask)

		FWriteAttributeScopeTask(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const PCGExMT::FScope& InScope,
			const FIdentityRef& InIdentity,
			const TSharedPtr<PCGExData::TBuffer<T>>& InOutBuffer)
			: FTask(),
			  PointIO(InPointIO),
			  Scope(InScope),
			  Identity(InIdentity),
			  OutBuffer(InOutBuffer)
		{
		}

		const TSharedPtr<PCGExData::FPointIO> PointIO;
		const PCGExMT::FScope Scope;
		const FIdentityRef Identity;
		const TSharedPtr<PCGExData::TBuffer<T>> OutBuffer;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			ScopeMerge<T>(Scope, Identity, PointIO, OutBuffer);
		}
	};
}
