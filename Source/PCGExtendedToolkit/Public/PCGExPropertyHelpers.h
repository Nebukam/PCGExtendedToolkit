// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGEx.h"
#include "PCGExBroadcast.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataAttributeTraits.h"

namespace PCGExData
{
	enum class EIOSide : uint8;
	class FFacade;
}

namespace PCGEx
{
#define PCGEX_FOREACH_SUPPORTEDFPROPERTY(MACRO)\
MACRO(FBoolProperty, bool) \
MACRO(FIntProperty, int32) \
MACRO(FInt64Property, int64) \
MACRO(FFloatProperty, float) \
MACRO(FDoubleProperty, double) \
MACRO(FStrProperty, FString) \
MACRO(FNameProperty, FName)
	//MACRO(FSoftClassProperty, FSoftClassPath) 
	//MACRO(FSoftObjectProperty, FSoftObjectPath)

#define PCGEX_FOREACH_SUPPORTEDFSTRUCT(MACRO)\
MACRO(FStructProperty, FVector2D) \
MACRO(FStructProperty, FVector) \
MACRO(FStructProperty, FVector4) \
MACRO(FStructProperty, FQuat) \
MACRO(FStructProperty, FRotator) \
MACRO(FStructProperty, FTransform)

	template <typename T>
	static bool TrySetFPropertyValue(void* InContainer, FProperty* InProperty, T InValue)
	{
		FSubSelection S = FSubSelection();
		if (InProperty->IsA<FObjectPropertyBase>())
		{
			// If input type is a soft object path, check if the target property is an object type
			// and resolve the object path
			const FSoftObjectPath Path = S.Get<T, FSoftObjectPath>(InValue);

			if (UObject* ResolvedObject = Path.TryLoad())
			{
				FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InProperty);
				if (ResolvedObject->IsA(ObjectProperty->PropertyClass))
				{
					void* PropertyContainer = ObjectProperty->ContainerPtrToValuePtr<void>(InContainer);
					ObjectProperty->SetObjectPropertyValue(PropertyContainer, ResolvedObject);
					return true;
				}
			}
		}

#define PCGEX_TRY_SET_FPROPERTY(_PTYPE, _TYPE) if(InProperty->IsA<_PTYPE>()){ _PTYPE* P = CastField<_PTYPE>(InProperty); P->SetPropertyValue_InContainer(InContainer, S.Get<T, _TYPE>(InValue)); return true;}
		PCGEX_FOREACH_SUPPORTEDFPROPERTY(PCGEX_TRY_SET_FPROPERTY)
#undef PCGEX_TRY_SET_FPROPERTY

		if (FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
		{
#define PCGEX_TRY_SET_FSTRUCT(_PTYPE, _TYPE) if (StructProperty->Struct == TBaseStructure<_TYPE>::Get()){ void* StructContainer = StructProperty->ContainerPtrToValuePtr<void>(InContainer); *reinterpret_cast<_TYPE*>(StructContainer) = S.Get<T, _TYPE>(InValue); return true; }
			PCGEX_FOREACH_SUPPORTEDFSTRUCT(PCGEX_TRY_SET_FSTRUCT)
#undef PCGEX_TRY_SET_FSTRUCT

			if (StructProperty->Struct == TBaseStructure<FPCGAttributePropertyInputSelector>::Get())
			{
				FPCGAttributePropertyInputSelector NewSelector = FPCGAttributePropertyInputSelector();
				NewSelector.Update(S.Get<T, FString>(InValue));
				void* StructContainer = StructProperty->ContainerPtrToValuePtr<void>(InContainer);
				*reinterpret_cast<FPCGAttributePropertyInputSelector*>(StructContainer) = NewSelector;
				return true;
			}
		}

		return false;
	}
}
