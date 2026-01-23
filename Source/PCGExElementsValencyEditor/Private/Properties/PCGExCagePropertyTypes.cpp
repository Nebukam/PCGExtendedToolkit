// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Properties/PCGExCagePropertyTypes.h"
#include "PCGExPropertyTypes.h"
#include "Collections/PCGExCollectionPropertyTypes.h"

// Macro to generate standard typed property implementations
#define PCGEX_CAGE_PROPERTY_EDITOR_IMPL(_NAME) \
bool UPCGExCageProperty_##_NAME::CompileProperty(FInstancedStruct& OutCompiled) const \
{ \
	FPCGExPropertyCompiled_##_NAME Compiled; \
	Compiled.PropertyName = GetEffectivePropertyName(); \
	Compiled.Value = Value; \
	OutCompiled.InitializeAs<FPCGExPropertyCompiled_##_NAME>(MoveTemp(Compiled)); \
	return true; \
} \
UScriptStruct* UPCGExCageProperty_##_NAME::GetCompiledStructType() const \
{ \
	return FPCGExPropertyCompiled_##_NAME::StaticStruct(); \
}

#pragma region UPCGExCageProperty_AssetCollection

bool UPCGExCageProperty_AssetCollection::CompileProperty(FInstancedStruct& OutCompiled) const
{
	FPCGExPropertyCompiled_AssetCollection Compiled;
	Compiled.PropertyName = GetEffectivePropertyName();
	Compiled.AssetCollection = AssetCollection;

	OutCompiled.InitializeAs<FPCGExPropertyCompiled_AssetCollection>(MoveTemp(Compiled));
	return true;
}

UScriptStruct* UPCGExCageProperty_AssetCollection::GetCompiledStructType() const
{
	return FPCGExPropertyCompiled_AssetCollection::StaticStruct();
}

#pragma endregion

#pragma region Atomic Typed Properties

PCGEX_CAGE_PROPERTY_EDITOR_IMPL(String)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Name)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Int32)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Int64)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Float)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Double)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Bool)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Vector)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Vector2)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Vector4)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Color)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Rotator)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Quat)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(Transform)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(SoftObjectPath)
PCGEX_CAGE_PROPERTY_EDITOR_IMPL(SoftClassPath)

#pragma endregion

#undef PCGEX_CAGE_PROPERTY_EDITOR_IMPL
