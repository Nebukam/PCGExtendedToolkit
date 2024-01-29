#include "Graph/Pathfinding/Search/PCGExScoredQueue.h"

PCGExSearch::TScoredQueue::TScoredQueue(const int32 Size, const int32& Item, const double Score)
{
	Scores.SetNum(Size);
	Enqueue(Item, Score);
}

PCGExSearch::TScoredQueue::~TScoredQueue()
{
	std::priority_queue<FScoredNode, std::vector<FScoredNode>, std::greater<FScoredNode>> EmptyQueue;
	std::swap(InternalQueue, EmptyQueue);
	Scores.Empty();
}

void PCGExSearch::TScoredQueue::Enqueue(const int32& Id, const double Score)
{
	Scores[Id] = Score;
	InternalQueue.push(FScoredNode(Id, Score));
}

bool PCGExSearch::TScoredQueue::Dequeue(int32& Item, double& OutScore)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(ScoredQueue::Dequeue);

	while (!InternalQueue.empty())
	{
		const FScoredNode TopNode = InternalQueue.top();
		InternalQueue.pop();

		if (TopNode.Score == Scores[TopNode.Id])
		{
			Item = TopNode.Id;
			OutScore = TopNode.Score;
			return true;
		}
	}

	return false;
}
