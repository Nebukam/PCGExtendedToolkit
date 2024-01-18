#pragma once

namespace PCGExSearch
{
	template <typename T>
	class TScoredQueue
	{
		using FElementType = T;

		struct FScoredNode
		{
			FScoredNode* Prev = nullptr;
			FScoredNode* Next = nullptr;

			FElementType Item;
			double Score = 0;

			/** Creates and initializes a new node. */
			explicit FScoredNode(const FElementType& InItem, const double InScore)
				: Item(InItem), Score(InScore)
			{
			}

			/** Creates and initializes a new node. */
			explicit FScoredNode(FElementType&& InItem, const double InScore)
				: Item(MoveTemp(InItem)), Score(InScore)
			{
			}

			bool operator<(const FScoredNode& Other) const { return Score < Other.Score; }

			void Collapse()
			{
				if (Prev) { Prev->Next = Next; }
				if (Next) { Next->Prev = Prev; }
			}
		};

	protected:
		FScoredNode* Head = nullptr;
		FScoredNode* Tail = nullptr;

		TMap<FElementType, FScoredNode*> Map;

		void InsertNode(FScoredNode* Node)
		{
			FScoredNode* PrevBest = Tail;
			while (PrevBest)
			{
				if (PrevBest->Score > Node->Score) { break; }
				PrevBest = PrevBest->Prev;
			}

			if (!PrevBest)
			{
				Head->Prev = Node;
				Node->Next = Head;
				Node->Prev = nullptr;
				Head = Node;
			}
			else
			{
				FScoredNode* OldNext = PrevBest->Next;
				if (PrevBest) { PrevBest->Next = Node; }

				Node->Prev = PrevBest;
				Node->Next = OldNext;

				if (OldNext) { OldNext->Prev = Node; }
				if (PrevBest == Tail) { Tail = Node; }
			}
		}

	public:
		TScoredQueue(const FElementType& Item, const double Score)
		{
			Enqueue(Item, Score);
		}

		~TScoredQueue()
		{
			while (Tail != nullptr)
			{
				FScoredNode* Node = Tail;
				Tail = Tail->Prev;
				delete Node;
			}
			Map.Empty();
		}

		void Enqueue(const FElementType& Item, const double Score)
		{
			if (Map.Contains(Item))
			{
				SetScore(Item, Score);
				return;
			}

			FScoredNode* NewNode = new FScoredNode(Item, Score);
			Map.Add(Item, NewNode);

			if (!Tail && !Head)
			{
				Head = Tail = NewNode;
				return;
			}

			InsertNode(NewNode);
		}

		void SetScore(const FElementType& Item, const double Score, bool bEnqueue = false)
		{
			FScoredNode** NodePtr = Map.Find(Item);

			if (!NodePtr)
			{
				if (bEnqueue) { Enqueue(Item, Score); }
				return;
			}

			FScoredNode* Node = *NodePtr;
			if (Node == Tail) { Tail = Node->Prev; }
			if (Node == Head) { Head = Node->Next; }

			Node->Collapse();
			Node->Score = Score;
			
			if(!Tail && !Head)
			{
				Tail = Head = Node;
				return;
			}

			InsertNode(Node);
		}

		bool GetScore(const FElementType& Item, double& OutScore)
		{
			if (FScoredNode** NodePtr = Map.Find(Item))
			{
				OutScore = (*NodePtr)->Score;
				return true;
			}

			return false;
		}

		bool Dequeue(FElementType& Item, double& OutScore)
		{
			if (!Tail) { return false; }

			FScoredNode* OldTail = Tail;
			if (OldTail == Head) { Head = nullptr; }
			Tail = Tail->Prev;


			OldTail->Collapse();

			Item = OldTail->Item;
			OutScore = OldTail->Score;

			Map.Remove(Item);
			delete OldTail;

			return true;
		}

		void Push(const FElementType& Item)
		{
			if (Map.Contains(Item))
			{
				SetScore(Item, TNumericLimits<double>::Max());
				return;
			}

			FScoredNode* NewNode = new FScoredNode(Item, TNumericLimits<double>::Max());
			Map.Add(Item, NewNode);

			if (!Tail && !Head)
			{
				Head = Tail = NewNode;
				return;
			}

			Head->Prev = NewNode;
			NewNode->Next = Head;
			NewNode->Prev = nullptr;
			Head = NewNode;
		}
	};
}
