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
	struct /*PCGEXTENDEDTOOLKIT_API*/ FIdentityRef : PCGEx::FAttributeIdentity
	{
		const FPCGMetadataAttributeBase* Attribute = nullptr;
		bool bInitDefault = false;

		FIdentityRef() : FAttributeIdentity()
		{
		};

		FIdentityRef(const FIdentityRef& Other)
			: FAttributeIdentity(Other)
		{
		}

		FIdentityRef(const FAttributeIdentity& Other)
			: FAttributeIdentity(Other)
		{
		}

		FIdentityRef(const FName InName, const EPCGMetadataTypes InUnderlyingType, const bool InAllowsInterpolation)
			: FAttributeIdentity(InName, InUnderlyingType, InAllowsInterpolation)
		{
		}
	};
}

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointIOMerger final : public TSharedFromThis<FPCGExPointIOMerger>
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

protected:
	int32 NumCompositePoints = 0;
};

namespace PCGExPointIOMerger
{
	template <typename T>
	static void ScopeMerge(const PCGExMT::FScope& Scope, const PCGEx::FAttributeIdentity& Identity, const TSharedPtr<PCGExData::FPointIO>& SourceIO, TArray<T>& OutValues)
	{
		UPCGMetadata* InMetadata = SourceIO->GetIn()->Metadata;

		// 'template' spec required for clang on mac, and rider keeps removing it without the comment below.
		// ReSharper disable once CppRedundantTemplateKeyword
		const FPCGMetadataAttribute<T>* TypedInAttribute = InMetadata->template GetConstTypedAttribute<T>(Identity.Name);
		TUniquePtr<FPCGAttributeAccessor<T>> InAccessor = MakeUnique<FPCGAttributeAccessor<T>>(TypedInAttribute, InMetadata);

		if (!TypedInAttribute || !InAccessor.IsValid()) { return; }

		TArrayView<T> InRange = MakeArrayView(OutValues.GetData() + Scope.Start, Scope.Count);
		InAccessor->GetRange(InRange, 0, *SourceIO->GetInKeys());
	}

	class /*PCGEXTENDEDTOOLKIT_API*/ FCopyAttributeTask final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		FCopyAttributeTask(
			const int32 InTaskIndex,
			const TSharedPtr<FPCGExPointIOMerger>& InMerger)
			: FPCGExIndexedTask(InTaskIndex),
			  Merger(InMerger)
		{
		}

		TSharedPtr<FPCGExPointIOMerger> Merger;
		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteAttributeScopeTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(FWriteAttributeScopeTask)

		FWriteAttributeScopeTask(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const PCGExMT::FScope& InScope,
			const PCGEx::FAttributeIdentity& InIdentity,
			const TSharedPtr<TArray<T>>& InOutValues)
			: FTask(),
			  PointIO(InPointIO),
			  Scope(InScope),
			  Identity(InIdentity),
			  OutValues(InOutValues)
		{
		}

		const TSharedPtr<PCGExData::FPointIO> PointIO;
		const PCGExMT::FScope Scope;
		const PCGEx::FAttributeIdentity Identity;
		TSharedPtr<TArray<T>> OutValues;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			ScopeMerge<T>(Scope, Identity, PointIO, *OutValues.Get());
		}
	};
}
