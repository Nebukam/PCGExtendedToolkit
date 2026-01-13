// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Core/PCGExUnionData.h"

namespace PCGExSampling
{
	/**
	 * Specialized union data that carries pre-computed weights.
	 * Used by sampling operations where weights are calculated during collection
	 * rather than from distances at blend time.
	 * 
	 * THREAD SAFETY:
	 * This class is designed for single-instance reuse patterns where
	 * one instance is Reset() and reused per-point during sequential processing.
	 * NOT intended for concurrent access.
	 * 
	 * USAGE:
	 *   FSamplingUnionData SampleData;
	 *   for (each target point) {
	 *       SampleData.Reset();
	 *       SampleData.WeightRange = ...;
	 *       // collect samples
	 *       SampleData.AddWeighted(element, weight);
	 *       // blend using the union data
	 *       Blender->MergeSingle(index, &SampleData, ...);
	 *   }
	 */
	class PCGEXBLENDING_API FSamplingUnionData : public PCGExData::IUnionData
	{
	public:
		/** Pre-computed weights keyed by element */
		TMap<PCGExData::FElement, double> Weights;

		FSamplingUnionData() = default;
		virtual ~FSamplingUnionData() override = default;

		/**
		 * Weight range for normalization.
		 * -2 = Don't remap weights (use raw values)
		 * -1 = Auto-detect range from collected weights
		 * >0 = Use specified range for normalization
		 */
		double WeightRange = -1;

		/**
		 * Override: Uses pre-computed weights instead of distance-based calculation.
		 */
		virtual int32 ComputeWeights(
			const TArray<const UPCGBasePointData*>& Sources,
			const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
			const PCGExData::FPoint& Target,
			const PCGExMath::IDistances* InDistances,
			TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const override;

		/**
		 * Add an element with a pre-computed weight.
		 * NOT THREAD SAFE - use in single-threaded context only.
		 * @param Element The source element reference
		 * @param InWeight Pre-computed weight value
		 */
		FORCEINLINE void AddWeighted(const PCGExData::FElement& Element, const double InWeight)
		{
			Add(Element.Index, Element.IO); // Base class method (lock-free)
			Weights.Add(Element, InWeight);
		}

		/** Get the average of all weights */
		double GetWeightAverage() const;

		/** Get the average of square roots of all weights */
		double GetSqrtWeightAverage() const;

		virtual void Reserve(int32 InSetReserve, int32 InElementReserve = 8) override;
		virtual void Reset() override;
	};
}