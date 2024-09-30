// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExData.h"
#include "PCGExDataFilter.h"
#include "PCGExMT.h"


#include "UObject/Object.h"

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointIOMerger final : public TSharedFromThis<FPCGExPointIOMerger>
{
	friend class FPCGExAttributeMergeTask;

public:
	TArray<PCGEx::FAttributeIdentity> UniqueIdentities;
	TSharedRef<PCGExData::FFacade> CompoundDataFacade;
	TArray<TSharedPtr<PCGExData::FPointIO>> IOSources;
	TArray<uint64> Scopes;
	TArray<TSharedPtr<PCGExData::FBufferBase>> Buffers;

	FPCGExPointIOMerger(const TSharedRef<PCGExData::FFacade>& InCompoundDataFacade);
	~FPCGExPointIOMerger();

	void Append(const TSharedPtr<PCGExData::FPointIO>& InData);
	void Append(const TArray<TSharedPtr<PCGExData::FPointIO>>& InData);
	void Append(PCGExData::FPointIOCollection* InCollection);
	void Merge(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails);

protected:
	int32 NumCompositePoints = 0;
};

namespace PCGExPointIOMerger
{
	template <typename T>
	static void ScopeMerge(const uint64 Scope, const PCGEx::FAttributeIdentity& Identity, const TSharedPtr<PCGExData::FPointIO>& SourceIO, TArray<T>& OutValues)
	{
		UPCGMetadata* InMetadata = SourceIO->GetIn()->Metadata;

		// 'template' spec required for clang on mac, not sure why.
		// ReSharper disable once CppRedundantTemplateKeyword
		const FPCGMetadataAttribute<T>* TypedInAttribute = InMetadata->template GetConstTypedAttribute<T>(Identity.Name);
		TUniquePtr<FPCGAttributeAccessor<T>> InAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedInAttribute, InMetadata);

		if (!TypedInAttribute || !InAccessor.IsValid()) { return; }

		uint32 StartIndex;
		uint32 Range;
		PCGEx::H64(Scope, StartIndex, Range);

		TArrayView<T> InRange = MakeArrayView(OutValues.GetData() + StartIndex, Range);
		InAccessor->GetRange(InRange, 0, *SourceIO->GetInKeys());
	}

	class /*PCGEXTENDEDTOOLKIT_API*/ FCopyAttributeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FCopyAttributeTask(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const TSharedPtr<FPCGExPointIOMerger>& InMerger)
			: FPCGExTask(InPointIO),
			  Merger(InMerger)
		{
		}

		TSharedPtr<FPCGExPointIOMerger> Merger;
		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteAttributeScopeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteAttributeScopeTask(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const uint64 InScope,
			const PCGEx::FAttributeIdentity& InIdentity,
			const TSharedPtr<TArray<T>>& InOutValues)
			: FPCGExTask(InPointIO),
			  Scope(InScope),
			  Identity(InIdentity),
			  OutValues(InOutValues)
		{
		}

		const uint64 Scope;
		const PCGEx::FAttributeIdentity Identity;
		const TSharedPtr<TArray<T>> OutValues;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			ScopeMerge<T>(Scope, Identity, PointIO, *OutValues.Get());
			return true;
		}
	};
}
