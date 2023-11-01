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
MACRO(Vector2, FVector2D, X, Y, __VA_ARGS__)

#define PCGEX_FOREACH_SUPPORTEDTYPES_3_FIELDS(MACRO, ...) \
MACRO(Vector, FVector, X, Y, Z, __VA_ARGS__)    \
MACRO(Rotator, FRotator, Roll, Pitch, Yaw, __VA_ARGS__)   \
MACRO(Vector4, FVector4, X, Y, Z, __VA_ARGS__)    \
MACRO(Quaternion, FQuat, X, Y, Z, __VA_ARGS__)

UENUM(BlueprintType)
enum class EPCGExTypeCategory : uint8
{
	Unsupported = 0,
	Num,
	Lengthy,
	Complex,
	Composite,
	String,
};

struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeProxy
{
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
	static EPCGExTypeCategory GetCategory(const EPCGMetadataTypes Type)
	{
		switch (Type)
		{
		case EPCGMetadataTypes::Float:
		case EPCGMetadataTypes::Double:
		case EPCGMetadataTypes::Integer32:
		case EPCGMetadataTypes::Integer64:
		case EPCGMetadataTypes::Boolean:
			return EPCGExTypeCategory::Num;
			break;
		case EPCGMetadataTypes::Vector2:
		case EPCGMetadataTypes::Vector:
		case EPCGMetadataTypes::Vector4:
			return EPCGExTypeCategory::Lengthy;
			break;
		case EPCGMetadataTypes::Quaternion:
		case EPCGMetadataTypes::Transform:
			return EPCGExTypeCategory::Composite;
			break;
		case EPCGMetadataTypes::Rotator:
			return EPCGExTypeCategory::Complex;
			break;
		case EPCGMetadataTypes::String:
		case EPCGMetadataTypes::Name:
			return EPCGExTypeCategory::String;
			break;
		case EPCGMetadataTypes::Count:
		case EPCGMetadataTypes::Unknown:
		default: return EPCGExTypeCategory::Unsupported;
		}

		return EPCGExTypeCategory::Unsupported;
	}

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
				FPCGExAttributeProxy NewProxy = FPCGExAttributeProxy{OutTypes[Index], BaseAtt};
				OutFound.Add(NewProxy);
			}
			else
			{
				OutMissing.Add(Name);
			}
		}
	}

	static bool TryGetAttributeType(const TObjectPtr<UPCGMetadata> Metadata, FName AttributeName, EPCGMetadataTypes& OutType)
	{
		TArray<FName> OutNames;
		TArray<EPCGMetadataTypes> OutTypes;
		Metadata->GetAttributes(OutNames, OutTypes);

		for (int i = 0; i < OutNames.Num(); i++)
		{
			if (AttributeName == OutNames[i])
			{
				OutType = OutTypes[i];
				return true;
			}
		}

		return false;
	}

	static bool ValidateAttribute(const TObjectPtr<UPCGMetadata> Metadata, FName AttributeName, EPCGMetadataTypes DesiredType, bool& OutExist, bool& OutExpectedType)
	{
		TArray<FName> OutNames;
		TArray<EPCGMetadataTypes> OutTypes;
		Metadata->GetAttributes(OutNames, OutTypes);

		OutExist = false;
		OutExpectedType = false;

		for (int i = 0; i < OutNames.Num(); i++)
		{
			if (AttributeName == OutNames[i])
			{
				OutExist = true;
				if (OutTypes[i] == DesiredType)
				{
					OutExpectedType = true;
					return true;
				}
				return false;
			}
		}

		return false;
	}
};
