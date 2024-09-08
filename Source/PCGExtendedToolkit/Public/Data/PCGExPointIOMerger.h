// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExMT.h"
#include "UObject/Object.h"

struct FPCGExCarryOverDetails;

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointIOMerger final
{
	friend class FPCGExAttributeMergeTask;

public:
	TArray<PCGEx::FAttributeIdentity> UniqueIdentities;
	PCGExData::FPointIO* CompositeIO = nullptr;
	TArray<PCGExData::FPointIO*> IOSources;
	TArray<uint64> Scopes;
	TArray<PCGEx::FAttributeIOBase*> Writers;

	FPCGExPointIOMerger(PCGExData::FPointIO* OutMergedData);
	~FPCGExPointIOMerger();

	void Append(PCGExData::FPointIO* InData);
	void Append(const TArray<PCGExData::FPointIO*>& InData);
	void Append(PCGExData::FPointIOCollection* InCollection);
	void Merge(PCGExMT::FTaskManager* AsyncManager, const FPCGExCarryOverDetails* InCarryOverDetails);
	void Write();
	void Write(PCGExMT::FTaskManager* AsyncManager);

protected:
	int32 NumCompositePoints = 0;
};

namespace PCGExPointIOMerger
{
	template <typename T>
	static void ScopeMerge(const uint64 Scope, const PCGEx::FAttributeIdentity& Identity, PCGExData::FPointIO* SourceIO, PCGEx::TAttributeWriter<T>* Writer)
	{
		PCGEx::TAttributeReader<T>* Reader = new PCGEx::TAttributeReader<T>(Identity.Name);
		Reader->Bind(SourceIO);

		uint32 StartIndex;
		uint32 Range;
		PCGEx::H64(Scope, StartIndex, Range);

		const int32 Count = static_cast<int>(Range);
		for (int i = 0; i < Count; i++) { Writer->Values[StartIndex + i] = Reader->Values[i]; }

		PCGEX_DELETE(Reader);
	}

	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteAttributeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteAttributeTask(
			PCGExData::FPointIO* InPointIO,
			FPCGExPointIOMerger* InMerger)
			: FPCGExTask(InPointIO),
			  Merger(InMerger)
		{
		}

		FPCGExPointIOMerger* Merger = nullptr;
		virtual bool ExecuteTask() override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ FWriteAttributeScopeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FWriteAttributeScopeTask(
			PCGExData::FPointIO* InPointIO,
			const uint64 InScope,
			const PCGEx::FAttributeIdentity& InIdentity,
			PCGEx::TAttributeWriter<T>* InWriter)
			: FPCGExTask(InPointIO),
			  Scope(InScope),
			  Identity(InIdentity),
			  Writer(InWriter)
		{
		}

		const uint64 Scope;
		const PCGEx::FAttributeIdentity Identity;
		PCGEx::TAttributeWriter<T>* Writer = nullptr;

		virtual bool ExecuteTask() override
		{
			ScopeMerge<T>(Scope, Identity, PointIO, Writer);
			return true;
		}
	};
}
