// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Blenders/PCGExMetadataBlender.h"

#include "Core/PCGExOpStats.h"
#include "Data/PCGExData.h"
#include "Data/PCGExProxyData.h"
#include "Details/PCGExBlendingDetails.h"

namespace PCGExBlending
{
	void FMetadataBlender::SetSourceData(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const PCGExData::EIOSide InSide, const bool bUseAsSecondarySource)
	{
		SourceFacadeHandle = InDataFacade;
		SourceSide = InSide;
		bUseTargetAsSecondarySource = !bUseAsSecondarySource;
	}

	void FMetadataBlender::SetTargetData(const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		TargetFacadeHandle = InDataFacade;
	}

	bool FMetadataBlender::Init(FPCGExContext* InContext, const FPCGExBlendingDetails& InBlendingDetails, const TSet<FName>* IgnoreAttributeSet, const bool bWantsDirectAccess, const PCGExData::EIOSide BSide)
	{
		const TSharedPtr<PCGExData::FFacade> SourceFacade = SourceFacadeHandle.Pin();
		const TSharedPtr<PCGExData::FFacade> TargetFacade = TargetFacadeHandle.Pin();

		check(TargetFacade)
		check(SourceFacade)

		TArray<FBlendingParam> BlendingParams;

		InBlendingDetails.GetBlendingParams(SourceFacade->GetData(SourceSide)->Metadata, TargetFacade->GetOut()->Metadata, BlendingParams, AttributeIdentifiers, !bBlendProperties, IgnoreAttributeSet);

		Blenders.Reserve(BlendingParams.Num());
		for (const FBlendingParam& Param : BlendingParams)
		{
			// Setup a single blender per A/B pair

			PCGExData::FProxyDescriptor A = PCGExData::FProxyDescriptor(SourceFacade, PCGExData::EProxyRole::Read);
			PCGExData::FProxyDescriptor B = PCGExData::FProxyDescriptor(bUseTargetAsSecondarySource ? TargetFacade : SourceFacade, PCGExData::EProxyRole::Read);

			if (!A.Capture(InContext, Param.Selector, SourceSide)) { return false; }

			if (Param.bIsNewAttribute)
			{
				// Capturing B will fail as it does not exist yet.
				// Simply copy A
				B = A;

				if (bUseTargetAsSecondarySource)
				{
					// Swap B side for Out so the buffer will be initialized
					B.Side = PCGExData::EIOSide::Out;
					B.DataFacade = TargetFacade;
				}
			}
			else
			{
				// Strict capture may fail here, TBD
				if (!B.CaptureStrict(InContext, Param.Selector, BSide)) { return false; }
			}

			PCGExData::FProxyDescriptor C = B;
			C.DataFacade = TargetFacade;
			C.Side = PCGExData::EIOSide::Out;
			C.Role = PCGExData::EProxyRole::Write;

			A.bWantsDirect = bWantsDirectAccess;
			B.bWantsDirect = bWantsDirectAccess;
			C.bWantsDirect = bWantsDirectAccess;

			TSharedPtr<FProxyDataBlender> Blender = CreateProxyBlender(InContext, Param.Blending, A, B, C);

			if (!Blender) { return false; }

			Blenders.Add(Blender);
		}

		return true;
	}

	void FMetadataBlender::Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight) const
	{
		for (int i = 0; i < Blenders.Num(); i++) { Blenders[i]->Blend(SourceIndex, TargetIndex, Weight); }
	}

	void FMetadataBlender::Blend(const int32 SourceAIndex, const int32 SourceBIndex, const int32 TargetIndex, const double Weight) const
	{
		for (int i = 0; i < Blenders.Num(); i++) { Blenders[i]->Blend(SourceAIndex, SourceBIndex, TargetIndex, Weight); }
	}

	void FMetadataBlender::InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const
	{
		Trackers.SetNumUninitialized(Blenders.Num());
	}

	void FMetadataBlender::BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const
	{
		for (int i = 0; i < Blenders.Num(); i++) { Trackers[i] = Blenders[i]->BeginMultiBlend(TargetIndex); }
	}

	void FMetadataBlender::MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, TArray<PCGEx::FOpStats>& Trackers) const
	{
		for (int i = 0; i < Blenders.Num(); i++) { Blenders[i]->MultiBlend(SourceIndex, TargetIndex, Weight, Trackers[i]); }
	}

	void FMetadataBlender::EndMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const
	{
		for (int i = 0; i < Blenders.Num(); i++) { Blenders[i]->EndMultiBlend(TargetIndex, Trackers[i]); }
	}
}
