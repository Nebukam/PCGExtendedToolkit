---
description: Don't use them. Ever.
icon: triangle-exclamation
---

# EditorOnly Subgraphs

{% hint style="danger" %}
This is actually a very important and overlooked piece of information regarding PCG, so read that carefully if you think of, or have any `Editor Only` subgraph in your project.
{% endhint %}

## A package breaking setting

<figure><img src="../../.gitbook/assets/image (13).png" alt=""><figcaption></figcaption></figure>

If you ever came across this option in the PCG graph editor and thought "oh, handy!", <mark style="background-color:red;">**go uncheck it right now**</mark>.

During the cooking process, Unreal will interrogate assets to check whether they are editor only or not, which is fine. PCG Graph, however, have an **extremely destructive implementation of that method**: a graph will recursively check all of its nodes, and if any one node (including unpluggued, culled, disabled ones)has this toggle set to true, **the entire hierarchy will be flagged as `EditorOnly` and stripped.**

Including any PCG Component that references that graph, rendering everything even remotely associated with an `EditorOnly` PCG Graph _null_.

## pcgex.ListEditorOnlyGraphs

I once encountered that and it cost me hours of tracking down why one of my PCG component was null in a packaged build. I was super freaked out so I made a quick script to make sure to leave no stone unturned: you can run the `pcgex.ListEditorOnlyGraphs` command in the console, this will list every single node that will be flagged by the cooking process including the affected hierarchy.

Go get rid of these, for real.

***

Here's the code I used, if you need it without the whole PCGEx thing:

```cpp
static FAutoConsoleCommand CommandListEditorOnlyGraphs(
		TEXT("pcgex.ListEditorOnlyGraphs"),
		TEXT("Finds all graph marked as IsEditorOnly."),
		FConsoleCommandDelegate::CreateLambda(
			[]()
			{
				const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

				FARFilter Filter;
				Filter.ClassPaths.Add(UPCGGraph::StaticClass()->GetClassPathName());
				Filter.bRecursiveClasses = true;

				TArray<FAssetData> AssetDataList;
				AssetRegistry.GetAssets(Filter, AssetDataList);

				if (AssetDataList.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("No Editor-only graph found."));
					return;
				}

				const int32 NumTotalGraphs = AssetDataList.Num();
				int32 NumEditorOnlyGraphs = 0;
				for (const FAssetData& AssetData : AssetDataList)
				{
					if (UPCGGraph* Graph = Cast<UPCGGraph>(AssetData.GetAsset()))
					{
						if (Graph->IsEditorOnly())
						{
							NumEditorOnlyGraphs++;
							UE_LOG(LogTemp, Warning, TEXT("%s"), *Graph->GetPathName());
						}
					}
				}

				UE_LOG(LogTemp, Warning, TEXT("Found %d EditorOnly graphs out of %d inspected graphs."), NumEditorOnlyGraphs, NumTotalGraphs);
			}));
```

