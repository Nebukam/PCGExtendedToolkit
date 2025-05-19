// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExMetadataBlender.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"
#include "Data/Blending/PCGExDataBlending.h"


namespace PCGExDataBlending
{
	void FMetadataBlender::SetSourceData(const TSharedRef<PCGExData::FFacade>& InDataFacade, const PCGExData::EIOSide InSide)
	{
		SourceFacadeHandle = InDataFacade;
		SourceSide = InSide;
	}

	void FMetadataBlender::SetTargetData(const TSharedRef<PCGExData::FFacade>& InDataFacade)
	{
		TargetFacadeHandle = InDataFacade;
	}

	bool FMetadataBlender::Init(FPCGExContext* InContext, const FPCGExBlendingDetails& InBlendingDetails, const TSet<FName>* IgnoreAttributeSet)
	{
		const TSharedPtr<PCGExData::FFacade> SourceFacade = SourceFacadeHandle.Pin();
		const TSharedPtr<PCGExData::FFacade> TargetFacade = TargetFacadeHandle.Pin();

		check(TargetFacade)
		check(SourceFacade)

		TArray<FBlendingHeader> BlendingHeaders;

		InBlendingDetails.GetBlendingHeaders(
			SourceFacade->GetData(SourceSide)->Metadata, TargetFacade->GetOut()->Metadata,
			BlendingHeaders, !bBlendProperties, IgnoreAttributeSet);

		Blenders.Reserve(BlendingHeaders.Num());
		for (const FBlendingHeader& Header : BlendingHeaders)
		{
			// Setup a single blender per A/B pair
			PCGExData::FProxyDescriptor A = PCGExData::FProxyDescriptor(SourceFacade);
			PCGExData::FProxyDescriptor B = PCGExData::FProxyDescriptor(TargetFacade);

			if (!A.Capture(InContext, Header.Selector, SourceSide)) { return false; }

			if (Header.bIsNewAttribute)
			{
				// Capturing B will fail as it does not exist yet.
				// Simply copy A
				B = A;

				// Swap B side for Out so the buffer will be initialized
				B.Side = PCGExData::EIOSide::Out;
				B.DataFacade = TargetFacade;
			}
			else
			{
				// Strict capture may fail here, TBD
				if (!B.CaptureStrict(InContext, Header.Selector, PCGExData::EIOSide::Out)) { return false; }
			}

			PCGExData::FProxyDescriptor C = B;

			TSharedPtr<PCGExDataBlending::FProxyDataBlender> Blender = PCGExDataBlending::CreateProxyBlender(InContext, Header.Blending, A, B, C);

			if (!Blender) { return false; }

			Blenders.Add(Blender);
		}

		return true;
	}

	void FMetadataBlender::Blend(const int32 SourceIndex, const int32 TargetIndex) const
	{
		for (int i = 0; i < Blenders.Num(); i++) { Blenders[i]->Blend(SourceIndex, TargetIndex); }
	}

	void FMetadataBlender::Blend(const int32 SourceAIndex, const int32 SourceBIndex, const int32 TargetIndex, const double Weight) const
	{
		for (int i = 0; i < Blenders.Num(); i++) { Blenders[i]->Blend(SourceAIndex, SourceBIndex, TargetIndex, Weight); }
	}

	void FMetadataBlender::Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight) const
	{
		for (int i = 0; i < Blenders.Num(); i++) { Blenders[i]->Blend(SourceIndex, TargetIndex, Weight); }
	}

	void FMetadataBlender::BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& OutTrackers) const
	{
		OutTrackers.SetNumUninitialized(Blenders.Num());
		for (int i = 0; i < Blenders.Num(); i++) { Blenders[i]->BeginMultiBlend(TargetIndex); }
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
