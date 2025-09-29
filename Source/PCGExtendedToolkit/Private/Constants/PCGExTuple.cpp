// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Constants/PCGExTuple.h"

#include "PCGExHelpers.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Tuple

EPCGMetadataTypes PCGExTuple::GetMetadataType(const EPCGExTupleTypes Type)
{
	switch (Type)
	{
	case EPCGExTupleTypes::Float:
		return EPCGMetadataTypes::Float;
	case EPCGExTupleTypes::Double:
		return EPCGMetadataTypes::Double;
	case EPCGExTupleTypes::Integer32:
		return EPCGMetadataTypes::Integer32;
	case EPCGExTupleTypes::Vector2:
		return EPCGMetadataTypes::Vector2;
	case EPCGExTupleTypes::Vector:
		return EPCGMetadataTypes::Vector;
	case EPCGExTupleTypes::Vector4:
		return EPCGMetadataTypes::Vector4;
	case EPCGExTupleTypes::Color:
		return EPCGMetadataTypes::Vector4;
	case EPCGExTupleTypes::Transform:
		return EPCGMetadataTypes::Transform;
	case EPCGExTupleTypes::String:
		return EPCGMetadataTypes::String;
	case EPCGExTupleTypes::Boolean:
		return EPCGMetadataTypes::Boolean;
	case EPCGExTupleTypes::Rotator:
		return EPCGMetadataTypes::Rotator;
	case EPCGExTupleTypes::Name:
		return EPCGMetadataTypes::Name;
	case EPCGExTupleTypes::SoftObjectPath:
		return EPCGMetadataTypes::SoftObjectPath;
	case EPCGExTupleTypes::SoftClassPath:
		return EPCGMetadataTypes::SoftClassPath;
	}

	return EPCGMetadataTypes::Unknown;
}

#define PCGEX_FOREACH_TUPLETYPE(MACRO) \
MACRO(float, Float) \
MACRO(double, Double) \
MACRO(int32, Integer32) \
MACRO(FVector2D, Vector2) \
MACRO(FVector, Vector) \
MACRO(FVector4, Vector4) \
MACRO(FLinearColor, Color) \
MACRO(FTransform, Transform) \
MACRO(FString, String) \
MACRO(bool, Boolean) \
MACRO(FRotator, Rotator) \
MACRO(FName, Name) \
MACRO(FSoftObjectPath, SoftObjectPath) \
MACRO(FSoftClassPath, SoftClassPath) 

FPCGExTupleValueHeader::FPCGExTupleValueHeader()
{
	HeaderId = GetTypeHash(FGuid::NewGuid());
	DefaultData.InitializeAs<FPCGExTupleValueWrapFloat>();
}

void FPCGExTupleValueHeader::SanitizeEntry(TInstancedStruct<FPCGExTupleValueWrap>& InData) const
{
	const FPCGExTupleValueWrap* HeaderData = DefaultData.GetPtr<FPCGExTupleValueWrap>();
	FPCGExTupleValueWrap* CurrentData = InData.GetMutablePtr<FPCGExTupleValueWrap>();

	if (!HeaderData) { return; }
	if (CurrentData && CurrentData->GetValueType() == HeaderData->GetValueType())
	{
		CurrentData->HeaderId = HeaderId;
		return;
	}

#define PCGEX_INIT_TUPLE_ENTRY(_TYPE, _NAME)\
	case EPCGExTupleTypes::_NAME:\
	InData.InitializeAs<FPCGExTupleValueWrap##_NAME>();\
	InData.GetMutable<FPCGExTupleValueWrap##_NAME>().Value = DefaultData.Get<FPCGExTupleValueWrap##_NAME>().Value;\
	break;
	
	switch (HeaderData->GetValueType())
	{
		PCGEX_FOREACH_TUPLETYPE(PCGEX_INIT_TUPLE_ENTRY)
	}

	CurrentData = InData.GetMutablePtr<FPCGExTupleValueWrap>();
	if (CurrentData)
	{
		CurrentData->HeaderId = HeaderId;
	}
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

	const FPCGExTupleValueWrap* CurrentData = DefaultData.GetPtr<FPCGExTupleValueWrap>();

	if (!CurrentData) { return nullptr; }

	PCGEx::ExecuteWithRightType(
		UnderlyingType, [&](auto DummyValue)
		{
			using T = decltype(DummyValue);
			T DefaultValue = T{};

			if constexpr (std::is_same_v<T, bool>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapBoolean>().Value; }
			else if constexpr (std::is_same_v<T, float>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapFloat>().Value; }
			else if constexpr (std::is_same_v<T, double>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapDouble>().Value; }
			else if constexpr (std::is_same_v<T, int32>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapInteger32>().Value; }
			else if constexpr (std::is_same_v<T, FVector2D>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapVector2>().Value; }
			else if constexpr (std::is_same_v<T, FVector>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapVector>().Value; }
			else if constexpr (std::is_same_v<T, FVector4>)
			{
				DefaultValue =
					CurrentData->GetValueType() == EPCGExTupleTypes::Color ?
						FVector4(DefaultData.Get<FPCGExTupleValueWrapColor>().Value)
						: DefaultData.Get<FPCGExTupleValueWrapVector4>().Value;
			}
			else if constexpr (std::is_same_v<T, FTransform>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapTransform>().Value; }
			else if constexpr (std::is_same_v<T, FString>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapString>().Value; }
			else if constexpr (std::is_same_v<T, FName>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapName>().Value; }
			else if constexpr (std::is_same_v<T, FRotator>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapRotator>().Value; }
			else if constexpr (std::is_same_v<T, FSoftObjectPath>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapSoftObjectPath>().Value; }
			else if constexpr (std::is_same_v<T, FSoftClassPath>) { DefaultValue = DefaultData.Get<FPCGExTupleValueWrapSoftClassPath>().Value; }

			NewAttribute = TupleData->Metadata->CreateAttribute<T>(Name, DefaultValue, true, true);
		});

	return NewAttribute;
}

