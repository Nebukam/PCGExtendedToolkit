// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoBuffer.h"
#include "PCGExGeoPrimtives.h"

namespace PCGExGeo
{
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class PCGEXTENDEDTOOLKIT_API TConvexHull
	{
	public:
		static constexpr double PLANE_DISTANCE_TOLERANCE = 1e-7f;

		TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*> Vertices;
		TArray<TFSimplex<DIMENSIONS, VECTOR_TYPE>*> Simplices;

		VECTOR_TYPE Centroid = VECTOR_TYPE{};

		TObjectBuffer<DIMENSIONS, VECTOR_TYPE>* Buffer = nullptr;

		TConvexHull()
		{
			Vertices.Empty();
			Simplices.Empty();
		}

		~TConvexHull()
		{
			Vertices.Empty();
			Simplices.Empty();
		}

		bool Contains(TFVtx<DIMENSIONS, VECTOR_TYPE>* vertex)
		{
			for (TFSimplex<DIMENSIONS, VECTOR_TYPE>* Simplex : Simplices)
			{
				if (Simplex->GetVertexDistance(vertex) >= PLANE_DISTANCE_TOLERANCE) { return false; }
			}

			return true;
		}

#pragma region Generate

		void Generate(TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*>& Input)
		{
			Clear();

			Buffer = new TObjectBuffer<DIMENSIONS, VECTOR_TYPE>();

			if (Input.Num() < DIMENSIONS + 1) { return; }

			Buffer->InitInput(Input);

			InitConvexHull();

			// Expand the convex hull and faces.
			while (Buffer->UnprocessedFaces->First)
			{
				TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* CurrentFace = Buffer->UnprocessedFaces->First;
				Buffer->CurrentVertex = CurrentFace->FurthestVertex;

				UpdateCenter();

				// The affected faces get tagged
				TagAffectedFaces(CurrentFace);

				// Create the cone from the currentVertex and the affected faces horizon.
				if (!Buffer->SingularVertices.Contains(Buffer->CurrentVertex) && CreateCone()) { CommitCone(); }
				else { HandleSingular(); }

				// Need to reset the tags
				for (TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Simplex : Buffer->AffectedFaceBuffer) { Simplex->Tag = 0; }
			}

			for (int i = 0; i < Buffer->ConvexSimplexs.Num(); i++)
			{
				TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Wrap = Buffer->ConvexSimplexs[i];
				Wrap->Tag = i;
				Simplices.Add(new TFSimplex<DIMENSIONS, VECTOR_TYPE>()); //TODO: Need to destroy this at some point
			}

			for (int i = 0; i < Buffer->ConvexSimplexs.Num(); i++)
			{
				TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Wrap = Buffer->ConvexSimplexs[i];
				TFSimplex<DIMENSIONS, VECTOR_TYPE>* Simplex = Simplices[i];

				Simplex->bIsNormalFlipped = Wrap->bIsNormalFlipped;
				Simplex->Offset = Wrap->Offset;

				for (int j = 0; j < DIMENSIONS; j++)
				{
					Simplex->Normal[j] = Wrap->Normal[j];
					Simplex->Vertices[j] = Wrap->Vertices[j];

					if (Wrap->AdjacentFaces[j]) { Simplex->AdjacentFaces[j] = Simplices[Wrap->AdjacentFaces[j]->Tag]; }
					else { Simplex->AdjacentFaces[j] = nullptr; }
				}

				Simplex->UpdateCentroid();
			}

			//TODO : Woah, definitely some more cleanup to do, buffer owns tons of shit apparently
			PCGEX_DELETE(Buffer)
		}

		void Clear()
		{
			Centroid = VECTOR_TYPE{};
			Vertices.Empty();
			PCGEX_DELETE_TARRAY(Simplices);
			PCGEX_DELETE(Buffer)
		}

#pragma endregion

#pragma region Initilization

		/// Find the (dimension+1) initial points and create the simplexes.
		void InitConvexHull()
		{
			TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*> Extremes;
			Extremes.Reserve(2 * DIMENSIONS);
			FindExtremes(Extremes);

			TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*> InitialPoints;
			FindInitialPoints(Extremes, InitialPoints);

			//TODO: InitialPoints may be empty due to singularities.

			const int NumPoints = InitialPoints.Num();

			// Add the initial points to the convex hull.
			for (int i = 0; i < NumPoints; i++)
			{
				Buffer->CurrentVertex = InitialPoints[i];
				// update center must be called before adding the vertex.
				UpdateCenter();
				Vertices.Add(Buffer->CurrentVertex);
				Buffer->InputVertices.Remove(InitialPoints[i]);

				// Because of the AklTou heuristic.
				Extremes.Remove(InitialPoints[i]);
			}

			// Create the initial simplexes.
			TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Faces[DIMENSIONS + 1];
			InitiateFaceDatabase(Faces);

			// Init the vertex beyond buffers.
			for (int i = 0; i < DIMENSIONS + 1; i++)
			{
				FindBeyondVertices(Faces[i]);
				if (Faces[i]->VerticesBeyond->Num() == 0)
					Buffer->ConvexSimplexs.Add(Faces[i]); // The face is on the hull
				else
					Buffer->UnprocessedFaces->Add(Faces[i]);
			}
		}

		/// Finds the extremes in all dimensions.
		void FindExtremes(TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*>& OutExtremes)
		{
			for (int i = 0; i < DIMENSIONS; i++)
			{
				double Min = TNumericLimits<double>::Max();
				double Max = TNumericLimits<double>::Min();
				int MinInd = 0;
				int MaxInd = 0;

				for (int j = 0; j < Buffer->InputVertices.Num(); j++)
				{
					const double v = Buffer->InputVertices[j]->Position[i];

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
					OutExtremes.Add(Buffer->InputVertices[MinInd]);
					OutExtremes.Add(Buffer->InputVertices[MaxInd]);
				}
				else { OutExtremes.Add(Buffer->InputVertices[MinInd]); }
			}
		}

		/// Computes the sum of square distances to the initial points.
		double GetSquaredDistanceSum(
			TFVtx<DIMENSIONS, VECTOR_TYPE>* Pivot,
			TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*>& initialPoints)
		{
			double Sum = 0.0f;
			for (int i = 0; i < initialPoints.Num(); i++)
			{
				TFVtx<DIMENSIONS, VECTOR_TYPE>* InitPt = initialPoints[i];
				for (int j = 0; j < DIMENSIONS; j++)
				{
					const float t = (InitPt->Position[j] - Pivot->Position[j]);
					Sum += t * t;
				}
			}
			return Sum;
		}

		/// Finds (dimension + 1) initial points.
		void FindInitialPoints(
			TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*>& InExtremes,
			TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*>& OutInitialPoints)
		{
			TFVtx<DIMENSIONS, VECTOR_TYPE>* First = nullptr;
			TFVtx<DIMENSIONS, VECTOR_TYPE>* Second = nullptr;
			double MaxDist = 0.0f;
			VECTOR_TYPE Temp;

			for (int i = 0; i < InExtremes.Num() - 1; i++)
			{
				TFVtx<DIMENSIONS, VECTOR_TYPE>* A = InExtremes[i];
				for (int j = i + 1; j < InExtremes.Num(); j++)
				{
					TFVtx<DIMENSIONS, VECTOR_TYPE>* B = InExtremes[j];

					Temp = A->Position - B->Position;

					float Dist = 0;
					for (int k = 0; k < DIMENSIONS; k++) { Dist += Temp[i] * Temp[i]; } // Length squared

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
				TFVtx<DIMENSIONS, VECTOR_TYPE>* MaxPoint = nullptr;

				for (int j = 0; j < InExtremes.Num(); j++)
				{
					TFVtx<DIMENSIONS, VECTOR_TYPE>* Extreme = InExtremes[j];
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
					for (int j = 0; j < Buffer->InputVertices.Num(); j++)
					{
						TFVtx<DIMENSIONS, VECTOR_TYPE>* Point = Buffer->InputVertices[j];
						if (OutInitialPoints.Contains(Point)) { continue; }

						if (const double Val = GetSquaredDistanceSum(Point, OutInitialPoints);
							Val > Maximum)
						{
							Maximum = Val;
							MaxPoint = Point;
						}
					}

					if (MaxPoint) { OutInitialPoints.Add(MaxPoint); }
					else { /* TODO : SINGULAR EXCEPTION TO BE HANDLED */ }
				}
			}
		}

		/// Create the first faces from (dimension + 1) vertices.
		void InitiateFaceDatabase(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* (&Faces)[DIMENSIONS + 1])
		{
			for (int i = 0; i < DIMENSIONS + 1; i++)
			{
				TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* NewFace = new TSimplexWrap<DIMENSIONS, VECTOR_TYPE>(new TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>());

				// Skips the i-th vertex
				int II = 0;
				for (int32 j = 0; j < Vertices.Num(); ++j) { if (i != j) { NewFace->Vertices[II++] = Vertices[j]; } }

				std::sort(
					std::begin(NewFace->Vertices), std::end(NewFace->Vertices),
					[](TFVtx<DIMENSIONS, VECTOR_TYPE>* A, TFVtx<DIMENSIONS, VECTOR_TYPE>* B) { return A->Id < B->Id; });

				CalculateFacePlane(NewFace);
				Faces[i] = NewFace;
			}

			// update the adjacency (check all pairs of faces)
			for (int i = 0; i < DIMENSIONS; i++) { for (int j = i + 1; j < DIMENSIONS + 1; j++) { Faces[i]->UpdateAdjacency(Faces[j]); } }
		}

		/// Calculates the normal and offset of the hyper-plane given by the face's vertices.
		bool CalculateFacePlane(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face)
		{
			VECTOR_TYPE Normal = VECTOR_TYPE{};
			Face->CalculateNormal(Normal);

			if (FMath::IsNaN(Normal[0])) { return false; }

			double Offset = 0.0f;
			double CenterDistance = 0.0f;
			VECTOR_TYPE Fi = Face->Vertices[0]->Position;

			for (int i = 0; i < DIMENSIONS; i++)
			{
				double n = Normal[i];
				Offset += n * Fi[i];
				CenterDistance += n * Centroid[i];
			}

			Face->Offset = -Offset;
			CenterDistance -= Offset;

			if (CenterDistance > 0)
			{
				for (int i = 0; i < DIMENSIONS; i++) { Normal[i] = -Normal[i]; }
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
		void FindBeyondVertices(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face)
		{
			TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* BeyondVertices = Face->VerticesBeyond;

			Buffer->MaxDistance = TNumericLimits<double>::Min();
			Buffer->FurthestVertex = nullptr;

			for (int i = 0; i < Buffer->InputVertices.Num(); i++) { IsBeyond(Face, BeyondVertices, Buffer->InputVertices[i]); }

			Face->FurthestVertex = Buffer->FurthestVertex;
		}

#pragma endregion

#pragma region Process

		/// Tags all faces seen from the current vertex with 1.
		void TagAffectedFaces(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* CurrentFace)
		{
			Buffer->AffectedFaceBuffer.Empty();
			Buffer->AffectedFaceBuffer.Add(CurrentFace);
			TraverseAffectedFaces(CurrentFace);
		}

		/// Recursively traverse all the relevant faces.
		void TraverseAffectedFaces(TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* CurrentFace)
		{
			Buffer->TraverseStack.Empty();
			Buffer->TraverseStack.Enqueue(CurrentFace);
			CurrentFace->Tag = 1;

			TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Top = nullptr;
			while (Buffer->TraverseStack.Dequeue(Top))
			{
				for (int i = 0; i < DIMENSIONS; i++)
				{
					TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* AdjFace = Top->TypedAdjacentFace(i);
					if(!AdjFace){continue;}
					/* (2) Adjacent Face should never be null */
					check(AdjFace)

					if (AdjFace->Tag == 0 && AdjFace->GetVertexDistance(Buffer->CurrentVertex) >= PLANE_DISTANCE_TOLERANCE)
					{
						Buffer->AffectedFaceBuffer.Add(AdjFace);
						AdjFace->Tag = 1;
						Buffer->TraverseStack.Enqueue(AdjFace);
					}
				}
			}
		}

		/// Removes the faces "covered" by the current vertex and adds the newly created ones.
		bool CreateCone()
		{
			int CurrentVertexIndex = Buffer->CurrentVertex->Id;
			Buffer->ConeFaceBuffer.Empty();

			for (int FIndex = 0; FIndex < Buffer->AffectedFaceBuffer.Num(); FIndex++)
			{
				TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* oldFace = Buffer->AffectedFaceBuffer[FIndex];

				// Find the faces that need to be updated
				int UpdateCount = 0;
				for (int i = 0; i < DIMENSIONS; i++)
				{
					TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Af = oldFace->TypedAdjacentFace(i);

					if(!Af){continue;}
					// (3) Adjacent Face should never be null"
					check(Af)

					if (Af->Tag == 0) // Tag == 0 when oldFaces does not contain af
					{
						Buffer->UpdateBuffer[UpdateCount] = Af;
						Buffer->UpdateIndices[UpdateCount] = i;
						++UpdateCount;
					}
				}

				for (int i = 0; i < UpdateCount; i++)
				{
					TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* AdjacentFace = Buffer->UpdateBuffer[i];
					int OldFaceAdjacentIndex = 0;
					for (int j = 0; j < DIMENSIONS; j++)
					{
						if (oldFace == AdjacentFace->AdjacentFaces[j])
						{
							OldFaceAdjacentIndex = j;
							break;
						}
					}

					// Index of the face that corresponds to this adjacent face
					int Forbidden = Buffer->UpdateIndices[i];

					TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* NewFace = Buffer->ObjectManager->GetFace();

					for (int j = 0; j < DIMENSIONS; j++) { NewFace->Vertices[j] = oldFace->Vertices[j]; }

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

					NewFace->Vertices[OrderedPivotIndex] = Buffer->CurrentVertex;

					if (!CalculateFacePlane(NewFace)) { return false; }

					Buffer->ConeFaceBuffer.Add(MakeDeferredFace(NewFace, OrderedPivotIndex, AdjacentFace, OldFaceAdjacentIndex, oldFace));
				}
			}

			return true;
		}

		/// Creates a new deferred face.
		TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* MakeDeferredFace(
			TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face, int FaceIndex,
			TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Pivot, int PivotIndex,
			TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* OldFace)
		{
			TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* Ret = Buffer->ObjectManager->GetDeferredSimplex();

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
			Vertices.Add(Buffer->CurrentVertex);

			// Fill the adjacency.
			for (int i = 0; i < Buffer->ConeFaceBuffer.Num(); i++)
			{
				TDeferredSimplex<TSimplexWrap<DIMENSIONS, VECTOR_TYPE>>* Face = Buffer->ConeFaceBuffer[i];

				TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* NewFace = Face->Face;
				TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* AdjacentFace = Face->Pivot;
				TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* OldFace = Face->OldFace;
				int OrderedPivotIndex = Face->FaceIndex;

				NewFace->AdjacentFaces[OrderedPivotIndex] = AdjacentFace;
				AdjacentFace->AdjacentFaces[Face->PivotIndex] = NewFace;

				// let there be a connection.
				for (int j = 0; j < DIMENSIONS; j++)
				{
					if (j == OrderedPivotIndex) continue;
					TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* Connector = Buffer->ObjectManager->GetConnector();
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
					Buffer->ConvexSimplexs.Add(NewFace);
					Buffer->UnprocessedFaces->Remove(NewFace);
					Buffer->ObjectManager->DepositVertexBuffer(NewFace->VerticesBeyond);
					NewFace->VerticesBeyond = Buffer->EmptyBuffer;
				}
				else // Add the face to the list
				{
					Buffer->UnprocessedFaces->Add(NewFace);
				}

				// recycle the object.
				Buffer->ObjectManager->DepositDeferredSimplex(Face);
			}

			// Recycle the affected faces.
			for (TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* DeprecatedFace : Buffer->AffectedFaceBuffer)
			{
				Buffer->UnprocessedFaces->Remove(DeprecatedFace);
				Buffer->ObjectManager->DepositFace(DeprecatedFace);
			}
		}

		/// Connect faces using a connector.
		void ConnectFace(TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* Connector)
		{
			int32 index = Connector->HashCode % Buffer->CONNECTOR_TABLE_SIZE;
			ConnectorList<TSimplexConnector<DIMENSIONS, VECTOR_TYPE>>* List = Buffer->ConnectorTable[index];

			for (TSimplexConnector<DIMENSIONS, VECTOR_TYPE>* Current = List->First; Current != nullptr; Current = Current->Next)
			{
				if (TSimplexConnector<DIMENSIONS, VECTOR_TYPE>::AreConnectable(Connector, Current))
				{
					List->Remove(Current);
					TSimplexConnector<DIMENSIONS, VECTOR_TYPE>::Connect(Current, Connector);

					Buffer->ObjectManager->DepositConnector(Current);
					Buffer->ObjectManager->DepositConnector(Connector);
					return;
				}
			}

			List->Add(Connector);
		}

		/// Used by update faces.
		void FindBeyondVertices(
			TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face,
			TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* Beyond,
			TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* Beyond1)
		{
			TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* BeyondVertices = Buffer->BeyondBuffer;

			Buffer->MaxDistance = TNumericLimits<double>::Min();
			Buffer->FurthestVertex = nullptr;
			TFVtx<DIMENSIONS, VECTOR_TYPE>* v = nullptr;

			for (int i = 0; i < Beyond1->Num(); i++) { (*Beyond1)[i]->Tag = 1; }

			Buffer->CurrentVertex->Tag = 0;

			for (int i = 0; i < Beyond->Num(); i++)
			{
				v = (*Beyond)[i];
				if (v == Buffer->CurrentVertex) { continue; }
				v->Tag = 0;
				IsBeyond(Face, BeyondVertices, v);
			}

			for (int i = 0; i < Beyond1->Num(); i++)
			{
				v = (*Beyond1)[i];
				if (v->Tag == 1) { IsBeyond(Face, BeyondVertices, v); }
			}

			Face->FurthestVertex = Buffer->FurthestVertex;

			// Pull the old switch a roo
			TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* Temp = Face->VerticesBeyond;
			Face->VerticesBeyond = BeyondVertices;
			if (Temp->Num() > 0) { Temp->Clear(); }
			Buffer->BeyondBuffer = Temp;
		}

		/// Handles singular vertex.
		void HandleSingular()
		{
			RollbackCenter();
			Buffer->SingularVertices.Add(Buffer->CurrentVertex);

			// This means that all the affected faces must be on the hull and that all their "vertices beyond" are singular.
			for (TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face : Buffer->AffectedFaceBuffer)
			{
				TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* VB = Face->VerticesBeyond;
				for (int i = 0; i < VB->Num(); i++) { Buffer->SingularVertices.Add((*VB)[i]); }

				Buffer->ConvexSimplexs.Add(Face);
				Buffer->UnprocessedFaces->Remove(Face);
				Buffer->ObjectManager->DepositVertexBuffer(Face->VerticesBeyond);
				Face->VerticesBeyond = Buffer->EmptyBuffer;
			}
		}

#pragma endregion

		/// Check whether the vertex v is beyond the given face. If so, add it to beyondVertices.
		void IsBeyond(
			TSimplexWrap<DIMENSIONS, VECTOR_TYPE>* Face,
			TVertexBuffer<TFVtx<DIMENSIONS, VECTOR_TYPE>>* BeyondVertices,
			TFVtx<DIMENSIONS, VECTOR_TYPE>* V)
		{
			double Distance = Face->GetVertexDistance(V);
			if (Distance >= PLANE_DISTANCE_TOLERANCE)
			{
				if (Distance > Buffer->MaxDistance)
				{
					Buffer->MaxDistance = Distance;
					Buffer->FurthestVertex = V;
				}
				BeyondVertices->Add(V);
			}
		}

		/// Recalculates the centroid of the current hull.
		void UpdateCenter()
		{
			const int32 Count = Vertices.Num() + 1;
			for (int i = 0; i < DIMENSIONS; i++) { Centroid[i] *= (Count - 1); }
			float f = 1.0f / Count;
			for (int i = 0; i < DIMENSIONS; i++) { Centroid[i] = f * (Centroid[i] + Buffer->CurrentVertex->Position[i]); }
		}

		/// Removes the last vertex from the center.
		void RollbackCenter()
		{
			int Count = Vertices.Num() + 1;
			for (int i = 0; i < DIMENSIONS; i++) { Centroid[i] *= Count; }
			float f = 1.0f / (Count - 1);
			for (int i = 0; i < DIMENSIONS; i++) { Centroid[i] = f * (Centroid[i] - Buffer->CurrentVertex->Position[i]); }
		}
	};
}
