// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoBuffer.h"
#include "PCGExGeoPrimtives.h"

namespace PCGExGeo
{
	template <int DIMENSIONS>
	class PCGEXTENDEDTOOLKIT_API TConvexHull
	{
	public:
		TArray<TFVtx<DIMENSIONS>*> Vertices;
		TArray<TFSimplex<DIMENSIONS>*> Simplices;

		double Centroid[DIMENSIONS];

	protected:
		static constexpr double PLANE_DISTANCE_TOLERANCE = 1e-7f;
		double CurrentMaxDistance = TNumericLimits<double>::Min();

		TArray<TDeferredSimplex<DIMENSIONS>*> ConeFaceBuffer;
		TArray<TFVtx<DIMENSIONS>*> InternalVertices;
		TArray<TSimplexWrap<DIMENSIONS>*> InternalSimplices;
		TArray<TSimplexWrap<DIMENSIONS>*> CurrentAffectedFaces;
		TSet<TFVtx<DIMENSIONS>*> SingularVertices;

		TSimplexWrap<DIMENSIONS>* InternalUpdateBuffer[DIMENSIONS];
		int32 InternalUpdateIndices[DIMENSIONS];

		TFVtx<DIMENSIONS>* CurrentVertex = nullptr;
		TFVtx<DIMENSIONS>* CurrentFurthestVertex = nullptr;

		TSimplexList<DIMENSIONS>* UnprocessedFaces = nullptr;

		TVertexBuffer<DIMENSIONS>* EMPTY_VBUFFER = nullptr;
		TVertexBuffer<DIMENSIONS>* OriginalBeyondVertices = nullptr;
		TVertexBuffer<DIMENSIONS>* CurrentBeyondVertices = nullptr;

		THullObjectsPool<DIMENSIONS>* Pool = nullptr;
		TQueue<TSimplexWrap<DIMENSIONS>*> TraverseQueue;

	public:
		TConvexHull()
		{
			Vertices.Empty();
			Simplices.Empty();

			ConeFaceBuffer.Empty();

			for (int i = 0; i < DIMENSIONS; i++)
			{
				InternalUpdateBuffer[i] = nullptr;
				InternalUpdateIndices[i] = -1;
			}

			UnprocessedFaces = new TSimplexList<DIMENSIONS>();
			EMPTY_VBUFFER = new TVertexBuffer<DIMENSIONS>();
			OriginalBeyondVertices = new TVertexBuffer<DIMENSIONS>();

			CurrentBeyondVertices = OriginalBeyondVertices;

			Pool = new THullObjectsPool<DIMENSIONS>();
		}

		~TConvexHull()
		{
			Vertices.Empty();
			Simplices.Empty();

			ConeFaceBuffer.Empty();
			InternalVertices.Empty();

			for (TSimplexWrap<DIMENSIONS>* Wrap : InternalSimplices) { if (Wrap->VerticesBeyond == EMPTY_VBUFFER) { Wrap->VerticesBeyond = nullptr; } }
			PCGEX_DELETE_TARRAY(InternalSimplices) //TODO: Need to check ownership of internal TVertexBuffer
			CurrentAffectedFaces.Empty();
			SingularVertices.Empty();

			PCGEX_DELETE(UnprocessedFaces)
			PCGEX_DELETE(EMPTY_VBUFFER)
			PCGEX_DELETE(OriginalBeyondVertices)
			PCGEX_DELETE(Pool)
		}

#pragma region Generate

		bool Generate(TArray<TFVtx<DIMENSIONS>*>& Input)
		{
			if (Input.Num() < DIMENSIONS + 1) { return false; }

			InternalVertices.Empty(Input.Num());
			InternalVertices.Append(Input);

			InitConvexHull();

			// Expand the convex hull and faces.
			while (UnprocessedFaces->First)
			{
				TSimplexWrap<DIMENSIONS>* CurrentFace = UnprocessedFaces->First;
				CurrentVertex = CurrentFace->FurthestVertex;

				UpdateCenter();

				// The affected faces get tagged
				TagAffectedFaces(CurrentFace);

				// Create the cone from the currentVertex and the affected faces horizon.
				if (!SingularVertices.Contains(CurrentVertex) && CreateCone()) { CommitCone(); }
				else { HandleSingular(); }

				// Need to reset the tags
				for (TSimplexWrap<DIMENSIONS>* Simplex : CurrentAffectedFaces) { Simplex->Tag = 0; }
			}

			for (int i = 0; i < InternalSimplices.Num(); i++)
			{
				TSimplexWrap<DIMENSIONS>* Wrap = InternalSimplices[i];
				Wrap->Tag = i;
				Simplices.Add(Wrap);
			}

			return true;
		}

#pragma endregion

