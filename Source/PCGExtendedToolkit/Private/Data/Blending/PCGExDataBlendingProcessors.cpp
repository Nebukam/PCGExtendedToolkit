// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlendingProcessors.h"

namespace PCGExDataBlending
{
	template <typename T>
	T TDataBlendingAverage<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::Add(A, B); }
	template <typename T>
	void TDataBlendingAverage<T>::SingleComplete(T& A, const int32 Count, const double Weight) const  { A = PCGExMath::Div(A, static_cast<double>(Count)); }

	template <typename T>
	T TDataBlendingCopy<T>::SingleOperation(T A, T B, double Weight) const  { return B; }

	template <typename T>
	T TDataBlendingCopyOther<T>::SingleOperation(T A, T B, double Weight) const  { return A; }

	template <typename T>
	void TDataBlendingSum<T>::SinglePrepare(T& A) const  { A = T{}; }
	template <typename T>
	T TDataBlendingSum<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::Add(A, B); }

	template <typename T>
	T TDataBlendingSubtract<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::Sub(A, B); }

	template <typename T>
	T TDataBlendingMax<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::Max(A, B); }

	template <typename T>
	T TDataBlendingMin<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::Min(A, B); }

	template <typename T>
	T TDataBlendingWeight<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::WeightedAdd(A, B, Weight); } // PCGExMath::Lerp(A, B, Alpha); }
	template <typename T>
	void TDataBlendingWeight<T>::SingleComplete(T& A, const int32 Count, const double Weight) const  { A = PCGExMath::Div(A, Weight); }

	template <typename T>
	T TDataBlendingWeightedSum<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::WeightedAdd(A, B, Weight); }

	template <typename T>
	T TDataBlendingLerp<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::Lerp(A, B, Weight); }

	template <typename T>
	T TDataBlendingNone<T>::SingleOperation(T A, T B, double Weight) const  { return A; }

	template <typename T>
	T TDataBlendingUnsignedMax<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::UnsignedMax(A, B); }

	template <typename T>
	T TDataBlendingUnsignedMin<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::UnsignedMin(A, B); }

	template <typename T>
	T TDataBlendingAbsoluteMax<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::AbsoluteMax(A, B); }

	template <typename T>
	T TDataBlendingAbsoluteMin<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::AbsoluteMin(A, B); }

	template <typename T>
	T TDataBlendingWeightedSubtract<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::WeightedSub(A, B, Weight); }

	template <typename T>
	T TDataBlendingHash<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::NaiveHash(A, B); }

	template <typename T>
	T TDataBlendingUnsignedHash<T>::SingleOperation(T A, T B, double Weight) const  { return PCGExMath::NaiveUnsignedHash(A, B); }

#define PCGEX_EXTERN_DECL_TYPED(_TYPE, _ID, _BLEND) template class TDataBlending##_BLEND<_TYPE>;
#define PCGEX_EXTERN_DECL(_BLEND) PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_EXTERN_DECL_TYPED, _BLEND)

	PCGEX_FOREACH_BLENDMODE(PCGEX_EXTERN_DECL)

#undef PCGEX_EXTERN_DECL_TYPED
#undef PCGEX_EXTERN_DECL


	TSharedPtr<FDataBlendingProcessorBase> PCGExDataBlending::CreateProcessor(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewProcessor = MakeShared<TDataBlending##_ID<_TYPE>>(); break;
#define PCGEX_BLEND_CASE(_ID) case EPCGExDataBlendingType::_ID: switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NEW, _ID) } break;

		TSharedPtr<FDataBlendingProcessorBase> NewProcessor;

		switch (Type)
		{
		default:
		PCGEX_FOREACH_BLENDMODE(PCGEX_BLEND_CASE)
		}

		if (NewProcessor) { NewProcessor->SetAttributeName(Identity.Name); }
		return NewProcessor;

#undef PCGEX_SAO_NEW
#undef PCGEX_BLEND_CASE
	}

	TSharedPtr<FDataBlendingProcessorBase> PCGExDataBlending::CreateProcessorWithDefaults(const EPCGExDataBlendingType DefaultType, const PCGEx::FAttributeIdentity& Identity)
	{
		EPCGExDataBlendingTypeDefault GlobalDefaultType = EPCGExDataBlendingTypeDefault::Default;

#define PCGEX_DEF_SET(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : GlobalDefaultType = GetDefault<UPCGExGlobalSettings>()->Default##_NAME##BlendMode; break;

		switch (Identity.UnderlyingType)
		{
		default:
			break;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DEF_SET)
		}

		if (GlobalDefaultType == EPCGExDataBlendingTypeDefault::Default) { return CreateProcessor(DefaultType, Identity); }
		return CreateProcessor(static_cast<EPCGExDataBlendingType>(static_cast<uint8>(GlobalDefaultType)), Identity);

#undef PCGEX_DEF_SET
#undef PCGEX_BLEND_CASE
	}

	TSharedPtr<FDataBlendingProcessorBase> PCGExDataBlending::CreateProcessor(const EPCGExDataBlendingType* Type, const EPCGExDataBlendingType DefaultType, const PCGEx::FAttributeIdentity& Identity)
	{
		return Type ? CreateProcessor(*Type, Identity) : CreateProcessorWithDefaults(DefaultType, Identity);
	}
}
