// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example heightfield grid animated with sine and cosine waves

#include "HeightFieldAnimatedActor.h"

AHeightFieldAnimatedActor::AHeightFieldAnimatedActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	MeshComponent = CreateDefaultSubobject<URuntimeProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);
}

void AHeightFieldAnimatedActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetActorTickEnabled(AnimateMesh);

	if (bRequiresMeshRebuild || MeshComponent->GetNumSections() == 0)
	{
		bMeshCreated = false;
		GenerateMesh();
		bRequiresMeshRebuild = false;
	}
}

#if WITH_EDITOR
void AHeightFieldAnimatedActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetOwnerClass()->IsChildOf(StaticClass()))
	{
		bRequiresMeshRebuild = true;
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AHeightFieldAnimatedActor::PostLoad()
{
	Super::PostLoad();
	SetActorTickEnabled(AnimateMesh);
	bMeshCreated = false;
	GenerateMesh();
	bRequiresMeshRebuild = false;
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
	MaxHeightValue = 0.0f;

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
	if (!IsValid(MeshComponent))
	{
		return;
	}

	if (Size.X < 1 || Size.Y < 1 || LengthSections < 1 || WidthSections < 1)
	{
		MeshComponent->ClearAllMeshSections();
		bMeshCreated = false;
		return;
	}

	SetupMeshBuffers();
	GeneratePoints();

	if (bMeshCreated)
	{
		// Fast path: only recompute positions and normals, skip triangles and UVs
		UpdatePositionsAndNormals(FVector2D(Size.X, Size.Y), LengthSections, WidthSections, HeightValues);
		MeshComponent->UpdateMeshSection(0, Positions, Normals, TexCoords, {}, {}, {}, {}, {});
	}
	else
	{
		// Full rebuild: topology + positions + normals + UVs
		GenerateGrid(FVector2D(Size.X, Size.Y), LengthSections, WidthSections, HeightValues);
		MeshComponent->ClearAllMeshSections();
		MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, {}, false);
		if (Material)
		{
			MeshComponent->SetMaterial(0, Material);
		}
		bMeshCreated = true;
	}
}

void AHeightFieldAnimatedActor::GenerateGrid(const FVector2D InSize, const int32 InLengthSections, const int32 InWidthSections, const TArray<float>& InHeightValues)
{
	const FVector2D SectionSize = FVector2D(InSize.X / InLengthSections, InSize.Y / InWidthSections);
	int32 VertexIndex = 0;
	int32 TriangleIndex = 0;

	const float LengthSectionsAsFloat = static_cast<float>(InLengthSections);
	const float WidthSectionsAsFloat = static_cast<float>(InWidthSections);

	for (int32 X = 0; X < InLengthSections + 1; X++)
	{
		for (int32 Y = 0; Y < InWidthSections + 1; Y++)
		{
			const int32 NewVertIndex = VertexIndex++;
			Positions[NewVertIndex] = FVector(X * SectionSize.X, Y * SectionSize.Y, InHeightValues[NewVertIndex]);

			// Note that Unreal UV origin (0,0) is top left
			const float U = static_cast<float>(X) / LengthSectionsAsFloat;
			const float V = static_cast<float>(Y) / WidthSectionsAsFloat;
			TexCoords[NewVertIndex] = FVector2D(U, V);

			// Once we've created enough verts we can start adding polygons
			if (X > 0 && Y > 0)
			{
				const int32 TopRightIndex = (X * (InWidthSections + 1)) + Y;
				const int32 TopLeftIndex = TopRightIndex - 1;
				const int32 BottomRightIndex = ((X - 1) * (InWidthSections + 1)) + Y;
				const int32 BottomLeftIndex = BottomRightIndex - 1;

				// The order of these (clockwise/counter-clockwise) dictates which way the normal will face.
				Triangles[TriangleIndex++] = BottomLeftIndex;
				Triangles[TriangleIndex++] = TopRightIndex;
				Triangles[TriangleIndex++] = TopLeftIndex;

				Triangles[TriangleIndex++] = BottomLeftIndex;
				Triangles[TriangleIndex++] = BottomRightIndex;
				Triangles[TriangleIndex++] = TopRightIndex;

				// Normals
				const FVector NormalCurrent = FVector::CrossProduct(Positions[BottomLeftIndex] - Positions[TopLeftIndex], Positions[TopLeftIndex] - Positions[TopRightIndex]).GetSafeNormal();
				Normals[BottomLeftIndex] = Normals[BottomRightIndex] = Normals[TopRightIndex] = Normals[TopLeftIndex] = NormalCurrent;
			}
		}
	}
}

void AHeightFieldAnimatedActor::UpdatePositionsAndNormals(const FVector2D InSize, const int32 InLengthSections, const int32 InWidthSections, const TArray<float>& InHeightValues)
{
	const FVector2D SectionSize = FVector2D(InSize.X / InLengthSections, InSize.Y / InWidthSections);
	int32 VertexIndex = 0;

	for (int32 X = 0; X < InLengthSections + 1; X++)
	{
		for (int32 Y = 0; Y < InWidthSections + 1; Y++)
		{
			const int32 NewVertIndex = VertexIndex++;
			Positions[NewVertIndex] = FVector(X * SectionSize.X, Y * SectionSize.Y, InHeightValues[NewVertIndex]);

			if (X > 0 && Y > 0)
			{
				const int32 TopRightIndex = (X * (InWidthSections + 1)) + Y;
				const int32 TopLeftIndex = TopRightIndex - 1;
				const int32 BottomRightIndex = ((X - 1) * (InWidthSections + 1)) + Y;
				const int32 BottomLeftIndex = BottomRightIndex - 1;

				const FVector NormalCurrent = FVector::CrossProduct(Positions[BottomLeftIndex] - Positions[TopLeftIndex], Positions[TopLeftIndex] - Positions[TopRightIndex]).GetSafeNormal();
				Normals[BottomLeftIndex] = Normals[BottomRightIndex] = Normals[TopRightIndex] = Normals[TopLeftIndex] = NormalCurrent;
			}
		}
	}
}