#if WITH_EDITOR
void UPCGExTupleSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTupleSettings::PostEditChangeProperty);

	TArray<int32> Guids;
	TMap<int32, int32> Order;
	Guids.Reserve(Composition.Num());
	Order.Reserve(Composition.Num());

	bool bReordered = false;
	for (FPCGExTupleValueHeader& Header : Composition)
	{
		int32 Index = Guids.Add(Header.HeaderId);

		if (Header.Order != Index)
		{
			Header.Order = Index;
			bReordered = true;
		}

		Order.Add(Header.HeaderId, Index);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTupleSettings::BodyUpdate);
		// First ensure all bodies have valid GUID from the composition, and the same number
		for (FPCGExTupleBody& Body : Values)
		{
			if (Body.Row.Num() > Guids.Num())
			{
				// Find members that needs to be removed
				int32 WriteIndex = 0;

				for (TInstancedStruct<FPCGExTupleValueWrap>& Value : Body.Row)
				{
					const FPCGExTupleValueWrap* ValuePtr = Value.GetPtr();
					if (ValuePtr && !Order.Contains(ValuePtr->HeaderId)) { continue; }
					Body.Row[WriteIndex++] = MoveTemp(Value);
				}

				Body.Row.SetNum(WriteIndex);
			}
			else if (Body.Row.Num() < Guids.Num())
			{
				// Need to adjust size and assign Guids
				const int32 StartIndex = Body.Row.Num();
				PCGEx::InitArray(Body.Row, Guids.Num());
				for (int i = StartIndex; i < Guids.Num(); ++i)
				{
					Composition[i].SanitizeEntry(Body.Row[i]);
				}
			}
			else if (bReordered)
			{
				// Reorder the values
				Body.Row.Sort(
					[&](const TInstancedStruct<FPCGExTupleValueWrap>& A, const TInstancedStruct<FPCGExTupleValueWrap>& B)
					{
						const FPCGExTupleValueWrap* APtr = A.GetPtr<FPCGExTupleValueWrap>();
						const FPCGExTupleValueWrap* BPtr = B.GetPtr<FPCGExTupleValueWrap>();
						if (!APtr) { return false; }
						if (!BPtr) { return true; }
						return Order[APtr->HeaderId] < Order[BPtr->HeaderId];
					});
			}
		}
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTupleSettings::HeaderAndTypeUpdate);
		// Enforce header types on rows

		for (int i = 0; i < Composition.Num(); i++)
		{
			FPCGExTupleValueHeader& Header = Composition[i];
			const FPCGExTupleValueWrap* HeaderData = Header.DefaultData.GetPtr<FPCGExTupleValueWrap>();

			if (!HeaderData) { continue; }

			const EPCGExTupleTypes HeaderType = HeaderData->GetValueType();
			Header.UnderlyingType = PCGExTuple::GetMetadataType(HeaderType);

			for (FPCGExTupleBody& Body : Values) { Header.SanitizeEntry(Body.Row[i]); }
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
		const FPCGExTupleValueWrap* HeaderData = Header.DefaultData.GetPtr<FPCGExTupleValueWrap>();

		if (!HeaderData) { continue; }

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
					const FPCGExTupleValueWrap* Row = Settings->Values[k].Row[i].GetPtr<FPCGExTupleValueWrap>();

					if (!Row) { continue; }

					if constexpr (std::is_same_v<T, bool>) { Value = static_cast<const FPCGExTupleValueWrapBoolean*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, float>) { Value = static_cast<const FPCGExTupleValueWrapFloat*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, double>) { Value = static_cast<const FPCGExTupleValueWrapDouble*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, int32>) { Value = static_cast<const FPCGExTupleValueWrapInteger32*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, FVector2D>) { Value = static_cast<const FPCGExTupleValueWrapVector2*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, FVector>) { Value = static_cast<const FPCGExTupleValueWrapVector*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, FVector4>)
					{
						Value =
							HeaderData->GetValueType() == EPCGExTupleTypes::Color ?
								FVector4(static_cast<const FPCGExTupleValueWrapColor*>(Row)->Value)
								: static_cast<const FPCGExTupleValueWrapVector4*>(Row)->Value;
					}
					else if constexpr (std::is_same_v<T, FTransform>) { Value = static_cast<const FPCGExTupleValueWrapTransform*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, FString>) { Value = static_cast<const FPCGExTupleValueWrapString*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, FName>) { Value = static_cast<const FPCGExTupleValueWrapName*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, FRotator>) { Value = static_cast<const FPCGExTupleValueWrapRotator*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, FSoftObjectPath>) { Value = static_cast<const FPCGExTupleValueWrapSoftObjectPath*>(Row)->Value; }
					else if constexpr (std::is_same_v<T, FSoftClassPath>) { Value = static_cast<const FPCGExTupleValueWrapSoftClassPath*>(Row)->Value; }

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
