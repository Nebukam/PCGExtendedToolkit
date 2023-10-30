#pragma once

#include "CoreMinimal.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGExAttributesUtils.generated.h"


USTRUCT()
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeProxy
{
	GENERATED_BODY()

public:
	EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;
	FPCGMetadataAttributeBase* Attribute = nullptr;
	FName Name = NAME_None;

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
	
	static void GetAttributesProxies(const TObjectPtr<UPCGMetadata> Metadata, const TArray<const FName>& InNames, TArray<const FPCGExAttributeProxy>& OutFound, TArray<const FName>& OutMissing)
	{
		OutFound.Reset();
		OutMissing.Reset();

		TArray<FName> OutNames;
		TArray<EPCGMetadataTypes> OutTypes;
		Metadata->GetAttributes(OutNames, OutTypes);

		for (const FName& Name : InNames)
		{
			int Index = OutNames.Find(Name);
			if (Index != -1)
			{
				OutMissing.Add(Name);
			}
			else
			{
				FPCGMetadataAttributeBase* BaseAtt = Metadata->GetMutableAttribute(Name);
				OutFound.Add(FPCGExAttributeProxy{OutTypes[Index], BaseAtt, Name});
			}
		}
	}

};
