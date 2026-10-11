// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Widgets/SPCGExPropertyChoicesEditor.h"

#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PCGExProperty.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Details/PCGExPropertyChoicesEditTargets.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SPCGExPropertyChoicesEditor"

void SPCGExPropertyChoicesEditor::Construct(const FArguments& InArgs)
{
	RealHandle = InArgs._RealHandle;
	Context = InArgs._Context;
	bPatch = InArgs._bPatch;

	const UScriptStruct* RootStruct = bPatch ? FPCGExPropertyChoicesPatchEditTarget::StaticStruct() : FPCGExPropertyChoicesEditTarget::StaticStruct();
	const FProperty* MemberProperty = RootStruct->FindPropertyByName(bPatch ? TEXT("Patch") : TEXT("Choices"));
	MemberStruct = bPatch ? FPCGExPropertyChoicesPatch::StaticStruct() : FPCGExPropertyChoices::StaticStruct();

	Scratch = MakeShared<FStructOnScope>(RootStruct);
	ScratchMember = MemberProperty ? MemberProperty->ContainerPtrToValuePtr<void>(Scratch->GetStructMemory()) : nullptr;
	if (!ScratchMember)
	{
		return;
	}
	if (bPatch && !Context.RealPatch)
	{
		Context.RealPatch = static_cast<const FPCGExPropertyChoicesPatch*>(GetRealData());
	}
	RefreshScratchFromReal();
	Scope = MakeUnique<FPCGExChoicesEditScope>(ScratchMember, Context);

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowScrollBar = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	View = PropertyModule.CreateStructureDetailView(DetailsArgs, FStructureDetailsViewArgs(), Scratch);
	View->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SPCGExPropertyChoicesEditor::OnFinishedChanging);

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(560.0f)
		.MaxDesiredHeight(480.0f)
		.Padding(4.0f)
		[
			View->GetWidget().ToSharedRef()
		]
	];
}

void* SPCGExPropertyChoicesEditor::GetRealData() const
{
	if (!RealHandle.IsValid())
	{
		return nullptr;
	}
	TArray<void*> RawData;
	RealHandle->AccessRawData(RawData);
	return RawData.Num() == 1 ? RawData[0] : nullptr;
}

void SPCGExPropertyChoicesEditor::RefreshScratchFromReal()
{
	if (void* Real = GetRealData())
	{
		MemberStruct->CopyScriptStruct(ScratchMember, Real);
	}
}

void SPCGExPropertyChoicesEditor::OnFinishedChanging(const FPropertyChangedEvent& InEvent)
{
	void* Real = GetRealData();
	if (bCommitting || !Real || !RealHandle.IsValid())
	{
		return;
	}
	TGuardValue<bool> Guard(bCommitting, true);

	// One transaction per finished edit: the owner is Modify'd by NotifyPreChange, the copy lands on the real
	// struct, and NotifyPostChange runs the host's post-edit path so every row re-mirrors.
	{
		FScopedTransaction Transaction(bPatch ? LOCTEXT("EditChoicesPatch", "Edit Choices") : LOCTEXT("EditChoices", "Edit Choices"));
		RealHandle->NotifyPreChange();
		MemberStruct->CopyScriptStruct(Real, ScratchMember);
		RealHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		RealHandle->NotifyFinishedChangingProperties();
	}

	// Import rows are only reconciled by the schema customization on shape changes: refresh their mirror here.
	if (bPatch && Context.Collection)
	{
		TArray<FPCGExPropertyResolved> Resolved;
		Context.Collection->Resolve(Resolved);
		Context.Collection->ImportOverrides.SyncInPlace(Resolved);
	}

	// The host may have re-minted ids or re-synced carriers: take its version back, then rebuild the rows that
	// alias the copy's element memory before anything paints.
	RefreshScratchFromReal();
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float) -> EActiveTimerReturnType
	{
		if (View.IsValid() && View->GetDetailsView())
		{
			View->GetDetailsView()->ForceRefresh();
		}
		return EActiveTimerReturnType::Stop;
	}));
}

#undef LOCTEXT_NAMESPACE
