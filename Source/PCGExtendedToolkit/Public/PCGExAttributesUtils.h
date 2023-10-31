#pragma once

#include "CoreMinimal.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGExAttributesUtils.generated.h"

#define PCGEX_FOREACH_SUPPORTEDTYPES(MACRO, ...) \
MACRO(Integer32, int32, __VA_ARGS__)      \
MACRO(Integer64, int64, __VA_ARGS__)      \
MACRO(Float, float, __VA_ARGS__)      \
MACRO(Double, double, __VA_ARGS__)     \
MACRO(Vector2, FVector2D, __VA_ARGS__)  \
MACRO(Vector, FVector, __VA_ARGS__)    \
MACRO(Vector4, FVector4, __VA_ARGS__)   \
MACRO(Quaternion, FQuat, __VA_ARGS__)      \
MACRO(Transform, FTransform, __VA_ARGS__) \
MACRO(String, FString, __VA_ARGS__)    \
MACRO(Boolean, bool, __VA_ARGS__)       \
MACRO(Rotator, FRotator, __VA_ARGS__)   \
MACRO(Name, FName, __VA_ARGS__)

#define PCGEX_FOREACH_SUPPORTEDTYPES_SINGLE(MACRO, ...) \
MACRO(Integer32, int32, __VA_ARGS__)      \
MACRO(Integer64, int64, __VA_ARGS__)      \
MACRO(Float, float, __VA_ARGS__)      \
MACRO(Double, double, __VA_ARGS__)     \
MACRO(String, FString, __VA_ARGS__)    \
MACRO(Boolean, bool, __VA_ARGS__)       \
MACRO(Name, FName, __VA_ARGS__)

#define PCGEX_FOREACH_SUPPORTEDTYPES_SINGLE_SAFE(MACRO, ...) \
MACRO(Integer32, int32, __VA_ARGS__)      \
MACRO(Integer64, int64, __VA_ARGS__)      \
MACRO(Float, float, __VA_ARGS__)      \
MACRO(Double, double, __VA_ARGS__)     \
MACRO(String, FString, __VA_ARGS__)    \
MACRO(Boolean, bool, __VA_ARGS__)       

#define PCGEX_FOREACH_SUPPORTEDTYPES_2_FIELDS(MACRO, ...) \
MACRO(Vector2, FVector2D, X, Y, __VA_ARGS__)  \

#define PCGEX_FOREACH_SUPPORTEDTYPES_3_FIELDS(MACRO, ...) \
MACRO(Vector, FVector, X, Y, Z, __VA_ARGS__)    \
MACRO(Rotator, FRotator, Roll, Pitch, Yaw, __VA_ARGS__)   \
MACRO(Vector4, FVector4, X, Y, Z, __VA_ARGS__)    \
MACRO(Quaternion, FQuat, X, Y, Z, __VA_ARGS__)   \

USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeProxy
{
	GENERATED_BODY()

public:
	EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
	FPCGMetadataAttributeBase* Attribute = nullptr;

	bool IsValid() const { return Attribute == nullptr ? false : true; }
	template <typename T>
	FPCGMetadataAttribute<T>* GetTypedAttribute()
	{
		if (Attribute == nullptr) { return nullptr; }
		FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(Attribute);
		return TypedAttribute;
	}

	template <typename T>
	T GetValue(PCGMetadataValueKey ValueKey) const
	{
		if (Attribute == nullptr) { return T{}; }
		FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(Attribute);
		return TypedAttribute->GetValue(ValueKey);
	}
	
};

class PCGEXTENDEDTOOLKIT_API AttributeHelpers
{
public:
	
	static void GetAttributesProxies(const TObjectPtr<UPCGMetadata> Metadata, const TArray<FName>& InNames, TArray<FPCGExAttributeProxy>& OutFound, TArray<FName>& OutMissing)
	{
		OutFound.Reset();
		OutMissing.Reset();

		TArray<FName> OutNames;
		TArray<EPCGMetadataTypes> OutTypes;
		Metadata->GetAttributes(OutNames, OutTypes);

		for (FName Name : InNames)
		{
			int Index = OutNames.Find(Name);
			if (Index != -1)
			{
				FPCGMetadataAttributeBase* BaseAtt = Metadata->GetMutableAttribute(Name);
				FPCGExAttributeProxy NewProxy =FPCGExAttributeProxy{OutTypes[Index], BaseAtt};
				OutFound.Add(NewProxy);
			}
			else
			{
				OutMissing.Add(Name);
			}
		}
	}

};