	protected:
#pragma region Initilization

		/// Find the (dimension+1) initial points and create the simplexes.
		void InitConvexHull()
		{
			TArray<TFVtx<DIMENSIONS>*> Extremes;
			Extremes.Reserve(2 * DIMENSIONS);
			FindExtremes(Extremes);

			TArray<TFVtx<DIMENSIONS>*> InitialPoints;
			FindInitialPoints(Extremes, InitialPoints);

			//TODO: InitialPoints may be empty due to singularities.

			const int NumPoints = InitialPoints.Num();

			// Add the initial points to the convex hull.
			for (int i = 0; i < NumPoints; i++)
			{
				CurrentVertex = InitialPoints[i];
				// update center must be called before adding the vertex.
				UpdateCenter();
				Vertices.Add(CurrentVertex);
				InternalVertices.Remove(InitialPoints[i]);

				// Because of the AklTou heuristic.
				Extremes.Remove(InitialPoints[i]);
			}

			// Create the initial simplexes.
			TArray<TSimplexWrap<DIMENSIONS>*> Faces;
			Faces.SetNumUninitialized(DIMENSIONS + 1);
			InitiateFaceDatabase(Faces);

			// Init the vertex beyond buffers.
			for (TSimplexWrap<DIMENSIONS>* Face : Faces)
			{
				FindBeyondVertices(Face);
				if (Face->VerticesBeyond->IsEmpty()) { InternalSimplices.Add(Face); } // The face is on the hull 
				else { UnprocessedFaces->Add(Face); }
			}
		}

		/// Finds the extremes in all dimensions.
		void FindExtremes(TArray<TFVtx<DIMENSIONS>*>& OutExtremes)
		{
			for (int i = 0; i < DIMENSIONS; i++)
			{
				double Min = TNumericLimits<double>::Max();
				double Max = TNumericLimits<double>::Min();
				int MinInd = 0;
				int MaxInd = 0;

				for (int j = 0; j < InternalVertices.Num(); j++)
				{
					const double v = (*InternalVertices[j])[i];

					if (v < Min)
					{
						Min = v;
						MinInd = j;
					}

					if (v > Max)
					{
						Max = v;
						MaxInd = j;
					}
				}

				if (MinInd != MaxInd)
				{
					OutExtremes.Add(InternalVertices[MinInd]);
					OutExtremes.Add(InternalVertices[MaxInd]);
				}
				else { OutExtremes.Add(InternalVertices[MinInd]); }
			}
		}

		/// Computes the sum of square distances to the initial points.
		static double GetSquaredDistanceSum(
			const TFVtx<DIMENSIONS>* Pivot,
			const TArray<TFVtx<DIMENSIONS>*>& InitialPoints)
		{
			double Sum = 0.0f;
			for (int i = 0; i < InitialPoints.Num(); i++)
			{
				const TFVtx<DIMENSIONS>* InitPt = InitialPoints[i];
				for (int j = 0; j < DIMENSIONS; j++)
				{
					const double t = ((*InitPt)[j] - (*Pivot)[j]);
					Sum += t * t;
				}
			}
			return Sum;
		}

