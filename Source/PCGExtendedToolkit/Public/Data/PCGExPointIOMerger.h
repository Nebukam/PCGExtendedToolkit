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

	struct PCGEXTENDEDTOOLKIT_API FMergeScope
	{
		PCGExMT::FScope Read;
		PCGExMT::FScope Write;
		bool bReverse = false;
		TArrayView<int32> ReadIndices;

		FMergeScope() = default;
	};
}

class PCGEXTENDEDTOOLKIT_API FPCGExPointIOMerger final : public TSharedFromThis<FPCGExPointIOMerger>
{
	friend class FPCGExAttributeMergeTask;

public:
	TArray<PCGExPointIOMerger::FIdentityRef> UniqueIdentities;
	TSharedRef<PCGExData::FFacade> UnionDataFacade;
	TArray<TSharedPtr<PCGExData::FPointIO>> IOSources;
	TArray<PCGExPointIOMerger::FMergeScope> Scopes;

	explicit FPCGExPointIOMerger(const TSharedRef<PCGExData::FFacade>& InUnionDataFacade);
	~FPCGExPointIOMerger();

	PCGExPointIOMerger::FMergeScope& Append(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExMT::FScope ReadScope, const PCGExMT::FScope WriteScope);
	PCGExPointIOMerger::FMergeScope& Append(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExMT::FScope ReadScope);
	PCGExPointIOMerger::FMergeScope& Append(const TSharedPtr<PCGExData::FPointIO>& InData);
	void Append(const TArray<TSharedPtr<PCGExData::FPointIO>>& InData);
	void MergeAsync(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails, const TSet<FName>* InIgnoredAttributes = nullptr);

	bool WantsDataToElements() const { return bDataDomainToElements; }

protected:
	PCGExPointIOMerger::FMergeScope NullScope;
	bool bDataDomainToElements = false;
	int32 NumCompositePoints = 0;
	EPCGPointNativeProperties AllocateProperties = EPCGPointNativeProperties::None;

	// Utils
	int32 MaxNumElements = 0;
	TArray<int32> ReverseIndices;
};

namespace PCGExPointIOMerger
{
	template <typename T>
	static void ScopeMerge(const FMergeScope& Scope, const FIdentityRef& Identity, const TSharedPtr<PCGExData::FPointIO>& SourceIO, const TSharedPtr<PCGExData::TBuffer<T>>& OutBuffer)
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
				const T Value = PCGExDataHelpers::ReadDataValue(TypedInAttribute);
				for (int Index = Scope.Write.Start; Index < Scope.Write.End; Index++) { OutElementsBuffer->SetValue(Index, Value); }
			}
			else
			{
				// From elements domain
				TUniquePtr<const IPCGAttributeAccessor> InAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TypedInAttribute, InMetadata);

				if (!InAccessor.IsValid()) { return; }

				TArrayView<T> InRange = MakeArrayView(OutElementsBuffer->GetOutValues()->GetData() + Scope.Write.Start, Scope.Write.Count);

				if (Scope.bReverse)
				{
					TArray<T> ReadData;
					PCGEx::InitArray(ReadData, Scope.Read.Count);
					InAccessor->GetRange<T>(ReadData, Scope.Read.Start, *SourceIO->GetInKeys());

					int32 WriteIndex = Scope.Write.Start;

					for (int i = ReadData.Num() - 1; i >= 0; --i) { InRange[WriteIndex++] = ReadData[i]; }
				}
				else
				{
					InAccessor->GetRange<T>(InRange, Scope.Read.Start, *SourceIO->GetInKeys());
				}
			}
		}
		else if (OutDataBuffer)
		{
			// We are writing to data domain

			if (TypedInAttribute->GetMetadataDomain()->GetDomainID().Flag == EPCGMetadataDomainFlag::Data)
			{
				// From data domain
				OutDataBuffer->SetValue(0, PCGExDataHelpers::ReadDataValue(TypedInAttribute));
			}
			else
			{
				// From elements domain
				TUniquePtr<const IPCGAttributeAccessor> InAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(TypedInAttribute, InMetadata);
				if (!InAccessor.IsValid()) { return; }
				if (T Value = T{}; InAccessor->Get(Value, Scope.Read.Start, *SourceIO->GetInKeys())) { OutDataBuffer->SetValue(0, Value); }
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
			const FMergeScope& InScope,
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
		const FMergeScope Scope;
		const FIdentityRef Identity;
		const TSharedPtr<PCGExData::TBuffer<T>> OutBuffer;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			ScopeMerge<T>(Scope, Identity, PointIO, OutBuffer);
		}
	};
}
