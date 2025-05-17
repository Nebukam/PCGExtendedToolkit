// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"
#include "UObject/Object.h"

#include "PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	BOOKMARK_BLENDMODE

	inline bool ResetBlend[] = {
		false, //None            
		true,  //Average         
		true,  //Weight          
		false, //Min             
		false, //Max             
		false, //Copy            
		true,  //Sum             
		true,  //WeightedSum     
		false, //Lerp            
		false, //Subtract        
		false, //UnsignedMin     
		false, //UnsignedMax     
		false, //AbsoluteMin     
		false, //AbsoluteMax     
		true,  //WeightedSubtract		
		false, //CopyOther		
		false, //Hash		
		false, //UnsignedHash		
	};

#pragma region Value Range Blending

	class PCGEXTENDEDTOOLKIT_API FValueRangeBlender : public TSharedFromThis<FValueRangeBlender>
	{
	public:
		virtual ~FValueRangeBlender() = default;

		virtual bool Init(UPCGBasePointData* InReadDataA, UPCGBasePointData* InReadDataB, UPCGBasePointData* InWriteData) = 0;

		virtual void PrepareBlending(const int32 TargetIndex, const int32 Default) const = 0;
		virtual void Blend(const int32 A, const int32 B, const int32 TargetIndex, double Weight) const = 0;
		virtual void CompleteBlending(const int32 TargetIndex, const int32 Count, const double TotalWeight) const = 0;

		virtual void PrepareRangeBlending(const TArrayView<const int32>& Targets, const int32 Default) const
		{
			for (const int32 TargetIndex : Targets) { PrepareBlending(TargetIndex, Default); }
		}

		virtual void BlendRange(const int32 From, const int32 To, const TArrayView<const int32> Targets, const TArrayView<const double>& Weights) const
		{
			for (int i = 0; i < Targets.Num(); i++) { Blend(From, To, Targets[i], Weights[i]); }
		}

		virtual void CompleteRangeBlending(const TArrayView<const int32>& Targets, const TArrayView<const int32>& Counts, const TArrayView<const double>& TotalWeights)
		{
			for (int i = 0; i < Targets.Num(); i++) { CompleteBlending(Targets[i], Counts[i], TotalWeights[i]); }
		}

		virtual void BlendRangeFromTo(const int32 From, const int32 To, const TArrayView<const int32> Targets, const TArrayView<const double>& Weights) const
		{
			if constexpr (bPrepare)
			{
				PrepareRangeBlending(Targets, From);
				BlendRange(From, To, Targets, Weights);
				for (int i = 0; i < Targets.Num(); i++) { CompleteBlending(Targets[i], 2, 1); }
			}
			else
			{
				BlendRange(From, To, Targets, Weights);
			}
		}

		bool WantsPreparation() const { return bPrepare; }

	protected:
		bool bPrepare = false;
		bool bResetValue = false;
	};

	template <EPCGPointProperties PointProperty, typename RawType, typename WorkingType, EPCGExDataBlendingType BlendMode>
	class PCGEXTENDEDTOOLKIT_API TValueRangeBlender : public FValueRangeBlender
	{
	public:
		virtual bool Init(UPCGBasePointData* InReadDataA, UPCGBasePointData* InReadDataB, UPCGBasePointData* InWriteData) override
		{
			if (ResetBlend[static_cast<uint8>(BlendMode)])
			{
				bResetValue = true;
				bPrepare = true;
			}

			if constexpr (std::is_same_v<RawType, WorkingType>)
			{
				if constexpr (PointProperty == EPCGPointProperties::Density)
				{
					RangeA = InReadDataA->GetConstDensityValueRange();
					RangeB = InReadDataB->GetConstDensityValueRange();
					WriteRange = InWriteData->GetDensityValueRange();
					ZeroedDefault = 0;
					return true;
				}
				else if constexpr (PointProperty == EPCGPointProperties::BoundsMin)
				{
					RangeA = InReadDataA->GetConstBoundsMinValueRange();
					RangeB = InReadDataB->GetConstBoundsMinValueRange();
					WriteRange = InWriteData->GetBoundsMinValueRange();
					ZeroedDefault = FVector::ZeroVector;
					return true;
				}
				else if constexpr (PointProperty == EPCGPointProperties::BoundsMax)
				{
					RangeA = InReadDataA->GetConstBoundsMaxValueRange();
					RangeB = InReadDataB->GetConstBoundsMaxValueRange();
					WriteRange = InWriteData->GetBoundsMaxValueRange();
					ZeroedDefault = FVector::ZeroVector;
					return true;
				}
				else if constexpr (PointProperty == EPCGPointProperties::Color)
				{
					RangeA = InReadDataA->GetConstColorValueRange();
					RangeB = InReadDataB->GetConstColorValueRange();
					WriteRange = InWriteData->GetColorValueRange();
					ZeroedDefault = FVector4::Zero();
					return true;
				}
				else if constexpr (PointProperty == EPCGPointProperties::Steepness)
				{
					RangeA = InReadDataA->GetConstDensityValueRange();
					RangeB = InReadDataB->GetConstDensityValueRange();
					WriteRange = InWriteData->GetDensityValueRange();
					ZeroedDefault = 0;
					return true;
				}
				else if constexpr (PointProperty == EPCGPointProperties::Seed)
				{
					RangeA = InReadDataA->GetConstDensityValueRange();
					RangeB = InReadDataB->GetConstDensityValueRange();
					WriteRange = InWriteData->GetDensityValueRange();
					ZeroedDefault = 0;
					return true;
				}
			}
			else
			{
				if constexpr (PointProperty == EPCGPointProperties::Position ||
					PointProperty == EPCGPointProperties::Rotation ||
					PointProperty == EPCGPointProperties::Scale)
				{
					RangeA = InReadDataA->GetConstTransformValueRange();
					RangeB = InReadDataB->GetConstTransformValueRange();
					WriteRange = InWriteData->GetTransformValueRange();

					if constexpr (PointProperty == EPCGPointProperties::Scale) { ZeroedDefault = FVector::OneVector; }
					else { ZeroedDefault = FVector::ZeroVector; }

					return true;
				}
			}

			return false;
		}

		virtual void PrepareBlending(const int32 TargetIndex, const int32 Default) const override
		{
			if constexpr (BlendMode != EPCGExDataBlendingType::None)
			{
				if constexpr (std::is_same_v<RawType, WorkingType>)
				{
					WriteRange[TargetIndex] = bResetValue ? ZeroedDefault : WriteRange[Default];
				}
				else
				{
					if constexpr (PointProperty == EPCGPointProperties::Position) { WriteRange[TargetIndex].SetLocation(bResetValue ? ZeroedDefault : WriteRange[Default].GetLocation()); }
					else if constexpr (PointProperty == EPCGPointProperties::Rotation) { WriteRange[TargetIndex].SetRotation(bResetValue ? ZeroedDefault : WriteRange[Default].GetRotation()); }
					else if constexpr (PointProperty == EPCGPointProperties::Scale) { WriteRange[TargetIndex].SetScale3D(bResetValue ? ZeroedDefault : WriteRange[Default].GetScale3D()); }
				}
			}
		}

		virtual void Blend(const int32 A, const int32 B, const int32 TargetIndex, double Weight) const override
		{
			if constexpr (BlendMode != EPCGExDataBlendingType::None)
			{
				WorkingType ValueA;
				WorkingType ValueB;
				WorkingType ValueC;

				// Get values to be blended

				if constexpr (std::is_same_v<RawType, WorkingType>)
				{
					ValueA = RangeA[A];
					ValueB = RangeB[B];
					ValueC = WriteRange[TargetIndex];
				}
				else
				{
					if constexpr (PointProperty == EPCGPointProperties::Position)
					{
						ValueA = RangeA[A].GetLocation();
						ValueB = RangeB[B].GetLocation();
						ValueC = WriteRange[TargetIndex].GetLocation();
					}
					else if constexpr (PointProperty == EPCGPointProperties::Rotation)
					{
						ValueA = RangeA[A].GetRotation();
						ValueB = RangeB[B].GetRotation();
						ValueC = WriteRange[TargetIndex].GetRotation();
					}
					else if constexpr (PointProperty == EPCGPointProperties::Scale)
					{
						ValueA = RangeA[A].GetScale3D();
						ValueB = RangeB[B].GetScale3D();
						ValueC = WriteRange[TargetIndex].GetScale3D();
					}
				}

				// Blend according to selected mode

				if constexpr (BlendMode == EPCGExDataBlendingType::None)
				{
				}
				else if constexpr (BlendMode == EPCGExDataBlendingType::Average) { ValueC = PCGExBlend::Add(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::Min) { ValueC = PCGExBlend::Min(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::Max) { ValueC = PCGExBlend::Max(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::Copy) { ValueC = PCGExBlend::Copy(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::Sum) { ValueC = PCGExBlend::Add(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::Weight) { ValueC = PCGExBlend::WeightedAdd(ValueA, ValueB, Weight); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::WeightedSum) { ValueC = PCGExBlend::WeightedAdd(ValueA, ValueB, Weight); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::Lerp) { ValueC = PCGExBlend::Lerp(ValueA, ValueB, Weight); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::Subtract) { ValueC = PCGExBlend::Sub(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::WeightedSubtract) { ValueC = PCGExBlend::WeightedSub(ValueA, ValueB, Weight); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::UnsignedMin) { ValueC = PCGExBlend::UnsignedMin(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::UnsignedMax) { ValueC = PCGExBlend::UnsignedMax(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::AbsoluteMin) { ValueC = PCGExBlend::AbsoluteMin(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::AbsoluteMax) { ValueC = PCGExBlend::AbsoluteMax(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::CopyOther) { ValueC = PCGExBlend::Copy(ValueB, ValueA); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::Hash) { ValueC = PCGExBlend::NaiveHash(ValueA, ValueB); }
				else if constexpr (BlendMode == EPCGExDataBlendingType::UnsignedHash) { ValueC = PCGExBlend::NaiveUnsignedHash(ValueA, ValueB); }

				// Write value

				if constexpr (std::is_same_v<RawType, WorkingType>) { WriteRange[TargetIndex] = ValueC; }
				else
				{
					if constexpr (PointProperty == EPCGPointProperties::Position) { WriteRange[TargetIndex].SetPosition(ValueC); }
					else if constexpr (PointProperty == EPCGPointProperties::Rotation) { WriteRange[TargetIndex].SetRotation(ValueC); }
					else if constexpr (PointProperty == EPCGPointProperties::Scale) { WriteRange[TargetIndex].SetScale3D(ValueC); }
				}
			}
		}

		virtual void CompleteBlending(const int32 TargetIndex, const int32 Count, const double TotalWeight) const override
		{
			if constexpr (BlendMode == EPCGExDataBlendingType::Average)
			{
				if constexpr (std::is_same_v<RawType, WorkingType>) { WriteRange[TargetIndex] = PCGExBlend::Div(WriteRange[TargetIndex], Count); }
				else
				{
					if constexpr (PointProperty == EPCGPointProperties::Position) { WriteRange[TargetIndex].SetLocation(PCGExBlend::Div(WriteRange[TargetIndex].GetLocation(), Count)); }
					else if constexpr (PointProperty == EPCGPointProperties::Rotation) { WriteRange[TargetIndex].SetRotation(PCGExBlend::Div(WriteRange[TargetIndex].GetRotation(), Count)); }
					else if constexpr (PointProperty == EPCGPointProperties::Scale) { WriteRange[TargetIndex].SetScale3D(PCGExBlend::Div(WriteRange[TargetIndex].GetScale3D(), Count)); }
				}
			}
			else if constexpr (BlendMode == EPCGExDataBlendingType::Weight)
			{
				if constexpr (std::is_same_v<RawType, WorkingType>) { WriteRange[TargetIndex] = PCGExBlend::Div(WriteRange[TargetIndex], TotalWeight); }
				else
				{
					if constexpr (PointProperty == EPCGPointProperties::Position) { WriteRange[TargetIndex].SetLocation(PCGExBlend::Div(WriteRange[TargetIndex].GetLocation(), TotalWeight)); }
					else if constexpr (PointProperty == EPCGPointProperties::Rotation) { WriteRange[TargetIndex].SetRotation(PCGExBlend::Div(WriteRange[TargetIndex].GetRotation(), TotalWeight)); }
					else if constexpr (PointProperty == EPCGPointProperties::Scale) { WriteRange[TargetIndex].SetScale3D(PCGExBlend::Div(WriteRange[TargetIndex].GetScale3D(), TotalWeight)); }
				}
			}
		}

	protected:
		TConstPCGValueRange<RawType> RangeA;
		TConstPCGValueRange<RawType> RangeB;
		TPCGValueRange<RawType> WriteRange;
		WorkingType ZeroedDefault = WorkingType{};
	};

#pragma endregion

	struct PCGEXTENDEDTOOLKIT_API FPropertiesBlender
	{
#define PCGEX_BLEND_FUNCREF(_NAME, ...) EPCGExDataBlendingType _NAME##Blending = EPCGExDataBlendingType::Weight;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_FUNCREF)
#undef PCGEX_BLEND_FUNCREF

		EPCGExDataBlendingType DefaultBlending = EPCGExDataBlendingType::Average;

		bool bRequiresPrepare = false;
		bool bHasNoBlending = false;

		FPropertiesBlender() = default;

		template <EPCGPointProperties PointProperty, typename RawType, typename WorkingType>
		TSharedPtr<FValueRangeBlender> CreateBlender(const EPCGExDataBlendingType BlendMode)
		{
			if (BlendMode == EPCGExDataBlendingType::None) { return nullptr; }

			TSharedPtr<FValueRangeBlender> Blender;

#define PCGEX_BLEND_MAKE(_NAME) case EPCGExDataBlendingType::_NAME : Blender = MakeShared<FValueRangeBlender<PointProperty, RawType, WorkingType, EPCGExDataBlendingType::_NAME>>(); break;
			switch (BlendMode)
			{
			PCGEX_FOREACH_DATABLENDMODE(PCGEX_BLEND_MAKE)
			default: break;
			}
#undef PCGEX_BLEND_MAKE

			return Blender;
		}

		void Init(const FPCGExPropertiesBlendingDetails& InDetails);

		void PrepareBlending(const int32 TargetIndex, const int32 Default) const;
		void Blend(const int32 A, const int32 B, const int32 Target, double Weight) const;
		void CompleteBlending(const int32 TargetIndex, const int32 Count, const double TotalWeight) const;

		void PrepareRangeBlending(const TArrayView<const int32>& Targets, const int32 Default) const;
		void BlendRange(const int32 From, const int32 To, const TArrayView<int32>& Targets, const TArrayView<const double>& Weights) const;
		void CompleteRangeBlending(const TArrayView<const int32>& Targets, const TArrayView<const int32>& Counts, const TArrayView<const double>& TotalWeights) const;

		void BlendRangeFromTo(const int32 From, const int32 To, const TArrayView<const int32>& Targets, const TArrayView<const double>& Weights) const;

	protected:
		TArray<TSharedPtr<FValueRangeBlender>> PoleBlenders;
		TArray<TSharedPtr<FValueRangeBlender>> Blenders;
	};
}
