// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertyTypes.h"

// Macro to generate standard typed property implementations
// NOTE: OutputBuffer validity is guaranteed by InitializeOutput returning true.
// Callers must exclude properties that failed initialization from processing.
#define PCGEX_PROPERTY_IMPL(_TYPE, _NAME) \
bool FPCGExPropertyCompiled_##_NAME::InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) \
{ \
	OutputBuffer = OutputFacade->GetWritable<_TYPE>(OutputName, Value, true, PCGExData::EBufferInit::Inherit); \
	return OutputBuffer.IsValid(); \
} \
void FPCGExPropertyCompiled_##_NAME::WriteOutput(int32 PointIndex) const \
{ \
	check(OutputBuffer); \
	OutputBuffer->SetValue(PointIndex, Value); \
} \
void FPCGExPropertyCompiled_##_NAME::WriteOutputFrom(int32 PointIndex, const FPCGExPropertyCompiled* Source) const \
{ \
	check(OutputBuffer); \
	const FPCGExPropertyCompiled_##_NAME* Typed = static_cast<const FPCGExPropertyCompiled_##_NAME*>(Source); \
	OutputBuffer->SetValue(PointIndex, Typed->Value); \
} \
void FPCGExPropertyCompiled_##_NAME::CopyValueFrom(const FPCGExPropertyCompiled* Source) \
{ \
	const FPCGExPropertyCompiled_##_NAME* Typed = static_cast<const FPCGExPropertyCompiled_##_NAME*>(Source); \
	Value = Typed->Value; \
}

#pragma region Standard Types

PCGEX_PROPERTY_IMPL(FString, String)
PCGEX_PROPERTY_IMPL(FName, Name)
PCGEX_PROPERTY_IMPL(int32, Int32)
PCGEX_PROPERTY_IMPL(int64, Int64)
PCGEX_PROPERTY_IMPL(float, Float)
PCGEX_PROPERTY_IMPL(double, Double)
PCGEX_PROPERTY_IMPL(bool, Bool)
PCGEX_PROPERTY_IMPL(FVector, Vector)
PCGEX_PROPERTY_IMPL(FVector2D, Vector2)
PCGEX_PROPERTY_IMPL(FVector4, Vector4)
PCGEX_PROPERTY_IMPL(FRotator, Rotator)
PCGEX_PROPERTY_IMPL(FQuat, Quat)
PCGEX_PROPERTY_IMPL(FTransform, Transform)
PCGEX_PROPERTY_IMPL(FSoftObjectPath, SoftObjectPath)
PCGEX_PROPERTY_IMPL(FSoftClassPath, SoftClassPath)

#pragma endregion

#pragma region Color (FLinearColor -> FVector4)

bool FPCGExPropertyCompiled_Color::InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName)
{
	OutputBuffer = OutputFacade->GetWritable<FVector4>(OutputName, FVector4(Value), true, PCGExData::EBufferInit::Inherit);
	return OutputBuffer.IsValid();
}

void FPCGExPropertyCompiled_Color::WriteOutput(int32 PointIndex) const
{
	check(OutputBuffer);
	OutputBuffer->SetValue(PointIndex, FVector4(Value));
}

void FPCGExPropertyCompiled_Color::WriteOutputFrom(int32 PointIndex, const FPCGExPropertyCompiled* Source) const
{
	check(OutputBuffer);
	const FPCGExPropertyCompiled_Color* Typed = static_cast<const FPCGExPropertyCompiled_Color*>(Source);
	OutputBuffer->SetValue(PointIndex, FVector4(Typed->Value));
}

void FPCGExPropertyCompiled_Color::CopyValueFrom(const FPCGExPropertyCompiled* Source)
{
	const FPCGExPropertyCompiled_Color* Typed = static_cast<const FPCGExPropertyCompiled_Color*>(Source);
	Value = Typed->Value;
}

#pragma endregion

#undef PCGEX_PROPERTY_IMPL
