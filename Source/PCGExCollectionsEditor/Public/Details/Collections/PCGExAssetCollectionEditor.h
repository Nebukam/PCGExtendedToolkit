// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Toolkits/AssetEditorToolkit.h"

class UPCGExAssetCollection;

namespace PCGExAssetCollectionEditor
{
	const FName EntriesName = FName("Entries");

	struct TabInfos
	{
		TabInfos() = default;

		TabInfos(const FName InId, const TSharedPtr<SWidget>& InView, const FName InLabel = NAME_None, const ETabRole InRole = MajorTab)
			: Id(InId), View(InView), Label(InLabel.IsNone() ? InId : InLabel), Role(InRole)
		{
		}

		FName Id = NAME_None;
		TSharedPtr<SWidget> Header = nullptr;
		TSharedPtr<SWidget> View = nullptr;
		TSharedPtr<SWidget> Footer = nullptr;
		TWeakPtr<SWidget> WeakView = nullptr;
		FName Label = NAME_None;
		ETabRole Role = MajorTab;
		FString Icon = TEXT("");
	};

	struct FilterInfos
	{
		FilterInfos() = default;

		FilterInfos(const FName InId, const FText& InLabel, const FText& InToolTip)
			: Id(InId), Label(InLabel), ToolTip(InToolTip)
		{
		}

		FName Id = NAME_None;
		FText Label = FText::GetEmpty();
		FText ToolTip = FText::GetEmpty();
	};
}

class FPCGExAssetCollectionEditor : public FAssetEditorToolkit
{
public:
	FPCGExAssetCollectionEditor();
	virtual ~FPCGExAssetCollectionEditor() override;

	virtual void InitEditor(UPCGExAssetCollection* InCollection, const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost);
	virtual UPCGExAssetCollection* GetEditedCollection() const;

	virtual FName GetToolkitFName() const override { return FName("PCGExAssetCollectionEditor"); }
	virtual FText GetBaseToolkitName() const override { return INVTEXT("PCGEx Collection Editor"); }
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("PCGEx"); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }

	TMap<FName, PCGExAssetCollectionEditor::FilterInfos> FilterInfos;

protected:
	TWeakObjectPtr<UPCGExAssetCollection> EditedCollection;
	virtual void RegisterPropertyNameMapping(TMap<FName, FName>& Mapping);

	FReply FilterShowAll() const;
	FReply FilterHideAll() const;
	FReply ToggleFilter(const PCGExAssetCollectionEditor::FilterInfos Filter) const;

	virtual void CreateTabs(TArray<PCGExAssetCollectionEditor::TabInfos>& OutTabs);
	virtual void BuildEditorToolbar(FToolBarBuilder& ToolbarBuilder);
	virtual void BuildAssetHeaderToolbar(FToolBarBuilder& ToolbarBuilder);
	virtual void BuildAssetFooterToolbar(FToolBarBuilder& ToolbarBuilder);
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void ForceRefreshTabs();

	TArray<PCGExAssetCollectionEditor::TabInfos> Tabs;
	FDelegateHandle OnHiddenAssetPropertyNamesChanged;
};
