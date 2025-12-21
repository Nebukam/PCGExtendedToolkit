// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSamplingUnionData.h"

#include "Containers/PCGExIndexLookup.h"

namespace PCGExSampling
{
	int32 FSampingUnionData::ComputeWeights(
		const TArray<const UPCGBasePointData*>& Sources,
		const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
		const PCGExData::FPoint& Target,
		const PCGExMath::FDistances* InDistanceDetails,
		TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const
	{
		const int32 NumElements = Elements.Num();
		OutWeightedPoints.Reset(NumElements);

		double TotalWeight = 0;
		int32 Index = 0;

		if (WeightRange == -2)
		{
			// Don't remap weight
			for (const PCGExData::FElement& Element : Elements)
			{
				const int32 IOIdx = IdxLookup->Get(Element.IO);
				if (IOIdx == -1) { continue; }

				const double Weight = Weights[Element];
				OutWeightedPoints.Emplace(Element.Index, Weight, IOIdx);
				TotalWeight += Weight;
				Index++;
			}
		}
		else if (WeightRange == -1)
		{
			double InternalRange = 0;
			for (const TPair<PCGExData::FElement, double>& W : Weights) { InternalRange = FMath::Max(InternalRange, W.Value); }

			// Remap weight to available max
			for (const PCGExData::FElement& Element : Elements)
			{
				const int32 IOIdx = IdxLookup->Get(Element.IO);
				if (IOIdx == -1) { continue; }

				const double Weight = 1 - (Weights[Element] / InternalRange);
				OutWeightedPoints.Emplace(Element.Index, Weight, IOIdx);
				TotalWeight += Weight;
				Index++;
			}
		}
		else
		{
			// Remap weight to specified max
			for (const PCGExData::FElement& Element : Elements)
			{
				const int32 IOIdx = IdxLookup->Get(Element.IO);
				if (IOIdx == -1) { continue; }

				const double Weight = 1 - (Weights[Element] / WeightRange);
				OutWeightedPoints.Emplace(Element.Index, Weight, IOIdx);
				TotalWeight += Weight;
				Index++;
			}
		}

		//OutWeightedPoints.Sort([](const PCGExData::FWeightedPoint& A, const PCGExData::FWeightedPoint& B) { return A.Weight < B.Weight; });

		if (Index == 0) { return 0; }
		if (TotalWeight == 0)
		{
			const double FixedWeight = 1 / static_cast<double>(Index);
			for (PCGExData::FWeightedPoint& P : OutWeightedPoints) { P.Weight = FixedWeight; }
			return Index;
		}

		// Normalize weights
		//for (PCGExData::FWeightedPoint& P : OutWeightedPoints) { P.Weight /= TotalWeight; }
		return Index;
	}

	void FSampingUnionData::AddWeighted(const PCGExData::FElement& Element, const double InWeight)
	{
		FWriteScopeLock WriteScopeLock(UnionLock);
		Add_Unsafe(Element.Index, Element.IO);
		Weights.Add(Element, InWeight);
	}

	double FSampingUnionData::GetWeightAverage() const
	{
		if (Weights.Num() == 0) { return 0; }

		double Average = 0;
		for (const TPair<PCGExData::FElement, double>& Pair : Weights) { Average += Pair.Value; }
		return Average / Weights.Num();
	}

	double FSampingUnionData::GetSqrtWeightAverage() const
	{
		if (Weights.Num() == 0) { return 0; }

		double Average = 0;
		for (const TPair<PCGExData::FElement, double>& Pair : Weights) { Average += FMath::Sqrt(Pair.Value); }
		return Average / Weights.Num();
	}

	void FSampingUnionData::Reserve(const int32 InSetReserve, const int32 InElementReserve)
	{
		IUnionData::Reserve(InSetReserve, InElementReserve);
		if (InElementReserve > 8) { Weights.Reserve(InElementReserve); }
	}

	void FSampingUnionData::Reset()
	{
		IUnionData::Reset();
		Weights.Reset();
	}
}
