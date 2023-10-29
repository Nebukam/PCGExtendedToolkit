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
	const FName& Name = FName{};

	bool IsValid() const { return Attribute == nullptr ? false : true; }

	template <typename T>
	FPCGMetadataAttribute<T>* GetTypedAttribute(TObjectPtr<UPCGMetadata> Metadata)
	{
		if (Attribute == nullptr) { return nullptr; }
		FPCGMetadataAttribute<T>* TypedAttribute = Cast<FPCGMetadataAttribute<T>*>(Attribute);
		return TypedAttribute;
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
