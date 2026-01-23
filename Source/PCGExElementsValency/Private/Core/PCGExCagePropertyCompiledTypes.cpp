// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExCagePropertyCompiledTypes.h"

// Macro to generate standard typed property implementations
#define PCGEX_CAGE_PROPERTY_IMPL(_TYPE, _NAME) \
bool FPCGExCagePropertyCompiled_##_NAME::InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) \
{ \
	OutputBuffer = OutputFacade->GetWritable<_TYPE>(OutputName, Value, true, PCGExData::EBufferInit::Inherit); \
	return OutputBuffer.IsValid(); \
} \
void FPCGExCagePropertyCompiled_##_NAME::WriteOutput(int32 PointIndex) const \
{ \
	if (OutputBuffer) { OutputBuffer->SetValue(PointIndex, Value); } \
} \
void FPCGExCagePropertyCompiled_##_NAME::CopyValueFrom(const FPCGExPropertyCompiled* Source) \
{ \
	if (const FPCGExCagePropertyCompiled_##_NAME* Typed = static_cast<const FPCGExCagePropertyCompiled_##_NAME*>(Source)) \
	{ \
		Value = Typed->Value; \
	} \
}

#pragma region Standard Types

PCGEX_CAGE_PROPERTY_IMPL(FString, String)
PCGEX_CAGE_PROPERTY_IMPL(FName, Name)
PCGEX_CAGE_PROPERTY_IMPL(int32, Int32)
PCGEX_CAGE_PROPERTY_IMPL(int64, Int64)
PCGEX_CAGE_PROPERTY_IMPL(float, Float)
PCGEX_CAGE_PROPERTY_IMPL(double, Double)
PCGEX_CAGE_PROPERTY_IMPL(bool, Bool)
PCGEX_CAGE_PROPERTY_IMPL(FVector, Vector)
PCGEX_CAGE_PROPERTY_IMPL(FVector2D, Vector2)
PCGEX_CAGE_PROPERTY_IMPL(FVector4, Vector4)
PCGEX_CAGE_PROPERTY_IMPL(FRotator, Rotator)
PCGEX_CAGE_PROPERTY_IMPL(FQuat, Quat)
PCGEX_CAGE_PROPERTY_IMPL(FTransform, Transform)
PCGEX_CAGE_PROPERTY_IMPL(FSoftObjectPath, SoftObjectPath)
PCGEX_CAGE_PROPERTY_IMPL(FSoftClassPath, SoftClassPath)

#pragma endregion

#pragma region Color (FLinearColor -> FVector4)

bool FPCGExCagePropertyCompiled_Color::InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName)
{
	OutputBuffer = OutputFacade->GetWritable<FVector4>(OutputName, FVector4(Value), true, PCGExData::EBufferInit::Inherit);
	return OutputBuffer.IsValid();
}

void FPCGExCagePropertyCompiled_Color::WriteOutput(int32 PointIndex) const
{
	if (OutputBuffer) { OutputBuffer->SetValue(PointIndex, FVector4(Value)); }
}

void FPCGExCagePropertyCompiled_Color::CopyValueFrom(const FPCGExPropertyCompiled* Source)
{
	if (const FPCGExCagePropertyCompiled_Color* Typed = static_cast<const FPCGExCagePropertyCompiled_Color*>(Source))
	{
		Value = Typed->Value;
	}
}

#pragma endregion

#undef PCGEX_CAGE_PROPERTY_IMPL
