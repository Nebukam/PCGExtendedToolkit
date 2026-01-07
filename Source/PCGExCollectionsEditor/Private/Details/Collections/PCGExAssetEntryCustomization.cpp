// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExAssetEntryCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PCGExCollectionsEditorSettings.h"
#include "PropertyHandle.h"
#include "Collections/PCGExActorCollection.h"
#include "Core/PCGExAssetCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"

void FPCGExAssetEntryCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> WeightHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExAssetCollectionEntry, Weight));
	TSharedPtr<IPropertyHandle> CategoryHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExAssetCollectionEntry, Category));
	TSharedPtr<IPropertyHandle> IsSubCollectionHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExAssetCollectionEntry, bIsSubCollection));

	HeaderRow.NameContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(2, 10)
			[
				GetAssetPicker(PropertyHandle, IsSubCollectionHandle)
			]
		]
		.ValueContent()
		.MinDesiredWidth(400)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(2, 0)
			[

				SNew(SHorizontalBox)

				// Weight
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 0)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Weight"))).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(FLinearColor::Gray)).MinDesiredWidth(10)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.MinWidth(50)
				.Padding(2, 0)
				[
					WeightHandle->CreatePropertyValueWidget()
				]

				// Category
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2, 0)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("·· Category"))).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(FLinearColor::Gray)).MinDesiredWidth(10)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				.MinWidth(50)
				.Padding(2, 0)
				[
					CategoryHandle->CreatePropertyValueWidget()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(2, 0)
			[
				// Wrap in a border to control opacity based on value
				SNew(SBorder)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.ColorAndOpacity(FLinearColor(1, 1, 1, 0.6f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0)
					[
						IsSubCollectionHandle->CreatePropertyValueWidget()
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2, 0)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("Sub-collection"))).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(FLinearColor::Gray)).MinDesiredWidth(8)
					]
				]
			]
		];
}

void FPCGExAssetEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Grab owning collection
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 0) { return; }

	const UPCGExAssetCollection* Collection = Cast<UPCGExAssetCollection>(OuterObjects[0]);
	if (!Collection) { return; }

	uint32 NumElements = 0;
	PropertyHandle->GetNumChildren(NumElements);

	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedPtr<IPropertyHandle> ElementHandle = PropertyHandle->GetChildHandle(i);
		FName ElementName = ElementHandle ? ElementHandle->GetProperty()->GetFName() : NAME_None;
		if (!ElementHandle.IsValid() || CustomizedTopLevelProperties.Contains(ElementName)) { continue; }

		IDetailPropertyRow& PropertyRow = ChildBuilder.AddProperty(ElementHandle.ToSharedRef());
		// Bind visibility dynamically
		PropertyRow.Visibility(
			MakeAttributeLambda(
				[ElementName]()
				{
					return GetDefault<UPCGExCollectionsEditorSettings>()->GetPropertyVisibility(ElementName);
				}));
	}
}

void FPCGExAssetEntryCustomization::FillCustomizedTopLevelPropertiesNames()
{
	CustomizedTopLevelProperties.Add(FName("Weight"));
	CustomizedTopLevelProperties.Add(FName("Category"));
	CustomizedTopLevelProperties.Add(FName("bIsSubCollection"));
	CustomizedTopLevelProperties.Add(FName("SubCollection"));
}

TSharedRef<IPropertyTypeCustomization> FPCGExMeshEntryCustomization::MakeInstance()
{
	TSharedRef<IPropertyTypeCustomization> Ref = MakeShareable(new FPCGExMeshEntryCustomization());
	static_cast<FPCGExMeshEntryCustomization&>(Ref.Get()).FillCustomizedTopLevelPropertiesNames();
	return Ref;
}

void FPCGExMeshEntryCustomization::FillCustomizedTopLevelPropertiesNames()
{
	FPCGExAssetEntryCustomization::FillCustomizedTopLevelPropertiesNames();
	CustomizedTopLevelProperties.Add(FName("StaticMesh"));
}

