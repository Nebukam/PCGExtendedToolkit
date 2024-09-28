// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExDataFilter.h"
#include "PCGExMT.h"


#include "UObject/Object.h"

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointIOMerger final : public TSharedFromThis<FPCGExPointIOMerger>
{
	friend class FPCGExAttributeMergeTask;

public:
	TArray<PCGEx::FAttributeIdentity> UniqueIdentities;
	TSharedPtr<PCGExData::FPointIO> CompositeIO;
	TArray<TSharedPtr<PCGExData::FPointIO>> IOSources;
	TArray<uint64> Scopes;
	TArray<TSharedPtr<PCGEx::FAttributeIOBase>> Writers;

	FPCGExPointIOMerger(TSharedPtr<PCGExData::FPointIO> OutMergedData);
	~FPCGExPointIOMerger();

	void Append(const TSharedPtr<PCGExData::FPointIO>& InData);
	void Append(const TArray<TSharedPtr<PCGExData::FPointIO>>& InData);
	void Append(PCGExData::FPointIOCollection* InCollection);
	void Merge(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails);
	void Write();
	void Write(TSharedPtr<PCGExMT::FTaskManager> AsyncManager);

protected:
	int32 NumCompositePoints = 0;
};

namespace PCGExPointIOMerger
{
	template <typename T>
	static void ScopeMerge(const uint64 Scope, const PCGEx::FAttributeIdentity& Identity, const TSharedPtr<PCGExData::FPointIO>& SourceIO, const TSharedPtr<PCGEx::TAttributeWriter<T>>& Writer)
	{
		TUniquePtr<PCGEx::TAttributeReader<T>> Reader = MakeUnique<PCGEx::TAttributeReader<T>>(Identity.Name);
		Reader->Bind(SourceIO);

		uint32 StartIndex;
		uint32 Range;
		PCGEx::H64(Scope, StartIndex, Range);

		const int32 Count = static_cast<int>(Range);
		for (int i = 0; i < Count; ++i) { Writer->Values[StartIndex + i] = Reader->Values[i]; }
	}

	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteAttributeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteAttributeTask(
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
			const TSharedPtr<PCGEx::TAttributeWriter<T>>& InWriter)
			: FPCGExTask(InPointIO),
			  Scope(InScope),
			  Identity(InIdentity),
			  Writer(InWriter)
		{
		}

		const uint64 Scope;
		const PCGEx::FAttributeIdentity Identity;
		const TSharedPtr<PCGEx::TAttributeWriter<T>> Writer;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			ScopeMerge<T>(Scope, Identity, PointIO, Writer);
			return true;
		}
	};
}
