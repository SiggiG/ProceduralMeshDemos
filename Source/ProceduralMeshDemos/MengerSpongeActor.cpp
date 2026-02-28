// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example Menger sponge fractal mesh

#include "MengerSpongeActor.h"

AMengerSpongeActor::AMengerSpongeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<URuntimeProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);
}

void AMengerSpongeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bRequiresMeshRebuild || MeshComponent->GetNumSections() == 0)
	{
		GenerateMesh();
		bRequiresMeshRebuild = false;
	}
}

#if WITH_EDITOR
void AMengerSpongeActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetOwnerClass()->IsChildOf(StaticClass()))
	{
		bRequiresMeshRebuild = true;
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AMengerSpongeActor::PostLoad()
{
	Super::PostLoad();
	GenerateMesh();
	bRequiresMeshRebuild = false;
}

bool AMengerSpongeActor::IsSolid(const int32 X, const int32 Y, const int32 Z, const int32 N)
{
	// A cell is solid if at every subdivision level, at most one of its three
	// base-3 coordinate digits is 1 (the middle). If two or more are in the
	// middle at any level, the cell is removed (center of a face or the center cube).
	int32 Cx = X;
	int32 Cy = Y;
	int32 Cz = Z;

	for (int32 i = 0; i < N; ++i)
	{
		const int32 NumCenter = (Cx % 3 == 1 ? 1 : 0) + (Cy % 3 == 1 ? 1 : 0) + (Cz % 3 == 1 ? 1 : 0);
		if (NumCenter >= 2)
		{
			return false;
		}
		Cx /= 3;
		Cy /= 3;
		Cz /= 3;
	}

	return true;
}

void AMengerSpongeActor::SetupMeshBuffers(const int32 InFaceCount)
{
	const int32 VertexCount = InFaceCount * 4;
	const int32 TriangleCount = InFaceCount * 6;

	Positions.SetNumUninitialized(VertexCount);
	Normals.SetNumUninitialized(VertexCount);
	Tangents.SetNumUninitialized(VertexCount);
	TexCoords.SetNumUninitialized(VertexCount);
	Triangles.SetNumUninitialized(TriangleCount);
}

void AMengerSpongeActor::GenerateMesh()
{
	if (!IsValid(MeshComponent))
	{
		return;
	}

	MeshComponent->ClearAllMeshSections();

	const int32 ClampedIterations = FMath::Clamp(Iterations, 0, 4);
	const int32 GridSize = FMath::RoundToInt32(FMath::Pow(3.f, ClampedIterations)); // 1, 3, 9, 27, 81
	const float CellSize = Size / static_cast<float>(GridSize);
	const float HalfSize = Size / 2.f;
	const float HalfCell = CellSize / 2.f;

	// Direction offsets for the 6 faces: +X, -X, +Y, -Y, +Z, -Z
	static constexpr int32 DX[] = { 1, -1,  0,  0,  0,  0};
	static constexpr int32 DY[] = { 0,  0,  1, -1,  0,  0};
	static constexpr int32 DZ[] = { 0,  0,  0,  0,  1, -1};

	// First pass: count exposed faces for buffer pre-allocation
	int32 FaceCount = 0;

	for (int32 X = 0; X < GridSize; ++X)
	{
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			for (int32 Z = 0; Z < GridSize; ++Z)
			{
				if (!IsSolid(X, Y, Z, ClampedIterations))
				{
					continue;
				}

				for (int32 Dir = 0; Dir < 6; ++Dir)
				{
					const int32 NX = X + DX[Dir];
					const int32 NY = Y + DY[Dir];
					const int32 NZ = Z + DZ[Dir];

					// Emit face if neighbor is outside the grid or empty
					if (NX < 0 || NX >= GridSize || NY < 0 || NY >= GridSize || NZ < 0 || NZ >= GridSize
						|| !IsSolid(NX, NY, NZ, ClampedIterations))
					{
						FaceCount++;
					}
				}
			}
		}
	}

	if (FaceCount == 0)
	{
		return;
	}

	SetupMeshBuffers(FaceCount);

	// Helper: compute UV from vertex position based on face direction.
	// UVs are projected onto the face plane and normalized to [0,1] across the full sponge Size,
	// so the texture maps continuously across the entire face rather than per sub-cube.
	const float InvSize = 1.f / Size;
	auto ComputeUV = [HalfSize, InvSize](const FVector& P, int32 Dir) -> FVector2D
	{
		switch (Dir)
		{
		case 0: return FVector2D((HalfSize - P.Y) * InvSize, (HalfSize - P.Z) * InvSize); // +X
		case 1: return FVector2D((P.Y + HalfSize) * InvSize, (HalfSize - P.Z) * InvSize); // -X
		case 2: return FVector2D((P.X + HalfSize) * InvSize, (HalfSize - P.Z) * InvSize); // +Y
		case 3: return FVector2D((HalfSize - P.X) * InvSize, (HalfSize - P.Z) * InvSize); // -Y
		case 4: return FVector2D((P.Y + HalfSize) * InvSize, (HalfSize - P.X) * InvSize); // +Z
		case 5: return FVector2D((P.Y + HalfSize) * InvSize, (P.X + HalfSize) * InvSize); // -Z
		default: return FVector2D::ZeroVector;
		}
	};

	// Second pass: emit geometry for exposed faces
	int32 VertexOffset = 0;
	int32 TriangleOffset = 0;

	for (int32 X = 0; X < GridSize; ++X)
	{
		for (int32 Y = 0; Y < GridSize; ++Y)
		{
			for (int32 Z = 0; Z < GridSize; ++Z)
			{
				if (!IsSolid(X, Y, Z, ClampedIterations))
				{
					continue;
				}

				// Cell center in local space (centered around origin)
				const FVector CellCenter(
					-HalfSize + (static_cast<float>(X) + 0.5f) * CellSize,
					-HalfSize + (static_cast<float>(Y) + 0.5f) * CellSize,
					-HalfSize + (static_cast<float>(Z) + 0.5f) * CellSize
				);

				for (int32 Dir = 0; Dir < 6; ++Dir)
				{
					const int32 NX = X + DX[Dir];
					const int32 NY = Y + DY[Dir];
					const int32 NZ = Z + DZ[Dir];

					if (NX >= 0 && NX < GridSize && NY >= 0 && NY < GridSize && NZ >= 0 && NZ < GridSize
						&& IsSolid(NX, NY, NZ, ClampedIterations))
					{
						continue;
					}

					// Emit a quad for this exposed face
					// Dir: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
					FVector Normal;
					FProcMeshTangent Tangent;
					FVector P0, P1, P2, P3; // BottomLeft, BottomRight, TopRight, TopLeft

					switch (Dir)
					{
					case 0: // +X face
						Normal = FVector(1, 0, 0);
						Tangent = FProcMeshTangent(FVector(0, 1, 0), false);
						P0 = CellCenter + FVector(HalfCell,  HalfCell, -HalfCell);
						P1 = CellCenter + FVector(HalfCell, -HalfCell, -HalfCell);
						P2 = CellCenter + FVector(HalfCell, -HalfCell,  HalfCell);
						P3 = CellCenter + FVector(HalfCell,  HalfCell,  HalfCell);
						break;
					case 1: // -X face
						Normal = FVector(-1, 0, 0);
						Tangent = FProcMeshTangent(FVector(0, -1, 0), false);
						P0 = CellCenter + FVector(-HalfCell, -HalfCell, -HalfCell);
						P1 = CellCenter + FVector(-HalfCell,  HalfCell, -HalfCell);
						P2 = CellCenter + FVector(-HalfCell,  HalfCell,  HalfCell);
						P3 = CellCenter + FVector(-HalfCell, -HalfCell,  HalfCell);
						break;
					case 2: // +Y face
						Normal = FVector(0, 1, 0);
						Tangent = FProcMeshTangent(FVector(-1, 0, 0), false);
						P0 = CellCenter + FVector(-HalfCell, HalfCell, -HalfCell);
						P1 = CellCenter + FVector( HalfCell, HalfCell, -HalfCell);
						P2 = CellCenter + FVector( HalfCell, HalfCell,  HalfCell);
						P3 = CellCenter + FVector(-HalfCell, HalfCell,  HalfCell);
						break;
					case 3: // -Y face
						Normal = FVector(0, -1, 0);
						Tangent = FProcMeshTangent(FVector(1, 0, 0), false);
						P0 = CellCenter + FVector( HalfCell, -HalfCell, -HalfCell);
						P1 = CellCenter + FVector(-HalfCell, -HalfCell, -HalfCell);
						P2 = CellCenter + FVector(-HalfCell, -HalfCell,  HalfCell);
						P3 = CellCenter + FVector( HalfCell, -HalfCell,  HalfCell);
						break;
					case 4: // +Z face
						Normal = FVector(0, 0, 1);
						Tangent = FProcMeshTangent(FVector(0, 1, 0), false);
						P0 = CellCenter + FVector(-HalfCell, -HalfCell, HalfCell);
						P1 = CellCenter + FVector(-HalfCell,  HalfCell, HalfCell);
						P2 = CellCenter + FVector( HalfCell,  HalfCell, HalfCell);
						P3 = CellCenter + FVector( HalfCell, -HalfCell, HalfCell);
						break;
					case 5: // -Z face
						Normal = FVector(0, 0, -1);
						Tangent = FProcMeshTangent(FVector(0, -1, 0), false);
						P0 = CellCenter + FVector( HalfCell, -HalfCell, -HalfCell);
						P1 = CellCenter + FVector( HalfCell,  HalfCell, -HalfCell);
						P2 = CellCenter + FVector(-HalfCell,  HalfCell, -HalfCell);
						P3 = CellCenter + FVector(-HalfCell, -HalfCell, -HalfCell);
						break;
					}

					BuildQuad(Positions, Triangles, Normals, Tangents, TexCoords, P0, P1, P2, P3, VertexOffset, TriangleOffset, Normal, Tangent);

					// Overwrite per-cell UVs with global face-projected UVs
					TexCoords[VertexOffset - 4] = ComputeUV(P0, Dir);
					TexCoords[VertexOffset - 3] = ComputeUV(P1, Dir);
					TexCoords[VertexOffset - 2] = ComputeUV(P2, Dir);
					TexCoords[VertexOffset - 1] = ComputeUV(P3, Dir);
				}
			}
		}
	}

	MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, Tangents, false);
	if (Material)
	{
		MeshComponent->SetMaterial(0, Material);
	}
}

