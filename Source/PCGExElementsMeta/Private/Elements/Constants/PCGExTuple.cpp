// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Constants/PCGExTuple.h"

#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Containers/PCGExManagedObjects.h"
#include "Helpers/PCGExArrayHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Tuple

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

// List used for boilerplace only, does not include stuff that require custom handling
#define PCGEX_FOREACH_TUPLETYPE_BOILERPLATE(MACRO) \
MACRO(float, Float) \
MACRO(double, Double) \
MACRO(int32, Integer32) \
MACRO(FVector2D, Vector2) \
MACRO(FVector, Vector) \
MACRO(FVector4, Vector4) \
MACRO(FTransform, Transform) \
MACRO(FString, String) \
MACRO(bool, Boolean) \
MACRO(FRotator, Rotator) \
MACRO(FName, Name) \
MACRO(FSoftObjectPath, SoftObjectPath) \
MACRO(FSoftClassPath, SoftClassPath)


FPCGMetadataAttributeBase* FPCGExTupleValueWrap::CreateAttribute(UPCGMetadata* Metadata, FName Name) const
{
	return nullptr;
}

void FPCGExTupleValueWrap::InitEntry(const FPCGExTupleValueWrap* InHeader)
{
}

void FPCGExTupleValueWrap::WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key) const
{
}

void FPCGExTupleValueWrap::SanitizeEntry(const FPCGExTupleValueWrap* InHeader)
{
}

#define PCGEX_TUPLE_TYPED_IMPL(_TYPE, _NAME)\
FPCGMetadataAttributeBase* FPCGExTupleValueWrap##_NAME::CreateAttribute(UPCGMetadata* Metadata, FName Name) const{\
	return Metadata->CreateAttribute<_TYPE>(Name, Value, true, true);}\
void FPCGExTupleValueWrap##_NAME::InitEntry(const FPCGExTupleValueWrap* InHeader){\
	Value = static_cast<const FPCGExTupleValueWrap##_NAME*>(InHeader)->Value; }\
void FPCGExTupleValueWrap##_NAME::WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key) const{\
	static_cast<FPCGMetadataAttribute<_TYPE>*>(Attribute)->SetValue(Key, Value);}

PCGEX_FOREACH_TUPLETYPE_BOILERPLATE(PCGEX_TUPLE_TYPED_IMPL)

#undef PCGEX_TUPLE_TYPED_IMPL

#pragma region Color

FPCGMetadataAttributeBase* FPCGExTupleValueWrapColor::CreateAttribute(UPCGMetadata* Metadata, FName Name) const
{
	return Metadata->CreateAttribute<FVector4>(Name, Value, true, true);
}

void FPCGExTupleValueWrapColor::InitEntry(const FPCGExTupleValueWrap* InHeader)
{
	Value = static_cast<const FPCGExTupleValueWrapColor*>(InHeader)->Value;
}

void FPCGExTupleValueWrapColor::WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key) const
{
	static_cast<FPCGMetadataAttribute<FVector4>*>(Attribute)->SetValue(Key, FVector4(Value));
}

#pragma endregion

#pragma region Enum Selector

FPCGMetadataAttributeBase* FPCGExTupleValueWrapEnumSelector::CreateAttribute(UPCGMetadata* Metadata, FName Name) const
{
	return Metadata->CreateAttribute<int64>(Name, Enum.Value, true, true);
}

void FPCGExTupleValueWrapEnumSelector::InitEntry(const FPCGExTupleValueWrap* InHeader)
{
	const FEnumSelector& ModelSelector = static_cast<const FPCGExTupleValueWrapEnumSelector*>(InHeader)->Enum;
	Enum.Class = ModelSelector.Class;
	Enum.Value = ModelSelector.Value;
}

void FPCGExTupleValueWrapEnumSelector::WriteValue(FPCGMetadataAttributeBase* Attribute, int64 Key) const
{
	static_cast<FPCGMetadataAttribute<FVector4>*>(Attribute)->SetValue(Key, Enum.Value);
}