		/// Finds (dimension + 1) initial points.
		void FindInitialPoints(
			TArray<TFVtx<DIMENSIONS>*>& InExtremes,
			TArray<TFVtx<DIMENSIONS>*>& OutInitialPoints)
		{
			TFVtx<DIMENSIONS>* First = nullptr;
			TFVtx<DIMENSIONS>* Second = nullptr;
			double MaxDist = 0.0f;

			for (int i = 0; i < InExtremes.Num() - 1; i++)
			{
				TFVtx<DIMENSIONS>* A = InExtremes[i];
				for (int j = i + 1; j < InExtremes.Num(); j++)
				{
					TFVtx<DIMENSIONS>* B = InExtremes[j];

					double Dist = 0;
					for (int k = 0; k < DIMENSIONS; k++)
					{
						const double S = (*A)[k] - (*B)[k];
						Dist += S * S;
					}

					if (Dist > MaxDist)
					{
						First = A;
						Second = B;
						MaxDist = Dist;
					}
				}
			}

			OutInitialPoints.Add(First);
			OutInitialPoints.Add(Second);

			for (int i = 2; i <= DIMENSIONS; i++)
			{
				double Maximum = 0.000001f;
				TFVtx<DIMENSIONS>* MaxPoint = nullptr;

				for (TFVtx<DIMENSIONS>* Extreme : InExtremes)
				{
					if (OutInitialPoints.Contains(Extreme)) { continue; }

					if (const double Val = GetSquaredDistanceSum(Extreme, OutInitialPoints);
						Val > Maximum)
					{
						Maximum = Val;
						MaxPoint = Extreme;
					}
				}

				if (MaxPoint)
				{
					OutInitialPoints.Add(MaxPoint);
				}
				else
				{
					for (TFVtx<DIMENSIONS>* Point : InternalVertices)
					{
						if (OutInitialPoints.Contains(Point)) { continue; }
						if (const double Val = GetSquaredDistanceSum(Point, OutInitialPoints);
							Val > Maximum)
						{
							Maximum = Val;
							MaxPoint = Point;
						}
					}

					check(MaxPoint) // Singular input data error
					OutInitialPoints.Add(MaxPoint);
				}
			}
		}

		/// Create the first faces from (dimension + 1) vertices.
		void InitiateFaceDatabase(TArray<TSimplexWrap<DIMENSIONS>*>& Faces)
		{
			for (int i = 0; i < Faces.Num(); i++)
			{
				TSimplexWrap<DIMENSIONS>* NewFace = Pool->GetFace();

				// Skips the i-th vertex
				int32 II = 0;
				for (int j = 0; j < Faces.Num() + 1; ++j)
				{
					if (j != i && II < DIMENSIONS) { NewFace->Vertices[II++] = Vertices[j]; }
				}

				std::sort(
					std::begin(NewFace->Vertices), std::end(NewFace->Vertices),
					[](const TFVtx<DIMENSIONS>* A, const TFVtx<DIMENSIONS>* B) { return A->Id < B->Id; });

				CalculateFacePlane(NewFace);
				Faces[i] = NewFace;
			}

			// update the adjacency (check all pairs of faces)
			for (int i = 0; i < DIMENSIONS; i++)
			{
				for (int j = i + 1; j < DIMENSIONS + 1; j++)
				{
					UpdateAdjacency(Faces[i], Faces[j]);
				}
			}
		}

		/// Check if 2 faces are adjacent and if so, update their AdjacentFaces array.
		void UpdateAdjacency(TSimplexWrap<DIMENSIONS>* Left, TSimplexWrap<DIMENSIONS>* Right)
		{
			int i;

			// reset marks on the 1st face
			for (i = 0; i < DIMENSIONS; i++) { Left->Vertices[i]->Tag = 0; }

			// mark all vertices on the 2nd face
			for (i = 0; i < DIMENSIONS; i++) { Right->Vertices[i]->Tag = 1; }

			// find the 1st false index
			for (i = 0; i < DIMENSIONS; i++) { if (Left->Vertices[i]->Tag == 0) { break; } }

			// no vertex was marked
			if (i == DIMENSIONS) { return; }

			// check if only 1 vertex wasn't marked
			for (int j = i + 1; j < DIMENSIONS; j++) { if (Left->Vertices[j]->Tag == 0) { return; } }

			// if we are here, the two faces share an edge
			Left->AdjacentFaces[i] = Right;

			// update the adj. face on the other face - find the vertex that remains marked
			for (i = 0; i < DIMENSIONS; i++) { Left->Vertices[i]->Tag = 0; }
			for (i = 0; i < DIMENSIONS; i++) { if (Right->Vertices[i]->Tag == 1) { break; } }

			Right->AdjacentFaces[i] = Left;
		}

		/// Calculates the normal and offset of the hyper-plane given by the face's vertices.
		bool CalculateFacePlane(TSimplexWrap<DIMENSIONS>* Face)
		{
			Face->UpdateNormal();

			if (FMath::IsNaN(Face->Normal[0])) { return false; }

			double Offset = 0.0f;
			double CenterDistance = 0.0f;
			TFVtx<DIMENSIONS>& V = *Face->Vertices[0];
			for (int i = 0; i < DIMENSIONS; i++)
			{
				double n = Face->Normal[i];
				Offset += n * V[i];
				CenterDistance += n * Centroid[i];
			}

			Face->Offset = -Offset;
			CenterDistance -= Offset;

			if (CenterDistance > 0)
			{
				for (int i = 0; i < DIMENSIONS; i++) { Face->Normal[i] = -Face->Normal[i]; }
				Face->Offset = Offset;
				Face->bIsNormalFlipped = true;
			}
			else
			{
				Face->bIsNormalFlipped = false;
			}

			return true;
		}