void AMengerSpongeActor::BuildQuad(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FProcMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector BottomLeft, const FVector BottomRight, const FVector TopRight, const FVector TopLeft, int32& VertexOffset, int32& TriangleOffset, const FVector Normal, const FProcMeshTangent Tangent)
{
	const int32 Index1 = VertexOffset++;
	const int32 Index2 = VertexOffset++;
	const int32 Index3 = VertexOffset++;
	const int32 Index4 = VertexOffset++;
	InVertices[Index1] = BottomLeft;
	InVertices[Index2] = BottomRight;
	InVertices[Index3] = TopRight;
	InVertices[Index4] = TopLeft;
	InTexCoords[Index1] = FVector2D(0.0f, 1.0f);
	InTexCoords[Index2] = FVector2D(1.0f, 1.0f);
	InTexCoords[Index3] = FVector2D(1.0f, 0.0f);
	InTexCoords[Index4] = FVector2D(0.0f, 0.0f);
	InTriangles[TriangleOffset++] = Index1;
	InTriangles[TriangleOffset++] = Index2;
	InTriangles[TriangleOffset++] = Index3;
	InTriangles[TriangleOffset++] = Index1;
	InTriangles[TriangleOffset++] = Index3;
	InTriangles[TriangleOffset++] = Index4;
	// On a cube side, all the vertex normals face the same way
	InNormals[Index1] = InNormals[Index2] = InNormals[Index3] = InNormals[Index4] = Normal;
	InTangents[Index1] = InTangents[Index2] = InTangents[Index3] = InTangents[Index4] = Tangent;
}