void FPCGExTupleValueWrapEnumSelector::SanitizeEntry(const FPCGExTupleValueWrap* InHeader)
{
	const FEnumSelector& ModelSelector = static_cast<const FPCGExTupleValueWrapEnumSelector*>(InHeader)->Enum;
	if (ModelSelector.Class != Enum.Class) { Enum = ModelSelector; }
}

#pragma endregion

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
	if (CurrentData && InData.GetScriptStruct() == DefaultData.GetScriptStruct())
	{
		CurrentData->HeaderId = HeaderId;
		CurrentData->SanitizeEntry(HeaderData);
		return;
	}

	InData.InitializeAsScriptStruct(DefaultData.GetScriptStruct());
	CurrentData = InData.GetMutablePtr<FPCGExTupleValueWrap>();
	if (CurrentData)
	{
		CurrentData->HeaderId = HeaderId;
		CurrentData->InitEntry(HeaderData);
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

	const FPCGExTupleValueWrap* CurrentData = DefaultData.GetPtr<FPCGExTupleValueWrap>();

	if (!CurrentData) { return nullptr; }

	return CurrentData->CreateAttribute(TupleData->Metadata, Name);
}

#if WITH_EDITOR
void UPCGExTupleSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTupleSettings::PostEditChangeProperty);

	bool bNeedsProcessing = false;

	if (PropertyChangedEvent.MemberProperty)
	{
		FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		EPropertyChangeType::Type ChangeType = PropertyChangedEvent.ChangeType;

		// Check for changes in Composition
		if (PropName == GET_MEMBER_NAME_CHECKED(UPCGExTupleSettings, Composition))
		{
			bNeedsProcessing = true;
		}
		// Check for structural values change
		else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGExTupleSettings, Values) && (ChangeType == EPropertyChangeType::ArrayAdd || ChangeType == EPropertyChangeType::ArrayRemove || ChangeType == EPropertyChangeType::ArrayClear || ChangeType == EPropertyChangeType::ArrayMove))
		{
			bNeedsProcessing = true;
		}
	}

	if (!bNeedsProcessing)
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return; // Skip heavy processing
	}

	TArray<int32> Guids;
	TMap<int32, int32> Order;
	Guids.Reserve(Composition.Num());
	Order.Reserve(Composition.Num());

	bool bReordered = false;
	for (FPCGExTupleValueHeader& Header : Composition)
	{
		if (FPCGExTupleValueWrap* Model = Header.DefaultData.GetMutablePtr<FPCGExTupleValueWrap>()) { Model->bIsModel = true; }

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
				PCGExArrayHelpers::InitArray(Body.Row, Guids.Num());
				for (int i = StartIndex; i < Guids.Num(); ++i)
				{
					Composition[i].SanitizeEntry(Body.Row[i]);
				}
			}
			else if (bReordered)
			{
				// Reorder the values
				Body.Row.Sort([&](const TInstancedStruct<FPCGExTupleValueWrap>& A, const TInstancedStruct<FPCGExTupleValueWrap>& B)
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
			if (const FPCGExTupleValueWrap* HeaderData = Header.DefaultData.GetPtr<FPCGExTupleValueWrap>(); !HeaderData) { continue; }
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


bool FPCGExTupleElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
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

		for (int k = 0; k < Keys.Num(); k++)
		{
			const FPCGExTupleValueWrap* Row = Settings->Values[k].Row[i].GetPtr<FPCGExTupleValueWrap>();
			if (!Row) { continue; }
			Row->WriteValue(Attribute, Keys[k]);
		}
	}

	TSet<FString> Tags;
	PCGExArrayHelpers::AppendEntriesFromCommaSeparatedList(Settings->CommaSeparatedTags, Tags);
	Context->StageOutput(TupleData, FName("Tuple"), PCGExData::EStaging::None, Tags);

	Context->Done();
	return Context->TryComplete();
}

#undef PCGEX_FOREACH_TUPLETYPE
#undef PCGEX_FOREACH_TUPLETYPE_BOILERPLATE

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
