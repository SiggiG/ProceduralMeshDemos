// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example cube mesh

#include "SimpleCubeActor.h"
#include "RuntimeMeshProviderStatic.h"

ASimpleCubeActor::ASimpleCubeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	StaticProvider = CreateDefaultSubobject<URuntimeMeshProviderStatic>(TEXT("RuntimeMeshProvider-Static"));
}

void ASimpleCubeActor::OnConstruction(const FTransform& Transform)
{
	GenerateMesh();
}

void ASimpleCubeActor::SetupMeshBuffers()
{
	const int32 VertexCount = 6 * 4; // 6 sides on a cube, 4 verts each
	Positions.AddUninitialized(VertexCount);
	Triangles.AddUninitialized(6 * 2 * 3); // 2x triangles per cube side, 3 verts each
	Normals.AddUninitialized(VertexCount);
	Tangents.AddUninitialized(VertexCount);
	TexCoords.AddUninitialized(VertexCount);
}

void ASimpleCubeActor::GenerateMesh()
{
	// The number of vertices or polygons wont change at runtime, so we'll just allocate the arrays once
	if (!bHaveBuffersBeenInitialized)
	{
		SetupMeshBuffers();
		bHaveBuffersBeenInitialized = true;
	}

	GetRuntimeMeshComponent()->Initialize(StaticProvider);
	GenerateCube(Positions, Triangles, Normals, Tangents, TexCoords, Size);

	TArray<FColor> EmptyColors{};
	StaticProvider->CreateSectionFromComponents(0, 0, 0, Positions, Triangles, Normals, TexCoords, EmptyColors, Tangents, ERuntimeMeshUpdateFrequency::Infrequent, true);
	StaticProvider->SetupMaterialSlot(0, TEXT("CubeMaterial"), Material);
}

void ASimpleCubeActor::GenerateCube(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, FVector InSize)
{
	// NOTE: Unreal uses an upper-left origin UV
	// NOTE: This sample uses a simple UV mapping scheme where each face is the same
	// NOTE: For a normal facing towards us, be build the polygon CCW in the order 0-1-2 then 0-2-3 to complete the quad.
	// Remember in Unreal, X is forwards, Y is to your right and Z is up.

	// Calculate a half offset so we get correct center of object
	const float OffsetX = InSize.X / 2.0f;
	const float OffsetY = InSize.Y / 2.0f;
	const float OffsetZ = InSize.Z / 2.0f;

	// Define the 8 corners of the cube
	const FVector p0 = FVector(OffsetX,  OffsetY, -OffsetZ);
	const FVector p1 = FVector(OffsetX, -OffsetY, -OffsetZ);
	const FVector p2 = FVector(OffsetX, -OffsetY,  OffsetZ);
	const FVector p3 = FVector(OffsetX,  OffsetY,  OffsetZ);
	const FVector p4 = FVector(-OffsetX, OffsetY, -OffsetZ);
	const FVector p5 = FVector(-OffsetX, -OffsetY, -OffsetZ);
	const FVector p6 = FVector(-OffsetX, -OffsetY, OffsetZ);
	const FVector p7 = FVector(-OffsetX, OffsetY, OffsetZ);

	// Now we create 6x faces, 4 vertices each
	int32 VertexOffset = 0;
	int32 TriangleOffset = 0;
	FVector Normal;
	FRuntimeMeshTangent Tangent;

 	// Front (+X) face: 0-1-2-3
	Normal = FVector(1, 0, 0);
	Tangent = FVector(0, 1, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, p0, p1, p2, p3, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Back (-X) face: 5-4-7-6
	Normal = FVector(-1, 0, 0);
	Tangent = FVector(0, -1, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, p5, p4, p7, p6, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Left (-Y) face: 1-5-6-2
	Normal = FVector(0, -1, 0);
	Tangent = FVector(1, 0, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, p1, p5, p6, p2, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Right (+Y) face: 4-0-3-7
	Normal = FVector(0, 1, 0);
	Tangent = FVector(-1, 0, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, p4, p0, p3, p7, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Top (+Z) face: 6-7-3-2
	Normal = FVector(0, 0, 1);
	Tangent = FVector(0, 1, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, p6, p7, p3, p2, VertexOffset, TriangleOffset, Normal, Tangent);

 	// Bottom (-Z) face: 1-0-4-5
	Normal = FVector(0, 0, -1);
	Tangent = FVector(0, -1, 0);
	BuildQuad(InVertices, InTriangles, InNormals, InTangents, InTexCoords, p1, p0, p4, p5, VertexOffset, TriangleOffset, Normal, Tangent);
}

void ASimpleCubeActor::BuildQuad(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, FVector BottomLeft, FVector BottomRight, FVector TopRight, FVector TopLeft, int32& VertexOffset, int32& TriangleOffset, FVector Normal, FRuntimeMeshTangent Tangent)
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
	// // On a cube side, all the vertex normals face the same way
	InNormals[Index1] = InNormals[Index2] = InNormals[Index3] = InNormals[Index4] = Normal;
	InTangents[Index1] = InTangents[Index2] = InTangents[Index3] = InTangents[Index4] = Tangent;
}
