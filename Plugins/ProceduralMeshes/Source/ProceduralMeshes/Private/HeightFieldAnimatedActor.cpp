// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield grid animated with sine and cosine waves

#include "HeightFieldAnimatedActor.h"
#include "Providers/RuntimeMeshProviderStatic.h"

AHeightFieldAnimatedActor::AHeightFieldAnimatedActor()
{
	PrimaryActorTick.bCanEverTick = true;
	StaticProvider = CreateDefaultSubobject<URuntimeMeshProviderStatic>(TEXT("RuntimeMeshProvider-Static"));
	StaticProvider->SetShouldSerializeMeshData(false);
}

void AHeightFieldAnimatedActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GenerateMesh();
}

// This is called when actor is already in level and map is opened
void AHeightFieldAnimatedActor::PostLoad()
{
	Super::PostLoad();
	GenerateMesh();
}

void AHeightFieldAnimatedActor::SetupMeshBuffers()
{
	const int32 NumberOfPoints = (LengthSections + 1) * (WidthSections + 1);
	const int32 VertexCount = (LengthSections + 1) * (WidthSections + 1);
	const int32 TriangleCount = LengthSections * WidthSections * 2 * 3; // 2x3 vertex indexes per quad

	if (VertexCount != Positions.Num())
	{
		Positions.Empty();
		Positions.AddUninitialized(VertexCount);
		Normals.Empty();
		Normals.AddUninitialized(VertexCount);
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

void AHeightFieldAnimatedActor::GeneratePoints()
{
	// Setup example height data
	// Combine variations of sine and cosine to create some variable waves
	// TODO Convert this to use a parallel for
	int32 PointIndex = 0;

	for (int32 X = 0; X < LengthSections + 1; X++)
	{
		for (int32 Y = 0; Y < WidthSections + 1; Y++)
		{
			// Just some quick hardcoded offset numbers in there
			const float ValueOne = FMath::Cos((X + CurrentAnimationFrameX)*ScaleFactor) * FMath::Sin((Y + CurrentAnimationFrameY)*ScaleFactor);
			const float ValueTwo = FMath::Cos((X + CurrentAnimationFrameX*0.7f)*ScaleFactor*2.5f) * FMath::Sin((Y - CurrentAnimationFrameY*0.7f)*ScaleFactor*2.5f);
			const float AvgValue = ((ValueOne + ValueTwo) / 2) * Size.Z;
			HeightValues[PointIndex++] = AvgValue;

			if (AvgValue > MaxHeightValue)
			{
				MaxHeightValue = AvgValue;
			}
		}
	}
}

void AHeightFieldAnimatedActor::Tick(float DeltaSeconds)
{
	if (AnimateMesh)
	{
		CurrentAnimationFrameX += DeltaSeconds * AnimationSpeedX;
		CurrentAnimationFrameY += DeltaSeconds * AnimationSpeedY;
		GenerateMesh();
	}
}

void AHeightFieldAnimatedActor::GenerateMesh()
{
	GetRuntimeMeshComponent()->Initialize(StaticProvider);
	StaticProvider->ClearSection(0, 0);
	
	if (Size.X < 1 || Size.Y < 1 || LengthSections < 1 || WidthSections < 1)
	{
		return;
	}

	SetupMeshBuffers();
	GeneratePoints();

	// TODO Convert this to use fast-past updates instead of regenerating the mesh every frame
	GenerateGrid(Positions, Triangles, Normals, TexCoords, FVector2D(Size.X, Size.Y), LengthSections, WidthSections, HeightValues);
	const TArray<FColor> EmptyColors{};
	const TArray<FRuntimeMeshTangent> EmptyTangents;
	StaticProvider->CreateSectionFromComponents(0, 0, 0, Positions, Triangles, Normals, TexCoords, EmptyColors, EmptyTangents, ERuntimeMeshUpdateFrequency::Infrequent, false);
	StaticProvider->SetupMaterialSlot(0, TEXT("CylinderMaterial"), Material);
}

void AHeightFieldAnimatedActor::GenerateGrid(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FVector2D>& InTexCoords, const FVector2D InSize, const int32 InLengthSections, const int32 InWidthSections, const TArray<float>& InHeightValues)
{
	// Note the coordinates are a bit weird here since I aligned it to the transform (X is forwards or "up", which Y is to the right)
	// Should really fix this up and use standard X, Y coords then transform into object space?
	const FVector2D SectionSize = FVector2D(InSize.X / InLengthSections, InSize.Y / InWidthSections);
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	const float LengthSectionsAsFloat = static_cast<float>(InLengthSections);
	const float WidthSectionsAsFloat = static_cast<float>(InWidthSections);
	
	for (int32 X = 0; X < InLengthSections + 1; X++)
	{
		for (int32 Y = 0; Y < InWidthSections + 1; Y++)
		{
			// Create a new vertex
			const int32 NewVertIndex = VertexIndex++;
			const FVector NewVertex = FVector(X * SectionSize.X, Y * SectionSize.Y, InHeightValues[NewVertIndex]);
			InVertices[NewVertIndex] = NewVertex;

			// Note that Unreal UV origin (0,0) is top left
			const float U = static_cast<float>(X) / LengthSectionsAsFloat;
			const float V = static_cast<float>(Y) / WidthSectionsAsFloat;
			InTexCoords[NewVertIndex] = FVector2D(U, V);

			// Once we've created enough verts we can start adding polygons
			if (X > 0 && Y > 0)
			{
				// Each row is InWidthSections+1 number of points.
				// And we have InLength+1 rows
				// Index of current vertex in position is thus: (X * (InWidthSections + 1)) + Y;
				const int32 TopRightIndex = (X * (InWidthSections + 1)) + Y; // Should be same as VertIndex1!
				const int32 TopLeftIndex = TopRightIndex - 1;
				const int32 BottomRightIndex = ((X - 1) * (InWidthSections + 1)) + Y;
				const int32 BottomLeftIndex = BottomRightIndex - 1;

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
				InNormals[BottomLeftIndex] = InNormals[BottomRightIndex] = InNormals[TopRightIndex] = InNormals[TopLeftIndex] = NormalCurrent;
			}
		}
	}
}
