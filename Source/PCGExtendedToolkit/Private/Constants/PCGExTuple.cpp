// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Constants/PCGExTuple.h"

#if WITH_EDITOR
#include "AnimationEditorTypes.h"
#endif // WITH_EDITOR
#include "PCGExHelpers.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Chaos/EPA.h"
#include "VerseVM/VBPVMRuntimeType.h"
#include "VerseVM/VVMVerseEnum.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Tuple

FPCGExTupleValueHeader::FPCGExTupleValueHeader()
{
	bIsHeader = true;
	EDITOR_GUID = GetTypeHash(FGuid::NewGuid());
}

FPCGMetadataAttributeBase* FPCGExTupleValueHeader::CreateAttribute(FPCGExContext* InContext, UPCGParamData* TupleData) const
{
	// Create attribute
	const FPCGMetadataAttributeBase* ExistingAttr = TupleData->Metadata->GetConstAttribute(Name);
	if (ExistingAttr)
	{
		PCGEX_LOG_INVALID_ATTR_C(InContext, Header Name, Name)
		return nullptr;
	}

	FPCGMetadataAttributeBase* NewAttribute = nullptr;

	PCGEx::ExecuteWithRightType(
		UnderlyingType, [&](auto DummyValue)
		{
			using T = decltype(DummyValue);
			T DefaultValue = T{};

			if constexpr (std::is_same_v<T, bool>) { DefaultValue = BooleanValue; }
			else if constexpr (std::is_same_v<T, float>) { DefaultValue = FloatValue; }
			else if constexpr (std::is_same_v<T, double>) { DefaultValue = DoubleValue; }
			else if constexpr (std::is_same_v<T, int32>) { DefaultValue = Integer32Value; }
			else if constexpr (std::is_same_v<T, FVector2D>) { DefaultValue = Vector2Value; }
			else if constexpr (std::is_same_v<T, FVector>) { DefaultValue = VectorValue; }
			else if constexpr (std::is_same_v<T, FVector4>) { DefaultValue = Type == EPCGExTupleTypes::Color ? FVector4(ColorValue) : Vector4Value; }
			else if constexpr (std::is_same_v<T, FTransform>) { DefaultValue = TransformValue; }
			else if constexpr (std::is_same_v<T, FString>) { DefaultValue = StringValue; }
			else if constexpr (std::is_same_v<T, FName>) { DefaultValue = NameValue; }
			else if constexpr (std::is_same_v<T, FRotator>) { DefaultValue = RotatorValue; }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { DefaultValue = SoftObjectPathValue; }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { DefaultValue = SoftClassPathValue; }

			NewAttribute = TupleData->Metadata->CreateAttribute<T>(Name, DefaultValue, true, true);
		});

	return NewAttribute;
}

#if WITH_EDITOR
void UPCGExTupleSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	TArray<int32> Guids;
	TMap<int32, int32> Order;
	Guids.Reserve(Composition.Num());
	Order.Reserve(Composition.Num());

	bool bReordered = false;
	for (FPCGExTupleValueHeader& Header : Composition)
	{
		int32 Index = Guids.Add(Header.EDITOR_GUID);

		if (Header.Order != Index)
		{
			Header.Order = Index;
			bReordered = true;
		}

		Order.Add(GetTypeHash(Header.EDITOR_GUID), Index);

		switch (Header.Type)
		{
		case EPCGExTupleTypes::Float:
			Header.UnderlyingType = EPCGMetadataTypes::Float;
			break;
		case EPCGExTupleTypes::Double:
			Header.UnderlyingType = EPCGMetadataTypes::Double;
			break;
		case EPCGExTupleTypes::Integer32:
			Header.UnderlyingType = EPCGMetadataTypes::Integer32;
			break;
		case EPCGExTupleTypes::Vector2:
			Header.UnderlyingType = EPCGMetadataTypes::Vector2;
			break;
		case EPCGExTupleTypes::Vector:
			Header.UnderlyingType = EPCGMetadataTypes::Vector;
			break;
		case EPCGExTupleTypes::Vector4:
			Header.UnderlyingType = EPCGMetadataTypes::Vector4;
			break;
		case EPCGExTupleTypes::Color:
			Header.UnderlyingType = EPCGMetadataTypes::Vector4;
			break;
		case EPCGExTupleTypes::Transform:
			Header.UnderlyingType = EPCGMetadataTypes::Transform;
			break;
		case EPCGExTupleTypes::String:
			Header.UnderlyingType = EPCGMetadataTypes::String;
			break;
		case EPCGExTupleTypes::Boolean:
			Header.UnderlyingType = EPCGMetadataTypes::Boolean;
			break;
		case EPCGExTupleTypes::Rotator:
			Header.UnderlyingType = EPCGMetadataTypes::Rotator;
			break;
		case EPCGExTupleTypes::Name:
			Header.UnderlyingType = EPCGMetadataTypes::Name;
			break;
		case EPCGExTupleTypes::SoftObjectPath:
			Header.UnderlyingType = EPCGMetadataTypes::SoftObjectPath;
			break;
		case EPCGExTupleTypes::SoftClassPath:
			Header.UnderlyingType = EPCGMetadataTypes::SoftClassPath;
			break;
		}
	}

	// First ensure all bodies have valid GUID from the composition, and the same number
	for (FPCGExTupleBody& Body : Values)
	{
		if (Body.Values.Num() > Guids.Num())
		{
			// Find members that needs to be removed
			int32 WriteIndex = 0;
			for (FPCGExTupleValue& Value : Body.Values)
			{
				if (!Order.Contains(Value.EDITOR_GUID)) { continue; }
				Body.Values[WriteIndex++] = MoveTemp(Value);
			}
			Body.Values.SetNum(WriteIndex);
		}
		else if (Body.Values.Num() < Guids.Num())
		{
			// Need to adjust size and assign Guids
			const int32 StartIndex = Body.Values.Num();
			PCGEx::InitArray(Body.Values, Guids.Num());
			for (int i = StartIndex; i < Guids.Num(); ++i) { Body.Values[i].EDITOR_GUID = Guids[i]; }
		}
		else if (bReordered)
		{
			// Reorder the values
			Body.Values.Sort([&](const FPCGExTupleValue& A, const FPCGExTupleValue& B) { return Order[A.EDITOR_GUID] < Order[B.EDITOR_GUID]; });
		}

		for (int i = 0; i < Body.Values.Num(); i++)
		{
			FPCGExTupleValue& Value = Body.Values[i];
			Value.Name = Composition[i].Name;
			Value.Type = Composition[i].Type;
		}
	}

	(void)MarkPackageDirty();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExTupleSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExTupleSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("Tuple"), TEXT("Tuple."), Required)
	return PinProperties;
}

