// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeo.h"
#include "PCGExGeoHull.h"
#include "PCGExGeoPrimtives.h"

namespace PCGExGeo
{
	class FPreProcessSimplexTask;
	class FProcessSimplexTask;

	template <int DIMENSIONS>
	class PCGEXTENDEDTOOLKIT_API TDelaunayCell
	{
	public:
		TFSimplex<DIMENSIONS>* Simplex = nullptr;
		TFVtx<DIMENSIONS>* Circumcenter = nullptr;
		double Radius = 0;
		bool bIsWithinBounds = true;
		bool bIsOnHull = false;
		FVector Centroid = FVector::Zero();

		TDelaunayCell(
			TFSimplex<DIMENSIONS>* InSimplex,
			TFVtx<DIMENSIONS>* InCircumcenter,
			const double InRadius)
		{
			Simplex = InSimplex;
			Circumcenter = InCircumcenter;
			Radius = InRadius;
			ComputeCentroid();
		}

		~TDelaunayCell()
		{
			Simplex = nullptr;
			PCGEX_DELETE(Circumcenter)
			Radius = 0;
		}

		void ComputeCentroid()
		{
			Centroid = FVector::Zero();
			for (TFVtx<DIMENSIONS>* Vtx : Simplex->Vertices) { Centroid += Vtx->Location; }
			Centroid /= DIMENSIONS;
		}

		FVector GetBestCenter() const
		{
			if (bIsWithinBounds) { return Circumcenter->GetV3(); }
			return Centroid;
		}

		void ComputeHullQuality()
		{
			bIsOnHull = true;
			for (int i = 0; i < DIMENSIONS - 1; i++)
			{
				if (!Simplex->Vertices[i]->bIsOnHull)
				{
					bIsOnHull = false;
					return;
				}
			}
		}
	};

	template <int DIMENSIONS, typename T_HULL>
	class PCGEXTENDEDTOOLKIT_API TDelaunayTriangulation
	{
	protected:
		bool bOwnsVertices = true;
		FRWLock HullLock;
		FRWLock AsyncLock;

		TMap<int32, int32> SimpliceIndices;

	public:
		TConvexHull<DIMENSIONS>* Hull = nullptr;
		TArray<TFVtx<DIMENSIONS>*> Vertices;
		TArray<TDelaunayCell<DIMENSIONS>*> Cells;
		TFVtx<DIMENSIONS>* Centroid = nullptr;
		int32 NumFinalCells = 0;
		TSet<int32>* ConvexHullIndices = nullptr;

		FBox Bounds;
		double BoundsExtension = 0;
		EPCGExCellCenter CellCenter = EPCGExCellCenter::Circumcenter;

		TDelaunayTriangulation()
		{
		}

		virtual ~TDelaunayTriangulation()
		{
			PCGEX_DELETE_TARRAY(Cells)
			SimpliceIndices.Empty();
			if (bOwnsVertices) { PCGEX_DELETE_TARRAY(Vertices) }
			else { Vertices.Empty(); }
			PCGEX_DELETE(Centroid)
			PCGEX_DELETE(Hull)
		}

		void GetUniqueEdges(TArray<PCGExGraph::FUnsignedEdge>& OutEdges)
		{
			TSet<uint64> UniqueEdges;
			UniqueEdges.Reserve(Cells.Num() * 3);

			for (const TDelaunayCell<DIMENSIONS>* Cell : Cells)
			{
				for (int i = 0; i < DIMENSIONS; i++)
				{
					const int32 A = Cell->Simplex->Vertices[i]->Id;
					for (int j = i + 1; j < DIMENSIONS; j++)
					{
						const int32 B = Cell->Simplex->Vertices[j]->Id;
						if (const uint64 Hash = PCGExGraph::GetUnsignedHash64(A, B);
							!UniqueEdges.Contains(Hash))
						{
							OutEdges.Emplace(A, B);
							UniqueEdges.Add(Hash);
						}
					}
				}
			}

			UniqueEdges.Empty();
		}

		void GetUrquhartEdges(TArray<PCGExGraph::FUnsignedEdge>& OutEdges)
		{
			TSet<uint64> UniqueEdges;
			UniqueEdges.Reserve(Cells.Num() * 3);

			struct FMeasuredEdge
			{
				int32 A;
				int32 B;
			};

			int32 EdgeCount = 0;
			for (int i = 0; i < DIMENSIONS; i++) { for (int j = i + 1; j < DIMENSIONS; j++) { EdgeCount++; } }

			TArray<FMeasuredEdge> MeasuredEdges;
			MeasuredEdges.SetNum(EdgeCount);

			for (const TDelaunayCell<DIMENSIONS>* Cell : Cells)
			{
				int32 E = -1;
				int32 LE = -1;
				double MaxDist = TNumericLimits<double>::Min();

				for (int i = 0; i < DIMENSIONS; i++)
				{
					const int32 A = Cell->Simplex->Vertices[i]->Id;
					for (int j = i + 1; j < DIMENSIONS; j++)
					{
						const int32 B = Cell->Simplex->Vertices[j]->Id;
						FMeasuredEdge& Edge = MeasuredEdges[++E];
						Edge.A = A;
						Edge.B = B;

						if (const double Dist = FVector::DistSquared(Vertices[A]->GetV3Downscaled(), Vertices[B]->GetV3Downscaled());
							Dist > MaxDist)
						{
							LE = E;
							MaxDist = Dist;
						}
					}
				}

				const FMeasuredEdge& LongestEdge = MeasuredEdges[LE];
				UniqueEdges.Add(PCGExGraph::GetUnsignedHash64(LongestEdge.A, LongestEdge.B));

				for (const FMeasuredEdge& Edge : MeasuredEdges)
				{
					if (uint64 Hash = PCGExGraph::GetUnsignedHash64(Edge.A, Edge.B);
						!UniqueEdges.Contains(Hash))
					{
						OutEdges.Emplace(Edge.A, Edge.B);
						UniqueEdges.Add(Hash);
					}
				}
			}

			UniqueEdges.Empty();
		}

