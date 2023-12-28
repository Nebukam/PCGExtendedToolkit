// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoHull.h"
#include "PCGExGeoPrimtives.h"

namespace PCGExGeo
{
	template <int DIMENSIONS, typename VECTOR_TYPE>
	class TDelaunayCell
	{
	public:
		TFSimplex<DIMENSIONS, VECTOR_TYPE>* Simplex = nullptr;
		TFVtx<DIMENSIONS, VECTOR_TYPE>* Circumcenter = nullptr;
		double Radius = 0;

		TDelaunayCell(
			TFSimplex<DIMENSIONS, VECTOR_TYPE>* InSimplex,
			VECTOR_TYPE InCircumcenter,
			const double InRadius)
		{
			Simplex = InSimplex;

			Circumcenter = new TFVtx<DIMENSIONS, VECTOR_TYPE>();
			Circumcenter->Position = InCircumcenter;
			Radius = InRadius;
		}

		~TDelaunayCell()
		{
			Simplex = nullptr;
			PCGEX_DELETE(Circumcenter)
			Radius = 0;
		}
	};

	template <int DIMENSIONS, typename VECTOR_TYPE, typename UPSCALED_VECTOR_TYPE>
	class TDelaunayTriangulation
	{
	public:
		TArray<TFVtx<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>*> UpscaledVertices;
		TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*> Vertices;
		TArray<TDelaunayCell<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>*> Cells;
		TFVtx<DIMENSIONS, VECTOR_TYPE>* Centroid = nullptr;

		double MTX[DIMENSIONS + 1][DIMENSIONS + 1] = {};

		TDelaunayTriangulation()
		{
			Centroid = new TFVtx<DIMENSIONS, VECTOR_TYPE>();
		}

		virtual ~TDelaunayTriangulation()
		{
			Cells.Empty();
			Vertices.Empty();
			UpscaledVertices.Empty();
			PCGEX_DELETE(Centroid)
		}

		virtual void Clear()
		{
			Cells.Empty();
			Vertices.Empty();
			UpscaledVertices.Empty();
			PCGEX_DELETE(Centroid)
			Centroid = new TFVtx<DIMENSIONS, VECTOR_TYPE>();
		}

		virtual void Generate(TArray<TFVtx<DIMENSIONS, VECTOR_TYPE>*>& Input, bool bAssignIds = true)
		{
			//TODO: Take an array of FPCGPoint as input so we don't have to create an upscaled copy of the point for hull generation
			Clear();
			if (Input.Num() <= DIMENSIONS + 1) return;

			UpscaledVertices.Reserve(Input.Num());

			//Create upscaled input for Hull generation			
			for (TFVtx<DIMENSIONS, VECTOR_TYPE>* Vtx : Input)
			{
				TFVtx<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>* UVtx = UpscaledVertices.Add_GetRef(new TFVtx<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>());
				for (int i = 0; i < DIMENSIONS; i++) { UVtx->Position[i] = Vtx->Position[i]; }
				UVtx->Position[DIMENSIONS] = Vtx->SqrMagnitude();
			}

			TConvexHull<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>* Hull = new TConvexHull<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>();
			Hull->Generate(UpscaledVertices, bAssignIds);

			Vertices.Empty(Hull->Vertices.Num());
			for (int i = 0; i < Hull->Vertices.Num(); i++)
			{
				TFVtx<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>* UVtx = Hull->Vertices[i];
				TFVtx<DIMENSIONS, VECTOR_TYPE>* Vtx = Input[i];
				// TODO: Update original points with hulled data? Pointless if id are stable.
			}

			for (int i = 0; i < DIMENSIONS; i++) { Centroid->Position[i] = Hull->Centroid[i]; }

			for (TFSimplex<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>* Simplex : Hull->Simplexs)
			{
				if (Simplex->Normal[DIMENSIONS] >= 0.0f)
				{
					for (TFSimplex<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>* Adjacent : Simplex->AdjacentFaces)
					{
						if (Adjacent) { Adjacent->Remove(Simplex); }
					}
				}
				else
				{
					TDelaunayCell<DIMENSIONS  + 1, UPSCALED_VECTOR_TYPE>* Cell = CreateCell(Simplex);
					//cell.CircumCenter.Id = i;
					Cells.Add(Cell);
				}
			}
		}

	protected:
		virtual TDelaunayCell<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>* CreateCell(TFSimplex<DIMENSIONS + 1, UPSCALED_VECTOR_TYPE>* Simplex) = 0;
		virtual double Determinant() const = 0;
	};

	class TDelaunayTriangulation2 : public TDelaunayTriangulation<2, FVector2D, FVector>
	{
	public:
		TDelaunayTriangulation2() : TDelaunayTriangulation()
		{
		}