		/// <summary>
		/// Used in the "initialization" code.
		/// </summary>
		void FindBeyondVertices(TSimplexWrap<DIMENSIONS>* Face)
		{
			TVertexBuffer<DIMENSIONS>* BeyondVertices = Face->VerticesBeyond;

			CurrentMaxDistance = TNumericLimits<double>::Min();
			CurrentFurthestVertex = nullptr;

			for (int i = 0; i < InternalVertices.Num(); i++)
			{
				IsBeyond(Face, BeyondVertices, InternalVertices[i]);
			}

			Face->FurthestVertex = CurrentFurthestVertex;
		}

#pragma endregion

#pragma region Process

		/// Tags all faces seen from the current vertex with 1.
		void TagAffectedFaces(TSimplexWrap<DIMENSIONS>* CurrentFace)
		{
			CurrentAffectedFaces.Empty();
			CurrentAffectedFaces.Add(CurrentFace);
			TraverseAffectedFaces(CurrentFace);
		}

		/// Recursively traverse all the relevant faces.
		void TraverseAffectedFaces(TSimplexWrap<DIMENSIONS>* CurrentFace)
		{
			TraverseQueue.Empty();
			TraverseQueue.Enqueue(CurrentFace);

			CurrentFace->Tag = 1;

			TSimplexWrap<DIMENSIONS>* Top = nullptr;
			while (TraverseQueue.Dequeue(Top))
			{
				for (int i = 0; i < DIMENSIONS; i++)
				{
					TSimplexWrap<DIMENSIONS>* AdjFace = Top->TypedAdjacentFace(i);

					check(AdjFace) // (2) Adjacent Face should never be null

					if (AdjFace->Tag == 0 && AdjFace->GetVertexDistance(CurrentVertex) >= PLANE_DISTANCE_TOLERANCE)
					{
						CurrentAffectedFaces.Add(AdjFace);
						AdjFace->Tag = 1;
						TraverseQueue.Enqueue(AdjFace);
					}
				}
			}
		}

		/// Removes the faces "covered" by the current vertex and adds the newly created ones.
		bool CreateCone()
		{
			int CurrentVertexIndex = CurrentVertex->Id;
			ConeFaceBuffer.Empty();

			for (int FIndex = 0; FIndex < CurrentAffectedFaces.Num(); FIndex++)
			{
				TSimplexWrap<DIMENSIONS>* OldFace = CurrentAffectedFaces[FIndex];

				// Find the faces that need to be updated
				int UpdateCount = 0;
				for (int i = 0; i < DIMENSIONS; i++)
				{
					TSimplexWrap<DIMENSIONS>* Af = OldFace->TypedAdjacentFace(i);

					// (3) Adjacent Face should never be null"
					check(Af)

					if (Af->Tag == 0) // Tag == 0 when oldFaces does not contain af
					{
						InternalUpdateBuffer[UpdateCount] = Af;
						InternalUpdateIndices[UpdateCount] = i;
						++UpdateCount;
					}
				}

				for (int i = 0; i < UpdateCount; i++)
				{
					TSimplexWrap<DIMENSIONS>* AdjacentFace = InternalUpdateBuffer[i];
					int OldFaceAdjacentIndex = 0;
					for (int j = 0; j < DIMENSIONS; j++)
					{
						if (OldFace == AdjacentFace->AdjacentFaces[j])
						{
							OldFaceAdjacentIndex = j;
							break;
						}
					}

					// Index of the face that corresponds to this adjacent face
					int Forbidden = InternalUpdateIndices[i];

					TSimplexWrap<DIMENSIONS>* NewFace = Pool->GetFace();

					for (int j = 0; j < DIMENSIONS; j++) { NewFace->Vertices[j] = OldFace->Vertices[j]; }

					const int OldVertexIndex = NewFace->Vertices[Forbidden]->Id;
					int OrderedPivotIndex;

					// correct the ordering
					if (CurrentVertexIndex < OldVertexIndex)
					{
						OrderedPivotIndex = 0;
						for (int j = Forbidden - 1; j >= 0; j--)
						{
							if (NewFace->Vertices[j]->Id > CurrentVertexIndex) { NewFace->Vertices[j + 1] = NewFace->Vertices[j]; }
							else
							{
								OrderedPivotIndex = j + 1;
								break;
							}
						}
					}
					else
					{
						OrderedPivotIndex = DIMENSIONS - 1;
						for (int j = Forbidden + 1; j < DIMENSIONS; j++)
						{
							if (NewFace->Vertices[j]->Id < CurrentVertexIndex) { NewFace->Vertices[j - 1] = NewFace->Vertices[j]; }
							else
							{
								OrderedPivotIndex = j - 1;
								break;
							}
						}
					}

					NewFace->Vertices[OrderedPivotIndex] = CurrentVertex;

					if (!CalculateFacePlane(NewFace)) { return false; }

					ConeFaceBuffer.Add(MakeDeferredFace(NewFace, OrderedPivotIndex, AdjacentFace, OldFaceAdjacentIndex, OldFace));
				}
			}

			return true;
		}

