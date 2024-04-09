// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExData.h"

namespace PCGExData
{
#pragma region FIdxCompound

	bool FIdxCompound::ContainsIOIndex(const int32 InIOIndex)
	{
		for (const uint64 H : CompoundedPoints)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(H, A, B);
			if (A == InIOIndex) { return true; }
		}
		return false;
	}

	void FIdxCompound::ComputeWeights(const TArray<FPointIO*>& Sources, const FPCGPoint& Target, const FPCGExDistanceSettings& DistSettings)
	{
		Weights.SetNumUninitialized(CompoundedPoints.Num());

		double MaxDist = TNumericLimits<double>::Min();
		for (int i = 0; i < CompoundedPoints.Num(); i++)
		{
			uint32 IOIndex;
			uint32 PtIndex;
			PCGEx::H64(CompoundedPoints[i], IOIndex, PtIndex);

			Weights[i] = DistSettings.GetDistance(Sources[IOIndex]->GetInPoint(PtIndex), Target);
			MaxDist = FMath::Max(MaxDist, Weights[i]);
		}

		for (double& Weight : Weights) { Weight = 1 - (Weight / MaxDist); }
	}

	void FIdxCompound::ComputeWeights(const TArray<FPCGPoint>& SourcePoints, const FPCGPoint& Target, const FPCGExDistanceSettings& DistSettings)
	{
		Weights.SetNumUninitialized(CompoundedPoints.Num());

		double MaxDist = TNumericLimits<double>::Min();
		for (int i = 0; i < CompoundedPoints.Num(); i++)
		{
			Weights[i] = DistSettings.GetDistance(Target, SourcePoints[PCGEx::H64B(CompoundedPoints[i])]);
			MaxDist = FMath::Max(MaxDist, Weights[i]);
		}

		for (double& Weight : Weights) { Weight = 1 - (Weight / MaxDist); }
	}

	uint64 FIdxCompound::Add(const int32 IOIndex, const int32 PointIndex)
	{
		const uint64 H = PCGEx::H64(IOIndex, PointIndex);
		CompoundedPoints.AddUnique(H);
		return H;
	}

#pragma endregion

#pragma region FIdxCompoundList

	FIdxCompound* FIdxCompoundList::New()
	{
		FIdxCompound* NewPointCompound = new FIdxCompound();
		Compounds.Add(NewPointCompound);
		return NewPointCompound;
	}

	uint64 FIdxCompoundList::Add(const int32 Index, const int32 IOIndex, const int32 PointIndex)
	{
		return Compounds[Index]->Add(IOIndex, PointIndex);
	}

	void FIdxCompoundList::GetIOIndices(const int32 Index, TArray<int32>& OutIOIndices)
	{
		FIdxCompound* Compound = Compounds[Index];
		OutIOIndices.SetNumUninitialized(Compound->Num());
		for (int i = 0; i < OutIOIndices.Num(); i++) { OutIOIndices[i] = PCGEx::H64A(Compound->CompoundedPoints[i]); }
	}

	bool FIdxCompoundList::HasIOIndexOverlap(const int32 InIdx, const TArray<int32>& InIndices)
	{
		FIdxCompound* OtherComp = Compounds[InIdx];
		for (const int32 IOIndex : InIndices) { if (OtherComp->ContainsIOIndex(IOIndex)) { return true; } }
		return false;
	}

#pragma endregion
}
