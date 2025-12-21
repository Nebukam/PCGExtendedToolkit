// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExShapeConfigBase.h"
#include "Details/PCGExSettingsDetails.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExShapeConfigBase, Resolution, double, ResolutionInput, ResolutionAttribute, ResolutionConstant);
PCGEX_SETTING_VALUE_IMPL(FPCGExShapeConfigBase, ResolutionVector, FVector, ResolutionInput, ResolutionAttribute, ResolutionConstantVector)

void FPCGExShapeConfigBase::Init()
{
	FQuat A = FQuat::Identity;
	FQuat B = FQuat::Identity;
	switch (SourceAxis)
	{
	case EPCGExAxisAlign::Forward: A = FRotationMatrix::MakeFromX(FVector::ForwardVector).ToQuat().Inverse();
		break;
	case EPCGExAxisAlign::Backward: A = FRotationMatrix::MakeFromX(FVector::BackwardVector).ToQuat().Inverse();
		break;
	case EPCGExAxisAlign::Right: A = FRotationMatrix::MakeFromX(FVector::RightVector).ToQuat().Inverse();
		break;
	case EPCGExAxisAlign::Left: A = FRotationMatrix::MakeFromX(FVector::LeftVector).ToQuat().Inverse();
		break;
	case EPCGExAxisAlign::Up: A = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat().Inverse();
		break;
	case EPCGExAxisAlign::Down: A = FRotationMatrix::MakeFromX(FVector::DownVector).ToQuat().Inverse();
		break;
	}

	switch (TargetAxis)
	{
	case EPCGExAxisAlign::Forward: B = FRotationMatrix::MakeFromX(FVector::ForwardVector).ToQuat();
		break;
	case EPCGExAxisAlign::Backward: B = FRotationMatrix::MakeFromX(FVector::BackwardVector).ToQuat();
		break;
	case EPCGExAxisAlign::Right: B = FRotationMatrix::MakeFromX(FVector::RightVector).ToQuat();
		break;
	case EPCGExAxisAlign::Left: B = FRotationMatrix::MakeFromX(FVector::LeftVector).ToQuat();
		break;
	case EPCGExAxisAlign::Up: B = FRotationMatrix::MakeFromX(FVector::UpVector).ToQuat();
		break;
	case EPCGExAxisAlign::Down: B = FRotationMatrix::MakeFromX(FVector::DownVector).ToQuat();
		break;
	}

	LocalTransform = FTransform(A * B, FVector::ZeroVector, FVector::OneVector);
}