FPCGElementPtr UPCGExTupleSettings::CreateElement() const { return MakeShared<FPCGExTupleElement>(); }


bool FPCGExTupleElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(Tuple)

	UPCGParamData* TupleData = Context->ManagedObjects->New<UPCGParamData>();

	TArray<FPCGMetadataAttributeBase*> Attributes;
	TArray<int64> Keys;

	Attributes.Reserve(Settings->Composition.Num());
	Keys.Reserve(Settings->Composition.Num());

	for (const FPCGExTupleValueHeader& Header : Settings->Composition) { Attributes.Add(Header.CreateAttribute(Context, TupleData)); }
	// Create all keys
	for (int i = 0; i < Settings->Values.Num(); ++i) { Keys.Add(TupleData->Metadata->AddEntry()); }
	for (int i = 0; i < Settings->Composition.Num(); ++i)
	{
		const FPCGExTupleValueHeader& Header = Settings->Composition[i];
		FPCGMetadataAttributeBase* Attribute = Attributes[i];

		if (!Attribute) { continue; }

		PCGEx::ExecuteWithRightType(
			Header.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(Attribute);

				for (int k = 0; k < Keys.Num(); k++)
				{
					T Value = T{};
					const FPCGExTupleValue& V = Settings->Values[k].Values[i];
					if (V.bUseDefaultValue) { continue; }

					if constexpr (std::is_same_v<T, bool>) { Value = V.BooleanValue; }
					else if constexpr (std::is_same_v<T, float>) { Value = V.FloatValue; }
					else if constexpr (std::is_same_v<T, double>) { Value = V.DoubleValue; }
					else if constexpr (std::is_same_v<T, int32>) { Value = V.Integer32Value; }
					else if constexpr (std::is_same_v<T, FVector2D>) { Value = V.Vector2Value; }
					else if constexpr (std::is_same_v<T, FVector>) { Value = V.VectorValue; }
					else if constexpr (std::is_same_v<T, FVector4>) { Value = Header.Type == EPCGExTupleTypes::Color ? FVector4(V.ColorValue) : V.Vector4Value; }
					else if constexpr (std::is_same_v<T, FTransform>) { Value = V.TransformValue; }
					else if constexpr (std::is_same_v<T, FString>) { Value = V.StringValue; }
					else if constexpr (std::is_same_v<T, FName>) { Value = V.NameValue; }
					else if constexpr (std::is_same_v<T, FRotator>) { Value = V.RotatorValue; }
					else if constexpr (std::is_same_v<T, FSoftObjectPath>) { Value = V.SoftObjectPathValue; }
					else if constexpr (std::is_same_v<T, FSoftClassPath>) { Value = V.SoftClassPathValue; }

					TypedAttribute->SetValue(Keys[k], Value);
				}
			});
	}

	FPCGTaggedData& StagedData = Context->StageOutput(TupleData, true);
	StagedData.Pin = FName("Tuple");

	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