	protected:
		virtual double Determinant() const override
		{
			const double F00 = MTX[1][1] * MTX[2][2] - MTX[1][2] * MTX[2][1];
			const double F10 = MTX[1][2] * MTX[2][0] - MTX[1][0] * MTX[2][2];
			const double F20 = MTX[1][0] * MTX[2][1] - MTX[1][1] * MTX[2][0];
			return MTX[0][0] * F00 + MTX[0][1] * F10 + MTX[0][2] * F20;
		}

		virtual TDelaunayCell<3, FVector>* CreateCell(TFSimplex<3, FVector>* Simplex) override
		{
			// From MathWorld: http://mathworld.wolfram.com/Circumcircle.html

			// x, y, 1
			for (int i = 0; i < 3; i++)
			{
				MTX[i][0] = Simplex->Vertices[i]->Position[0];
				MTX[i][1] = Simplex->Vertices[i]->Position[1];
				MTX[i][2] = 1;
			}

			const double a = Determinant();

			// size, y, 1
			for (int i = 0; i < 3; i++) { MTX[i][0] = Simplex->Vertices[i]->Position[2]; } //->SqrMagnitude();
			const double DX = -Determinant();

			// size, x, 1
			for (int i = 0; i < 3; i++) { MTX[i][1] = Simplex->Vertices[i]->Position[0]; }
			const double DY = Determinant();

			// size, x, y
			for (int i = 0; i < 3; i++) { MTX[i][2] = Simplex->Vertices[i]->Position[1]; }
			const double c = -Determinant();

			const double s = -1.0f / (2.0f * a);
			return new TDelaunayCell(
				Simplex, FVector(s * DX, s * DY, 0),
				FMath::Abs(s) * FMath::Sqrt(DX * DX + DY * DY - 4.0 * a * c));
		}
	};

	class TDelaunayTriangulation3 : public TDelaunayTriangulation<3, FVector, FVector4>
	{
	public:
		TDelaunayTriangulation3() : TDelaunayTriangulation()
		{
		}

	protected:
		double MINOR(const int R0, const int R1, const int R2, const int C0, const int C1, const int C2) const
		{
			return
				MTX[R0][C0] * (MTX[R1][C1] * MTX[R2][C2] - MTX[R2][C1] * MTX[R1][C2]) -
				MTX[R0][C1] * (MTX[R1][C0] * MTX[R2][C2] - MTX[R2][C0] * MTX[R1][C2]) +
				MTX[R0][C2] * (MTX[R1][C0] * MTX[R2][C1] - MTX[R2][C0] * MTX[R1][C1]);
		}

		virtual double Determinant() const override
		{
			return (MTX[0][0] * MINOR(1, 2, 3, 1, 2, 3) -
				MTX[0][1] * MINOR(1, 2, 3, 0, 2, 3) +
				MTX[0][2] * MINOR(1, 2, 3, 0, 1, 3) -
				MTX[0][3] * MINOR(1, 2, 3, 0, 1, 2));
		}

		virtual TDelaunayCell<4, FVector4>* CreateCell(TFSimplex<4, FVector4>* Simplex) override
		{
			// From MathWorld: http://mathworld.wolfram.com/Circumsphere.html

			// x, y, z, 1
			for (int i = 0; i < 4; i++)
			{
				MTX[i][0] = Simplex->Vertices[i]->Position[0];
				MTX[i][1] = Simplex->Vertices[i]->Position[1];
				MTX[i][2] = Simplex->Vertices[i]->Position[2];
				MTX[i][3] = 1;
			}
			const double a = Determinant();

			// size, y, z, 1
			for (int i = 0; i < 4; i++) { MTX[i][0] = Simplex->Vertices[i]->Position[3]; } //->SqrMagnitude();
			const double DX = Determinant();

			// size, x, z, 1
			for (int i = 0; i < 4; i++) { MTX[i][1] = Simplex->Vertices[i]->Position[0]; }
			const double DY = -Determinant();

			// size, x, y, 1
			for (int i = 0; i < 4; i++) { MTX[i][2] = Simplex->Vertices[i]->Position[1]; }
			const double DZ = Determinant();

			//size, x, y, z
			for (int i = 0; i < 4; i++) { MTX[i][3] = Simplex->Vertices[i]->Position[2]; }
			const double c = Determinant();

			const double s = -1.0f / (2.0f * a);
			return new TDelaunayCell(
				Simplex,
				FVector4(s * DX, s * DY, s * DZ, 0),
				FMath::Abs(s) * FMath::Sqrt(DX * DX + DY * DY + DZ * DZ - 4 * a * c));
		}
	};
}