		/// Creates a deferred face.
		TDeferredSimplex<DIMENSIONS>* MakeDeferredFace(
			TSimplexWrap<DIMENSIONS>* Face, int FaceIndex,
			TSimplexWrap<DIMENSIONS>* Pivot, int PivotIndex,
			TSimplexWrap<DIMENSIONS>* OldFace)
		{
			TDeferredSimplex<DIMENSIONS>* Ret = Pool->GetDeferredSimplex();

			Ret->Face = Face;
			Ret->FaceIndex = FaceIndex;
			Ret->Pivot = Pivot;
			Ret->PivotIndex = PivotIndex;
			Ret->OldFace = OldFace;

			return Ret;
		}

		/// <summary>
		/// Commits a cone and adds a vertex to the convex hull.
		/// </summary>

		void CommitCone()
		{
			// Add the current vertex.
			Vertices.Add(CurrentVertex);

			// Fill the adjacency.
			for (int i = 0; i < ConeFaceBuffer.Num(); i++)
			{
				TDeferredSimplex<DIMENSIONS>* Face = ConeFaceBuffer[i];

				TSimplexWrap<DIMENSIONS>* NewFace = Face->Face;
				TSimplexWrap<DIMENSIONS>* AdjacentFace = Face->Pivot;
				TSimplexWrap<DIMENSIONS>* OldFace = Face->OldFace;
				int OrderedPivotIndex = Face->FaceIndex;

				NewFace->AdjacentFaces[OrderedPivotIndex] = AdjacentFace;
				AdjacentFace->AdjacentFaces[Face->PivotIndex] = NewFace;

				// let there be a connection.
				for (int j = 0; j < DIMENSIONS; j++)
				{
					if (j == OrderedPivotIndex) continue;
					TSimplexConnector<DIMENSIONS>* Connector = Pool->GetConnector();
					Connector->Update(NewFace, j);
					ConnectFace(Connector);
				}

				// This could slightly help...
				if (AdjacentFace->VerticesBeyond->Num() < OldFace->VerticesBeyond->Num())
				{
					FindBeyondVertices(NewFace, AdjacentFace->VerticesBeyond, OldFace->VerticesBeyond);
				}
				else
				{
					FindBeyondVertices(NewFace, OldFace->VerticesBeyond, AdjacentFace->VerticesBeyond);
				}

				// This face will definitely lie on the hull
				if (NewFace->VerticesBeyond->Num() == 0)
				{
					InternalSimplices.Add(NewFace);
					UnprocessedFaces->Remove(NewFace);
					Pool->ReturnVertexBuffer(NewFace->VerticesBeyond);
					NewFace->VerticesBeyond = EMPTY_VBUFFER;
				}
				else // Add the face to the list
				{
					UnprocessedFaces->Add(NewFace);
				}

				// recycle the object.
				Pool->ReturnDeferredSimplex(Face);
			}

			// Recycle the affected faces.
			for (TSimplexWrap<DIMENSIONS>* DeprecatedFace : CurrentAffectedFaces)
			{
				UnprocessedFaces->Remove(DeprecatedFace);
				Pool->ReturnFace(DeprecatedFace);
			}
		}