		bool PrepareFrom(const TArray<FPCGPoint>& InPoints)
		{
			bOwnsVertices = true;

			GetUpscaledVerticesFromPoints<DIMENSIONS>(InPoints, Vertices);

			if (Vertices.Num() <= DIMENSIONS) { return false; }
			ComputeVerticesBounds();

			return InternalPrepare();
		}

		bool PrepareFrom(const TArray<TFVtx<DIMENSIONS>*>& InVertices)
		{
			bOwnsVertices = false;

			if (InVertices.Num() <= DIMENSIONS) { return false; }

			Vertices.Reset(InVertices.Num());
			Vertices.Append(InVertices);
			ComputeVerticesBounds();

			for (TFVtx<DIMENSIONS>* Vtx : Vertices) { Vtx->bIsOnHull = ConvexHullIndices->Contains(Vtx->Id); }

			return InternalPrepare();
		}

		virtual void Generate()
		{
			Hull->Generate();
			PrepareDelaunay();

			for (int i = 0; i < Hull->Simplices.Num(); i++) { PreprocessSimplex(i); }

			Cells.SetNumUninitialized(NumFinalCells);
			for (int i = 0; i < NumFinalCells; i++) { ProcessSimplex(i); }
		}

	protected:
		void PrepareDelaunay()
		{
			for (int i = 0; i < DIMENSIONS; i++) { (*Centroid)[i] = Hull->Centroid[i]; }

			NumFinalCells = 0;
			SimpliceIndices.Empty();
		}

		void ComputeVerticesBounds()
		{
			Bounds = FBox(ForceInit);
			for (TFVtx<DIMENSIONS>* Vtx : Vertices) { Bounds += Vtx->Location; }
			Bounds = Bounds.ExpandBy(BoundsExtension);
		}

		bool InternalPrepare()
		{
			PCGEX_DELETE_TARRAY(Cells)
			PCGEX_DELETE(Centroid)
			PCGEX_DELETE(Hull)

			if (Vertices.Num() <= DIMENSIONS) { return false; }

			Hull = new T_HULL();

			if (!Hull->Prepare(Vertices))
			{
				PCGEX_DELETE(Hull)
				return false;
			}

			Centroid = new TFVtx<DIMENSIONS>();
			return true;
		}

	public:
		void PreprocessSimplex(int32 Index)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Delaunay::PreprocessSimplex);

			TFSimplex<DIMENSIONS>* Simplex = Hull->Simplices[Index];

			if (Simplex->Normal[DIMENSIONS - 1] >= 0.0f)
			{
				FWriteScopeLock WriteLock(HullLock);
				for (TFSimplex<DIMENSIONS>* Adjacent : Simplex->AdjacentFaces) { if (Adjacent) { Adjacent->Remove(Simplex); } }
				return;
			}

			{
				FWriteScopeLock WriteLock(AsyncLock);
				SimpliceIndices.Add(NumFinalCells++, Index);
			}
		}

		void ProcessSimplex(int32 Index)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Delaunay::ProcessSimplex);

			TFSimplex<DIMENSIONS>* Simplex = Hull->Simplices[*SimpliceIndices.Find(Index)];
			TDelaunayCell<DIMENSIONS>* Cell = CreateCell(Simplex);
			Cell->bIsWithinBounds = Bounds.IsInside(CellCenter == EPCGExCellCenter::Centroid ? Cell->Centroid : Cell->Circumcenter->GetV3());
			Cell->Circumcenter->Id = Index;
			Cell->ComputeHullQuality();
			Cells[Index] = Cell;
		}

	protected:
		virtual TDelaunayCell<DIMENSIONS>* CreateCell(TFSimplex<DIMENSIONS>* Simplex) = 0;
	};

