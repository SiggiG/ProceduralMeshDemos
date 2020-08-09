// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield generated with noise

#include "HeightFieldNoiseActor.h"
#include "Providers/RuntimeMeshProviderStatic.h"

AHeightFieldNoiseActor::AHeightFieldNoiseActor()
{
	PrimaryActorTick.bCanEverTick = false;
	StaticProvider = CreateDefaultSubobject<URuntimeMeshProviderStatic>(TEXT("RuntimeMeshProvider-Static"));
	StaticProvider->SetShouldSerializeMeshData(false);
}

void AHeightFieldNoiseActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GenerateMesh();
}

void AHeightFieldNoiseActor::SetupMeshBuffers()
{
	const int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);
	const int32 VertexCount = LengthSections * WidthSections * 4; // 4x vertices per quad/section
	const int32 TriangleCount = LengthSections * WidthSections * 2 * 3; // 2x3 vertex indexes per quad

	if (VertexCount != Positions.Num())
	{
		Positions.Empty();
		Positions.AddUninitialized(VertexCount);
		Normals.Empty();
		Normals.AddUninitialized(VertexCount);
		Tangents.Empty();
		Tangents.AddUninitialized(VertexCount);
		TexCoords.Empty();
		TexCoords.AddUninitialized(VertexCount);
	}

	if (TriangleCount != Triangles.Num())
	{
		Triangles.Empty();
		Triangles.AddUninitialized(TriangleCount);
	}

	if (NumberOfPoints != HeightValues.Num())
	{
		HeightValues.Empty();
		HeightValues.AddUninitialized(NumberOfPoints);
	}
}

void AHeightFieldNoiseActor::GeneratePoints()
{
	RngStream.Initialize(RandomSeed);

	// Setup example height data
	const int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);

	// Fill height data with random values
	for (int32 i = 0; i < NumberOfPoints; i++)
	{
		HeightValues[i] = RngStream.FRandRange(0, Size.Z);
	}
}

void AHeightFieldNoiseActor::GenerateMesh()
{
	GetRuntimeMeshComponent()->Initialize(StaticProvider);
	StaticProvider->ClearSection(0, 0);
	
	if (Size.X < 1 || Size.Y < 1 || LengthSections < 1 || WidthSections < 1)
	{
		return;
	}

	SetupMeshBuffers();
	GeneratePoints();
	GenerateGrid(Positions, Triangles, Normals, Tangents, TexCoords, FVector2D(Size.X, Size.Y), LengthSections, WidthSections, HeightValues);

	const TArray<FColor> EmptyColors{};
	StaticProvider->CreateSectionFromComponents(0, 0, 0, Positions, Triangles, Normals, TexCoords, EmptyColors, Tangents, ERuntimeMeshUpdateFrequency::Infrequent, false);
	StaticProvider->SetupMaterialSlot(0, TEXT("CylinderMaterial"), Material);
}

void AHeightFieldNoiseActor::GenerateGrid(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector2D InSize, const int32 InLengthSections, const int32 InWidthSections, const TArray<float>& InHeightValues)
{
	// Note the coordinates are a bit weird here since I aligned it to the transform (X is forwards or "up", which Y is to the right)
	// Should really fix this up and use standard X, Y coords then transform into object space?
	const FVector2D SectionSize = FVector2D(InSize.X / InLengthSections, InSize.Y / InWidthSections);
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	for (int32 X = 0; X < InLengthSections; X++)
	{
		for (int32 Y = 0; Y < InWidthSections; Y++)
		{
			// Setup a quad
			const int32 BottomLeftIndex = VertexIndex++;
			const int32 BottomRightIndex = VertexIndex++;
			const int32 TopRightIndex = VertexIndex++;
			const int32 TopLeftIndex = VertexIndex++;

			const int32 NoiseIndex_BottomLeft = (X * InWidthSections) + Y;
			const int32 NoiseIndex_BottomRight = NoiseIndex_BottomLeft + 1;
			const int32 NoiseIndex_TopLeft = ((X+1) * InWidthSections) + Y;
			const int32 NoiseIndex_TopRight = NoiseIndex_TopLeft + 1;

			const FVector PBottomLeft = FVector(X * SectionSize.X, Y * SectionSize.Y, InHeightValues[NoiseIndex_BottomLeft]);
			const FVector PBottomRight = FVector(X * SectionSize.X, (Y+1) * SectionSize.Y, InHeightValues[NoiseIndex_BottomRight]);
			const FVector PTopRight = FVector((X + 1) * SectionSize.X, (Y + 1) * SectionSize.Y, InHeightValues[NoiseIndex_TopRight]);
			const FVector PTopLeft = FVector((X+1) * SectionSize.X, Y * SectionSize.Y, InHeightValues[NoiseIndex_TopLeft]);
			
			InVertices[BottomLeftIndex] = PBottomLeft;
			InVertices[BottomRightIndex] = PBottomRight;
			InVertices[TopRightIndex] = PTopRight;
			InVertices[TopLeftIndex] = PTopLeft;

			// Note that Unreal UV origin (0,0) is top left
			InTexCoords[BottomLeftIndex] = FVector2D(static_cast<float>(X) / static_cast<float>(InLengthSections), static_cast<float>(Y) / static_cast<float>(InWidthSections));
			InTexCoords[BottomRightIndex] = FVector2D(static_cast<float>(X) / static_cast<float>(InLengthSections), static_cast<float>(Y + 1) / static_cast<float>(InWidthSections));
			InTexCoords[TopRightIndex] = FVector2D(static_cast<float>(X + 1) / static_cast<float>(InLengthSections), static_cast<float>(Y + 1) / static_cast<float>(InWidthSections));
			InTexCoords[TopLeftIndex] = FVector2D(static_cast<float>(X + 1) / static_cast<float>(InLengthSections), static_cast<float>(Y) / static_cast<float>(InWidthSections));

			// Now create two triangles from those four vertices
			// The order of these (clockwise/counter-clockwise) dictates which way the normal will face. 
			InTriangles[TriangleIndex++] = BottomLeftIndex;
			InTriangles[TriangleIndex++] = TopRightIndex;
			InTriangles[TriangleIndex++] = TopLeftIndex;

			InTriangles[TriangleIndex++] = BottomLeftIndex;
			InTriangles[TriangleIndex++] = BottomRightIndex;
			InTriangles[TriangleIndex++] = TopRightIndex;

			// Normals
			const FVector NormalCurrent = FVector::CrossProduct(InVertices[BottomLeftIndex] - InVertices[TopLeftIndex], InVertices[TopLeftIndex] - InVertices[TopRightIndex]).GetSafeNormal();

			// If not smoothing we just set the vertex normal to the same normal as the polygon they belong to
			InNormals[BottomLeftIndex] = InNormals[BottomRightIndex] = InNormals[TopRightIndex] = InNormals[TopLeftIndex] = NormalCurrent;

			// Tangents (perpendicular to the surface)
			const FVector SurfaceTangent = (PBottomLeft - PBottomRight).GetSafeNormal();
			InTangents[BottomLeftIndex] = InTangents[BottomRightIndex] = InTangents[TopRightIndex] = InTangents[TopLeftIndex] = SurfaceTangent;
		}
	}
}
