// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExTypeErasedProxyBlending.h"
#include "Data/PCGExData.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExTypeErasedProxyHelpers.h"
#include "Data/BlendOperations/PCGExBlendOperations.h"

namespace PCGExDataBlending
{
	//////////////////////////////////////////////////////////////////////////
	// FTypeErasedProxyBlender Implementation
	//////////////////////////////////////////////////////////////////////////

	FTypeErasedProxyBlender::FTypeErasedProxyBlender()
	{
		FMemory::Memzero(AccumulatorStorage);
	}

	FTypeErasedProxyBlender::~FTypeErasedProxyBlender()
	{
	}

	bool FTypeErasedProxyBlender::Init(
		FPCGExContext* InContext,
		const FBlendingParam& InParam,
		const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
		const TSharedPtr<PCGExData::FFacade>& InSourceFacade,
		PCGExData::EIOSide InSide,
		bool bWantsDirectAccess)
	{
		// This method would create proxies from the blending param
		// Implementation depends on full FBlendingParam definition

		BlendMode = InParam.Blending;

		// TODO: Create proxies from param using ProxyHelpers
		// ProxyA = create from InSourceFacade based on InParam.Selector
		// ProxyB = create from InTargetFacade or InSourceFacade
		// ProxyC = create output proxy

		// For now, validate we have what we need and create the blend op
		if (ProxyC)
		{
			WorkingType = ProxyC->GetWorkingType();
			TypeOps = PCGExTypeOps::FTypeOpsRegistry::Get(WorkingType);
			CreateBlendOperation();
		}

		return IsValid();
	}

	bool FTypeErasedProxyBlender::InitFromProxies(
		const TSharedPtr<PCGExData::FTypeErasedProxy>& InProxyA,
		const TSharedPtr<PCGExData::FTypeErasedProxy>& InProxyB,
		const TSharedPtr<PCGExData::FTypeErasedProxy>& InProxyC,
		EPCGExABBlendingType InBlendMode,
		bool bInResetValueForMultiBlend)
	{
		ProxyA = InProxyA;
		ProxyB = InProxyB;
		ProxyC = InProxyC;
		BlendMode = InBlendMode;
		bResetValueForMultiBlend = bInResetValueForMultiBlend;

		// Determine working type from output proxy
		if (ProxyC)
		{
			WorkingType = ProxyC->GetWorkingType();
			TypeOps = PCGExTypeOps::FTypeOpsRegistry::Get(WorkingType);
		}

		// Create the blend operation using the factory
		CreateBlendOperation();

		return IsValid();
	}

	bool FTypeErasedProxyBlender::InitSelfBlend(
		const TSharedPtr<PCGExData::FTypeErasedProxy>& InProxy,
		EPCGExABBlendingType InBlendMode,
		bool bInResetValueForMultiBlend)
	{
		return InitFromProxies(InProxy, InProxy, InProxy, InBlendMode, bInResetValueForMultiBlend);
	}

	void FTypeErasedProxyBlender::CreateBlendOperation()
	{
		if (WorkingType == EPCGMetadataTypes::Unknown) { return; }

		// Use the existing FBlendOperationFactory to create the operation
		// This leverages all the FTypeOps blend functions we already defined

		BlendOp = FBlendOperationFactory::Create(WorkingType, BlendMode, bResetValueForMultiBlend);
	}

	/*
	#define PCGEX_TPL(_TYPE, _NAME, ...) template PCGEXTENDEDTOOLKIT_API void FTypeErasedProxyBlender::Set<_TYPE>(int32 TargetIndex, const _TYPE& Value) const;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
	#undef PCGEX_TPL
	*/

	void FTypeErasedProxyBlender::Blend(int32 SourceIndexA, int32 SourceIndexB, int32 TargetIndex, double Weight)
	{
		if (!BlendOp || !ProxyA || !ProxyC) { return; }

		// Allocate stack buffers for values
		alignas(16) uint8 ValueA[sizeof(FTransform)];
		alignas(16) uint8 ValueB[sizeof(FTransform)];
		alignas(16) uint8 Result[sizeof(FTransform)];

		// Get value A from source
		ProxyA->Get(SourceIndexA, ValueA);

		// Get value B - either from ProxyB or from target's current value
		if (ProxyB && ProxyB != ProxyA)
		{
			ProxyB->Get(SourceIndexB, ValueB);
		}
		else
		{
			// B is same as A or doesn't exist - use target current value
			ProxyC->GetCurrent(TargetIndex, ValueB);
		}

		// Use the IBlendOperation to perform the blend
		// This delegates to FTypeOps::Lerp, FTypeOps::Add, etc. based on blend mode
		BlendOp->Blend(ValueA, ValueB, Weight, Result);

		// Set result to output
		// !BUG : ProxyC->Set(TargetIndex, Result);
	}

	PCGEx::FOpStats FTypeErasedProxyBlender::BeginMultiBlend(int32 TargetIndex)
	{
		PCGEx::FOpStats Stats;

		if (!BlendOp) { return Stats; }

		// Use IBlendOperation::BeginMulti to initialize the accumulator
		// This handles the reset logic based on bResetValueForMultiBlend
		if (bResetValueForMultiBlend)
		{
			BlendOp->BeginMulti(AccumulatorStorage);
		}
		else
		{
			// Start with current target value
			if (ProxyC)
			{
				ProxyC->GetCurrent(TargetIndex, AccumulatorStorage);
			}
			else
			{
				BlendOp->InitDefault(AccumulatorStorage);
			}
		}
		return Stats;
	}

