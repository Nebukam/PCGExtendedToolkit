// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExPropertyHelpers.h"

#include "CoreMinimal.h"

namespace PCGExPropertyHelpers
{
	void CopyStructProperties(const void* SourceStruct, void* TargetStruct, const UStruct* SourceStructType, const UStruct* TargetStructType)
	{
		for (TFieldIterator<FProperty> SourceIt(SourceStructType); SourceIt; ++SourceIt)
		{
			const FProperty* SourceProperty = *SourceIt;
			const FProperty* TargetProperty = TargetStructType->FindPropertyByName(SourceProperty->GetFName());
			if (!TargetProperty || SourceProperty->GetClass() != TargetProperty->GetClass()) { continue; }

			if (SourceProperty->SameType(TargetProperty))
			{
				const void* SourceValue = SourceProperty->ContainerPtrToValuePtr<void>(SourceStruct);
				void* TargetValue = TargetProperty->ContainerPtrToValuePtr<void>(TargetStruct);

				SourceProperty->CopyCompleteValue(TargetValue, SourceValue);
			}
		}
	}

	bool CopyProperties(UObject* Target, const UObject* Source, const TSet<FString>* Exclusions)
	{
		const UClass* SourceClass = Source->GetClass();
		const UClass* TargetClass = Target->GetClass();
		const UClass* CommonBaseClass = nullptr;

		if (SourceClass->IsChildOf(TargetClass)) { CommonBaseClass = TargetClass; }
		else if (TargetClass->IsChildOf(SourceClass)) { CommonBaseClass = SourceClass; }
		else
		{
			// Traverse up the hierarchy to find a shared base class
			const UClass* TempClass = SourceClass;
			while (TempClass)
			{
				if (TargetClass->IsChildOf(TempClass))
				{
					CommonBaseClass = TempClass;
					break;
				}
				TempClass = TempClass->GetSuperClass();
			}
		}

		if (!CommonBaseClass) { return false; }

		// Iterate over source properties
		for (TFieldIterator<FProperty> It(CommonBaseClass); It; ++It)
		{
			const FProperty* Property = *It;
			if (Exclusions && Exclusions->Contains(Property->GetName())) { continue; }

			// Skip properties that shouldn't be copied (like transient properties)
			if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_ConstParm | CPF_OutParm)) { continue; }

			// Copy the value from source to target
			const void* SourceValue = Property->ContainerPtrToValuePtr<void>(Source);
			void* TargetValue = Property->ContainerPtrToValuePtr<void>(Target);
			Property->CopyCompleteValue(TargetValue, SourceValue);
		}

		return true;
	}
}