#pragma region Delaunay2


	class PCGEXTENDEDTOOLKIT_API TDelaunayTriangulation2 : public TDelaunayTriangulation<3, TConvexHull3>
	{
	public:
		TDelaunayTriangulation2() : TDelaunayTriangulation()
		{
		}

	protected:
		virtual TDelaunayCell<3>* CreateCell(TFSimplex<3>* Simplex) override
		{
			// From MathWorld: http://mathworld.wolfram.com/Circumcircle.html
			double MTX[3][3];

			auto Determinant = [&]() -> double
			{
				const double F00 = MTX[1][1] * MTX[2][2] - MTX[1][2] * MTX[2][1];
				const double F10 = MTX[1][2] * MTX[2][0] - MTX[1][0] * MTX[2][2];
				const double F20 = MTX[1][0] * MTX[2][1] - MTX[1][1] * MTX[2][0];
				return MTX[0][0] * F00 + MTX[0][1] * F10 + MTX[0][2] * F20;
			};

			// x, y, 1
			for (int i = 0; i < 3; i++)
			{
				TFVtx<3>& V = (*Simplex->Vertices[i]);
				MTX[i][0] = V[0];
				MTX[i][1] = V[1];
				MTX[i][2] = 1;
			}

			const double a = Determinant();

			// size, y, 1
			for (int i = 0; i < 3; i++) { MTX[i][0] = (*Simplex->Vertices[i])[2]; } //->SqrMagnitude();
			const double DX = -Determinant();

			// size, x, 1
			for (int i = 0; i < 3; i++) { MTX[i][1] = (*Simplex->Vertices[i])[0]; }
			const double DY = Determinant();

			// size, x, y
			for (int i = 0; i < 3; i++) { MTX[i][2] = (*Simplex->Vertices[i])[1]; }
			const double c = -Determinant();

			const double s = -1.0f / (2.0f * a);

			TFVtx<3>* CC = new TFVtx<3>();
			CC->SetV3(FVector(s * DX, s * DY, 0));

			return new TDelaunayCell(
				Simplex, CC,
				FMath::Abs(s) * FMath::Sqrt(DX * DX + DY * DY - 4.0 * a * c));
		}
	};

#pragma endregion
#pragma region Delaunay3

	class PCGEXTENDEDTOOLKIT_API TDelaunayTriangulation3 : public TDelaunayTriangulation<4, TConvexHull4>
	{
	public:
		TDelaunayTriangulation3() : TDelaunayTriangulation()
		{
		}

	protected:
		virtual TDelaunayCell<4>* CreateCell(TFSimplex<4>* Simplex) override
		{
			// From MathWorld: http://mathworld.wolfram.com/Circumsphere.html

			double MTX[4][4];

			auto MINOR = [&](const int R0, const int R1, const int R2, const int C0, const int C1, const int C2) -> double
			{
				return
					MTX[R0][C0] * (MTX[R1][C1] * MTX[R2][C2] - MTX[R2][C1] * MTX[R1][C2]) -
					MTX[R0][C1] * (MTX[R1][C0] * MTX[R2][C2] - MTX[R2][C0] * MTX[R1][C2]) +
					MTX[R0][C2] * (MTX[R1][C0] * MTX[R2][C1] - MTX[R2][C0] * MTX[R1][C1]);
			};

			auto Determinant = [&]()-> double
			{
				return (MTX[0][0] * MINOR(1, 2, 3, 1, 2, 3) -
					MTX[0][1] * MINOR(1, 2, 3, 0, 2, 3) +
					MTX[0][2] * MINOR(1, 2, 3, 0, 1, 3) -
					MTX[0][3] * MINOR(1, 2, 3, 0, 1, 2));
			};

			// x, y, z, 1
			for (int i = 0; i < 4; i++)
			{
				TFVtx<4>& V = (*Simplex->Vertices[i]);
				MTX[i][0] = V[0];
				MTX[i][1] = V[1];
				MTX[i][2] = V[2];
				MTX[i][3] = 1;
			}
			const double a = Determinant();

			// size, y, z, 1
			for (int i = 0; i < 4; i++) { MTX[i][0] = (*Simplex->Vertices[i])[3]; } //->SqrMagnitude();
			const double DX = Determinant();

			// size, x, z, 1
			for (int i = 0; i < 4; i++) { MTX[i][1] = (*Simplex->Vertices[i])[0]; }
			const double DY = -Determinant();

			// size, x, y, 1
			for (int i = 0; i < 4; i++) { MTX[i][2] = (*Simplex->Vertices[i])[1]; }
			const double DZ = Determinant();

			//size, x, y, z
			for (int i = 0; i < 4; i++) { MTX[i][3] = (*Simplex->Vertices[i])[2]; }
			const double c = Determinant();

			const double s = 1.0f / (2.0f * a);

			TFVtx<4>* CC = new TFVtx<4>();
			CC->SetV4(FVector4(s * DX, s * DY, s * DZ, 0));

			return new TDelaunayCell(
				Simplex, CC,
				FMath::Abs(s) * FMath::Sqrt(DX * DX + DY * DY + DZ * DZ - 4 * a * c));
		}
	};

#pragma endregion

#undef PCGEX_DELAUNAY_CLASS_DECL
#undef PCGEX_DELAUNAY_METHODS
#undef PCGEX_DELAUNAY_CLASS
}