	void FTypeErasedProxyBlender::MultiBlend(int32 SourceIndex, int32 TargetIndex, double Weight, PCGEx::FOpStats& Tracker)
	{
		if (!BlendOp || !ProxyA) { return; }

		// Get source value
		alignas(16) uint8 SourceValue[sizeof(FTransform)];
		ProxyA->Get(SourceIndex, SourceValue);

		// Use IBlendOperation::Accumulate to add to the accumulator
		// This leverages FTypeOps::WeightedAdd etc.
		BlendOp->Accumulate(SourceValue, AccumulatorStorage, Weight);

		// Update tracker
		Tracker.Count++;
		Tracker.Weight += Weight;
	}

	void FTypeErasedProxyBlender::EndMultiBlend(int32 TargetIndex, PCGEx::FOpStats& Tracker)
	{
		if (!BlendOp || !ProxyC) { return; }

		// Use IBlendOperation::EndMulti to finalize
		// This handles averaging, weight normalization, etc.
		BlendOp->EndMulti(AccumulatorStorage, Tracker.Weight, Tracker.Count);

		// Write final result to target
		// !BUG : ProxyC->Set(TargetIndex, AccumulatorStorage);
	}

	void FTypeErasedProxyBlender::Div(int32 TargetIndex, double Divider)
	{
		if (!TypeOps || !ProxyC || Divider == 0.0) { return; }

		// Get current value
		alignas(16) uint8 CurrentValue[sizeof(FTransform)];
		alignas(16) uint8 Result[sizeof(FTransform)];

		ProxyC->GetCurrent(TargetIndex, CurrentValue);

		// Use type ops to divide (BlendDiv takes A, Divisor, Out)
		TypeOps->BlendDiv(CurrentValue, Divider, Result);

		// Set result
		ProxyC->Set(TargetIndex, Result);
	}

	void FTypeErasedProxyBlender::Set(int32 TargetIndex, EPCGMetadataTypes Type, const void* Value) const
	{
		if (!ProxyC) { return; }

		if (Type == WorkingType)
		{
			// Direct set
			ProxyC->Set(TargetIndex, Value);
		}
		else
		{
			// Convert and set
			alignas(16) uint8 ConvertedValue[sizeof(FTransform)];
			PCGExTypeOps::FConversionTable::Convert(Type, Value, WorkingType, ConvertedValue);
			ProxyC->Set(TargetIndex, ConvertedValue);
		}
	}

	TSharedPtr<PCGExData::IBuffer> FTypeErasedProxyBlender::GetOutputBuffer() const
	{
		if (ProxyC) { return ProxyC->GetBuffer(); }
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	// FTypeErasedProxyBlenderFactory Implementation
	//////////////////////////////////////////////////////////////////////////

	TSharedPtr<FTypeErasedProxyBlender> FTypeErasedProxyBlenderFactory::Create(
		FPCGExContext* InContext,
		EPCGExABBlendingType BlendMode,
		const PCGExData::FProxyDescriptor& DescriptorA,
		const PCGExData::FProxyDescriptor& DescriptorB,
		const PCGExData::FProxyDescriptor& DescriptorC,
		bool bResetValueForMultiBlend)
	{
		// Create proxies from descriptors using the helper functions
		auto ProxyA = PCGExData::ProxyHelpers::GetProxy(InContext, DescriptorA);
		auto ProxyB = PCGExData::ProxyHelpers::GetProxy(InContext, DescriptorB);
		auto ProxyC = PCGExData::ProxyHelpers::GetProxy(InContext, DescriptorC);

		if (!ProxyA || !ProxyC) { return nullptr; }

		return CreateFromProxies(ProxyA, ProxyB, ProxyC, BlendMode, bResetValueForMultiBlend);
	}

	TSharedPtr<FTypeErasedProxyBlender> FTypeErasedProxyBlenderFactory::Create(
		FPCGExContext* InContext,
		EPCGExABBlendingType BlendMode,
		const PCGExData::FProxyDescriptor& DescriptorA,
		const PCGExData::FProxyDescriptor& DescriptorC,
		bool bResetValueForMultiBlend)
	{
		// A and B use the same descriptor
		return Create(InContext, BlendMode, DescriptorA, DescriptorA, DescriptorC, bResetValueForMultiBlend);
	}

	TSharedPtr<FTypeErasedProxyBlender> FTypeErasedProxyBlenderFactory::CreateFromProxies(
		const TSharedPtr<PCGExData::FTypeErasedProxy>& ProxyA,
		const TSharedPtr<PCGExData::FTypeErasedProxy>& ProxyB,
		const TSharedPtr<PCGExData::FTypeErasedProxy>& ProxyC,
		EPCGExABBlendingType BlendMode,
		bool bResetValueForMultiBlend)
	{
		auto Blender = MakeShared<FTypeErasedProxyBlender>();
		if (!Blender->InitFromProxies(ProxyA, ProxyB, ProxyC, BlendMode, bResetValueForMultiBlend)) { return nullptr; }
		return Blender;
	}

	TSharedPtr<FTypeErasedProxyBlender> FTypeErasedProxyBlenderFactory::CreateFromParam(
		FPCGExContext* InContext,
		const FBlendingParam& InParam,
		const TSharedPtr<PCGExData::FFacade>& InTargetFacade,
		const TSharedPtr<PCGExData::FFacade>& InSourceFacade,
		PCGExData::EIOSide InSide,
		bool bWantsDirectAccess)
	{
		auto Blender = MakeShared<FTypeErasedProxyBlender>();
		if (!Blender->Init(InContext, InParam, InTargetFacade, InSourceFacade, InSide, bWantsDirectAccess)) { return nullptr; }
		return Blender;
	}
}
