#pragma once

namespace PCGExSearch
{
	template <typename T>
	struct FNode
	{
		T* Item = nullptr;
		double Priority = 0;
		bool operator<(const FNode& Other) const { return Priority < Other.Priority; }
	};

	template <typename T>
	class TPriorityQueue
	{
	protected:
		TArray<FNode<T>*> Queue;
		TMap<T*, FNode<T>*> Items;

	public:
		void Enqueue(T* Item, const double Priority)
		{
			if (Items.Contains(Item))
			{
				SetPriority(Item, Priority);
				return;
			}

			FNode<T>* NewNode = new FNode<T>(Item, Priority);
			Items.Add(Item, NewNode);

			int32 HeapIndex = 0;
			for (int i = HeapIndex; i >= 0; i--)
			{
				if (Priority < Queue[i].Priority)
				{
					HeapIndex = i + 1;
					break;
				}
			}

			if (HeapIndex >= Queue.Num()) { Queue.Add(NewNode); }
			else { Queue.Insert(NewNode, HeapIndex); }
		}

		void SetPriority(T* Item, const double Priority)
		{
			FNode<T>** NodePtr = Items.Find(Item);

			if (!NodePtr)
			{
				Enqueue(Item, Priority);
				return;
			}

			FNode<T>* Node = *NodePtr;

			if (Node->Priority == Priority) { return; }

			int32 NewIndex = -1;
			int32 NodeIndex = -1;
			if (Node->Priority > Priority)
			{
				//Start from lower priorities
				for (int i = NewIndex; i <= Queue.Num(); i++)
				{
					if (Queue[i] == Node)
					{
						NodeIndex = i;
						if (NewIndex != -1) { break; }
					}

					if (NewIndex == -1 && Priority < Queue[i].Priority)
					{
						NewIndex = i;
						if (NodeIndex != -1) { break; }
					}
				}
			}
			else if (Node->Priority < Priority)
			{
				//Start from higher priorities
				for (int i = NewIndex; i >= 0; i--)
				{
					if (Queue[i] == Node)
					{
						NodeIndex = i;
						if (NewIndex != -1) { break; }
					}

					if (NewIndex == -1 && Priority < Queue[i].Priority)
					{
						NewIndex = i;
						if (NodeIndex != -1) { break; }
					}
				}
			}

			if(NewIndex == -1){ NewIndex = Queue.Num(); }
			else if (NewIndex > NodeIndex) { NewIndex--; }
			Queue.RemoveAt(NodeIndex);
			Queue.Insert(Node, NewIndex);
		}

		bool Dequeue(T& Item)
		{
			if (Queue.IsEmpty()) { return false; }
			FNode<T>* Node = Queue.Pop();
			Item = Node->Item;
			delete Node;
			return true;
		}
	};
}
