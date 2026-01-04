// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Noises/PCGExNoiseGabor.h"
#include "Helpers/PCGExNoise3DMath.h"
#include "Containers/PCGExManagedObjects.h"

using namespace PCGExNoise3D::Math;

FORCEINLINE double FPCGExNoiseGabor::GaborKernel(const FVector& Offset, const double K, const double A) const
{
	const double R2 = Offset.SizeSquared();
	if (R2 > KernelRadius * KernelRadius) { return 0.0; }

	// Gaussian envelope
	const double Gaussian = FMath::Exp(-PI * A * A * R2);

	// Sinusoidal carrier
	const double Phase = 2.0 * PI * K * FVector::DotProduct(Direction, Offset);

	return Gaussian * FMath::Cos(Phase);
}

FORCEINLINE FVector FPCGExNoiseGabor::RandomImpulse(const int32 CellX, const int32 CellY, const int32 CellZ, const int32 Idx) const
{
	const uint32 H1 = Hash32(CellX + Seed, CellY + Idx, CellZ);
	const uint32 H2 = Hash32(CellX + Idx, CellY + Seed, CellZ);
	const uint32 H3 = Hash32(CellX, CellY, CellZ + Seed + Idx);

	return FVector(
		Hash32ToDouble01(H1),
		Hash32ToDouble01(H2),
		Hash32ToDouble01(H3)
	);
}

double FPCGExNoiseGabor::GenerateRaw(const FVector& Position) const
{
	const int32 CellX = FastFloor(Position.X);
	const int32 CellY = FastFloor(Position.Y);
	const int32 CellZ = FastFloor(Position.Z);

	const double K = Frequency * Bandwidth;
	const double A = Bandwidth;

	double Sum = 0.0;
	int32 Count = 0;

	// Search cells within kernel radius
	const int32 SearchRadius = static_cast<int32>(FMath::CeilToInt(KernelRadius));

	for (int32 DZ = -SearchRadius; DZ <= SearchRadius; ++DZ)
	{
		for (int32 DY = -SearchRadius; DY <= SearchRadius; ++DY)
		{
			for (int32 DX = -SearchRadius; DX <= SearchRadius; ++DX)
			{
				const int32 NX = CellX + DX;
				const int32 NY = CellY + DY;
				const int32 NZ = CellZ + DZ;

				// Process impulses in this cell
				for (int32 i = 0; i < ImpulsesPerCell; ++i)
				{
					const FVector ImpulseOffset = RandomImpulse(NX, NY, NZ, i);
					const FVector ImpulsePos = FVector(NX, NY, NZ) + ImpulseOffset;
					const FVector Delta = Position - ImpulsePos;

					// Random weight for this impulse
					const uint32 HW = Hash32(NX + i, NY + i, NZ + Seed);
					const double Weight = Hash32ToDouble(HW);

					Sum += Weight * GaborKernel(Delta, K, A);
					Count++;
				}
			}
		}
	}

	// Normalize
	return Sum * FMath::Sqrt(1.0 / FMath::Max(Count, 1));
}

TSharedPtr<FPCGExNoise3DOperation> UPCGExNoise3DFactoryGabor::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(NoiseGabor)
	PCGEX_FORWARD_NOISE3D_CONFIG

	NewOperation->Frequency = Config.Frequency;
	NewOperation->Seed = Config.Seed;
	NewOperation->Direction = Config.Direction.GetSafeNormal();
	NewOperation->Bandwidth = Config.Bandwidth;
	NewOperation->ImpulsesPerCell = Config.ImpulsesPerCell;
	NewOperation->KernelRadius = Config.KernelRadius;
	NewOperation->bInvert = Config.bInvert;
	NewOperation->Octaves = 1;

	return NewOperation;
}

PCGEX_NOISE3D_FACTORY_BOILERPLATE_IMPL(Gabor, {})

UPCGExFactoryData* UPCGExNoise3DGaborProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExNoise3DFactoryGabor* NewFactory = InContext->ManagedObjects->New<UPCGExNoise3DFactoryGabor>();
	PCGEX_FORWARD_NOISE3D_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}