#define PCGEX_SUBCOLLECTION_VISIBLE \
.Visibility_Lambda([IsSubCollectionHandle](){ \
	bool bWantsSubcollections = false; \
	IsSubCollectionHandle->GetValue(bWantsSubcollections);\
	return bWantsSubcollections ? EVisibility::Visible : EVisibility::Collapsed;})

#define PCGEX_SUBCOLLECTION_COLLAPSED \
.Visibility_Lambda([IsSubCollectionHandle](){ \
	bool bWantsSubcollections = false; \
	IsSubCollectionHandle->GetValue(bWantsSubcollections); \
	return bWantsSubcollections ? EVisibility::Collapsed : EVisibility::Visible;})

#define PCGEX_ENTRY_INDEX \
+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0)[\
SNew(STextBlock).Text_Lambda([PropertyHandle](){\
		const int32 Index = PropertyHandle->GetIndexInArray();\
		if (Index == INDEX_NONE) { return FText::FromString(TEXT("")); }\
		return FText::FromString(FString::Printf(TEXT("%d →"), Index));}).Font(IDetailLayoutBuilder::GetDetailFont())\
.ColorAndOpacity(FSlateColor(FLinearColor(1,1,1,0.25)))]

TSharedRef<SWidget> FPCGExMeshEntryCustomization::GetAssetPicker(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<IPropertyHandle> IsSubCollectionHandle)
{
	TSharedPtr<IPropertyHandle> SubCollection = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExMeshCollectionEntry, SubCollection));
	TSharedPtr<IPropertyHandle> StaticMeshHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExMeshCollectionEntry, StaticMesh));

	return SNew(SHorizontalBox)
			PCGEX_ENTRY_INDEX
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.MinWidth(200)
			.Padding(2, 0)
			[
				SNew(SBox)
				PCGEX_SUBCOLLECTION_VISIBLE
				[
					SubCollection->CreatePropertyValueWidget()
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.MinWidth(200)
			.Padding(2, 0)
			[
				SNew(SBox)
				PCGEX_SUBCOLLECTION_COLLAPSED
				[
					StaticMeshHandle->CreatePropertyValueWidget()
				]
			];
}

TSharedRef<IPropertyTypeCustomization> FPCGExActorEntryCustomization::MakeInstance()
{
	TSharedRef<IPropertyTypeCustomization> Ref = MakeShareable(new FPCGExActorEntryCustomization());
	static_cast<FPCGExActorEntryCustomization&>(Ref.Get()).FillCustomizedTopLevelPropertiesNames();
	return Ref;
}

void FPCGExActorEntryCustomization::FillCustomizedTopLevelPropertiesNames()
{
	FPCGExAssetEntryCustomization::FillCustomizedTopLevelPropertiesNames();
	CustomizedTopLevelProperties.Add(FName("Actor"));
}

TSharedRef<SWidget> FPCGExActorEntryCustomization::GetAssetPicker(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<IPropertyHandle> IsSubCollectionHandle)
{
	TSharedPtr<IPropertyHandle> SubCollection = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExMeshCollectionEntry, SubCollection));
	TSharedPtr<IPropertyHandle> ActorPickHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExActorCollectionEntry, Actor));

	return SNew(SHorizontalBox)
			PCGEX_ENTRY_INDEX
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.MinWidth(200)
			.Padding(2, 0)
			[
				SNew(SBox)
				PCGEX_SUBCOLLECTION_VISIBLE
				[
					SubCollection->CreatePropertyValueWidget()
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.MinWidth(200)
			.Padding(2, 0)
			[
				SNew(SBox)
				PCGEX_SUBCOLLECTION_COLLAPSED
				[
					ActorPickHandle->CreatePropertyValueWidget()
				]
			];
}

#undef PCGEX_SUBCOLLECTION_VISIBLE
#undef PCGEX_SUBCOLLECTION_COLLAPSED