		/// Connect faces using a connector.
		void ConnectFace(TSimplexConnector<DIMENSIONS>* Connector)
		{
			int32 index = Connector->HashCode % Pool->CONNECTOR_TABLE_SIZE;
			ConnectorList<DIMENSIONS>* List = Pool->ConnectorTable[index];

			for (TSimplexConnector<DIMENSIONS>* Current = List->First; Current != nullptr; Current = Current->Next)
			{
				if (TSimplexConnector<DIMENSIONS>::AreConnectable(Connector, Current))
				{
					List->Remove(Current);
					TSimplexConnector<DIMENSIONS>::Connect(Current, Connector);

					Pool->ReturnConnector(Current);
					Pool->ReturnConnector(Connector);
					return;
				}
			}

			List->Add(Connector);
		}

		/// Used by update faces.
		void FindBeyondVertices(
			TSimplexWrap<DIMENSIONS>* Face,
			TVertexBuffer<DIMENSIONS>* Beyond,
			TVertexBuffer<DIMENSIONS>* Beyond1)
		{
			CurrentMaxDistance = TNumericLimits<double>::Min();
			CurrentFurthestVertex = nullptr;
			TFVtx<DIMENSIONS>* v = nullptr;

			for (int i = 0; i < Beyond1->Num(); i++) { (*Beyond1)[i]->Tag = 1; }

			CurrentVertex->Tag = 0;

			for (int i = 0; i < Beyond->Num(); i++)
			{
				v = (*Beyond)[i];
				if (v == CurrentVertex) { continue; }
				v->Tag = 0;
				IsBeyond(Face, CurrentBeyondVertices, v);
			}

			for (int i = 0; i < Beyond1->Num(); i++)
			{
				v = (*Beyond1)[i];
				if (v->Tag == 1) { IsBeyond(Face, CurrentBeyondVertices, v); }
			}

			Face->FurthestVertex = CurrentFurthestVertex;

			// Pull the old switch a roo
			TVertexBuffer<DIMENSIONS>* NextVerticesBeyond = Face->VerticesBeyond;
			Face->VerticesBeyond = CurrentBeyondVertices;
			NextVerticesBeyond->Empty();
			CurrentBeyondVertices = NextVerticesBeyond;
		}

		/// Handles singular vertex.
		void HandleSingular()
		{
			RollbackCenter();
			SingularVertices.Add(CurrentVertex);

			// This means that all the affected faces must be on the hull and that all their "vertices beyond" are singular.
			for (TSimplexWrap<DIMENSIONS>* Face : CurrentAffectedFaces)
			{
				TVertexBuffer<DIMENSIONS>* VB = Face->VerticesBeyond;
				for (int i = 0; i < VB->Num(); i++) { SingularVertices.Add((*VB)[i]); }

				InternalSimplices.Add(Face);
				UnprocessedFaces->Remove(Face);
				Pool->ReturnVertexBuffer(Face->VerticesBeyond);
				Face->VerticesBeyond = EMPTY_VBUFFER;
			}
		}

#pragma endregion

		/// Check whether the vertex v is beyond the given face. If so, add it to beyondVertices.
		void IsBeyond(
			TSimplexWrap<DIMENSIONS>* Face,
			TVertexBuffer<DIMENSIONS>* BeyondVertices,
			TFVtx<DIMENSIONS>* V)
		{
			const double Distance = Face->GetVertexDistance(V);
			if (Distance >= PLANE_DISTANCE_TOLERANCE)
			{
				if (Distance > CurrentMaxDistance)
				{
					CurrentMaxDistance = Distance;
					CurrentFurthestVertex = V;
				}
				BeyondVertices->Add(V);
			}
		}

		/// Recalculates the centroid of the current hull.
		void UpdateCenter()
		{
			const int32 Count = Vertices.Num() + 1;
			const double F1 = Count - 1;
			const double F2 = 1.0f / static_cast<double>(Count);
			for (int i = 0; i < DIMENSIONS; i++) { Centroid[i] = F2 * ((Centroid[i] * F1) + (*CurrentVertex)[i]); }
		}

		/// Removes the last vertex from the center.
		void RollbackCenter()
		{
			const int32 Count = Vertices.Num() + 1;
			double F1 = Count;
			double F2 = 1.0f / static_cast<double>(Count - 1);
			for (int i = 0; i < DIMENSIONS; i++) { Centroid[i] = F2 * ((Centroid[i] * F1) - (*CurrentVertex)[i]); }
		}
	};
}
