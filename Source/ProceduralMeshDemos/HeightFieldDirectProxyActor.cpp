// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example heightfield grid animated with sine and cosine waves
// Uses a custom FPrimitiveSceneProxy for direct GPU buffer updates (single copy path)

#include "HeightFieldDirectProxyActor.h"
#include "DirectProxyMeshComponent.h"

AHeightFieldDirectProxyActor::AHeightFieldDirectProxyActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	MeshComponent = CreateDefaultSubobject<UDirectProxyMeshComponent>(TEXT("DirectProxyMesh"));
	SetRootComponent(MeshComponent);
}

void AHeightFieldDirectProxyActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetActorTickEnabled(AnimateMesh);

	if (bRequiresMeshRebuild || !bMeshCreated)
	{
		bMeshCreated = false;
		GenerateMesh();
		bRequiresMeshRebuild = false;
	}
}

#if WITH_EDITOR
void AHeightFieldDirectProxyActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetOwnerClass()->IsChildOf(StaticClass()))
	{
		bRequiresMeshRebuild = true;
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AHeightFieldDirectProxyActor::PostLoad()
{
	Super::PostLoad();
	SetActorTickEnabled(AnimateMesh);
	bMeshCreated = false;
	GenerateMesh();
	bRequiresMeshRebuild = false;
}

void AHeightFieldDirectProxyActor::Tick(float DeltaSeconds)
{
	if (AnimateMesh)
	{
		CurrentAnimationFrameX += DeltaSeconds * AnimationSpeedX;
		CurrentAnimationFrameY += DeltaSeconds * AnimationSpeedY;
		GenerateMesh();
	}
}

void AHeightFieldDirectProxyActor::FillPositionsAndNormals(
	TArray<FVector3f>& OutPositions, TArray<FVector3f>& OutNormals,
	const FVector2D& SectionSize)
{
	int32 VertexIndex = 0;
	for (int32 X = 0; X < LengthSections + 1; X++)
	{
		for (int32 Y = 0; Y < WidthSections + 1; Y++)
		{
			const int32 Idx = VertexIndex++;

			// Inline height computation (avoids intermediate HeightValues array)
			const float ValueOne = FMath::Cos((X + CurrentAnimationFrameX) * ScaleFactor) * FMath::Sin((Y + CurrentAnimationFrameY) * ScaleFactor);
			const float ValueTwo = FMath::Cos((X + CurrentAnimationFrameX * 0.7f) * ScaleFactor * 2.5f) * FMath::Sin((Y - CurrentAnimationFrameY * 0.7f) * ScaleFactor * 2.5f);
			const float HeightValue = ((ValueOne + ValueTwo) * 0.5f) * Size.Z;

			OutPositions[Idx] = FVector3f(X * SectionSize.X, Y * SectionSize.Y, HeightValue);

			if (X > 0 && Y > 0)
			{
				const int32 TopRight = (X * (WidthSections + 1)) + Y;
				const int32 TopLeft = TopRight - 1;
				const int32 BottomRight = ((X - 1) * (WidthSections + 1)) + Y;
				const int32 BottomLeft = BottomRight - 1;

				const FVector3f NormalCurrent = FVector3f::CrossProduct(
					OutPositions[BottomLeft] - OutPositions[TopLeft],
					OutPositions[TopLeft] - OutPositions[TopRight]).GetSafeNormal();
				OutNormals[BottomLeft] = OutNormals[BottomRight] = OutNormals[TopRight] = OutNormals[TopLeft] = NormalCurrent;
			}
		}
	}
}

void AHeightFieldDirectProxyActor::GenerateMesh()
{
	if (!IsValid(MeshComponent))
	{
		return;
	}

	if (Size.X < 1 || Size.Y < 1 || LengthSections < 1 || WidthSections < 1)
	{
		bMeshCreated = false;
		return;
	}

	const int32 NumVerts = (LengthSections + 1) * (WidthSections + 1);
	const FVector2D SectionSize = FVector2D(Size.X / LengthSections, Size.Y / WidthSections);

	if (!bMeshCreated)
	{
		// Full rebuild: topology + positions + normals + UVs
		const int32 TriangleCount = LengthSections * WidthSections * 2 * 3;
		const float LengthSectionsF = static_cast<float>(LengthSections);
		const float WidthSectionsF = static_cast<float>(WidthSections);

		TArray<uint32> Indices;
		TArray<FVector2f> TexCoords;
		Indices.AddUninitialized(TriangleCount);
		TexCoords.AddUninitialized(NumVerts);

		// Resize persistent buffers
		Positions.SetNumUninitialized(NumVerts);
		Normals.SetNumUninitialized(NumVerts);

		// Build topology (indices + UVs)
		int32 TriangleIndex = 0;
		for (int32 X = 0; X < LengthSections + 1; X++)
		{
			for (int32 Y = 0; Y < WidthSections + 1; Y++)
			{
				const int32 Idx = X * (WidthSections + 1) + Y;
				TexCoords[Idx] = FVector2f(static_cast<float>(X) / LengthSectionsF, static_cast<float>(Y) / WidthSectionsF);

				if (X > 0 && Y > 0)
				{
					const int32 TopRight = (X * (WidthSections + 1)) + Y;
					const int32 TopLeft = TopRight - 1;
					const int32 BottomRight = ((X - 1) * (WidthSections + 1)) + Y;
					const int32 BottomLeft = BottomRight - 1;

					Indices[TriangleIndex++] = BottomLeft;
					Indices[TriangleIndex++] = TopRight;
					Indices[TriangleIndex++] = TopLeft;

					Indices[TriangleIndex++] = BottomLeft;
					Indices[TriangleIndex++] = BottomRight;
					Indices[TriangleIndex++] = TopRight;
				}
			}
		}

		// Fill positions + normals
		FillPositionsAndNormals(Positions, Normals, SectionSize);

		// Sync material and upload
		MeshComponent->SetMaterial(0, Material);
		MeshComponent->SetStaticTopology(MoveTemp(Indices), MoveTemp(TexCoords), NumVerts);
		MeshComponent->UpdateDynamicData(Positions, Normals);

		// Set analytical bounds: XY from grid size, Z conservative from wave amplitude
		MeshComponent->SetFixedBounds(FBox(FVector(0, 0, -Size.Z), FVector(Size.X, Size.Y, Size.Z)));

		bMeshCreated = true;
	}
	else
	{
		// Fast path: only recompute positions and normals (no allocations)
		FillPositionsAndNormals(Positions, Normals, SectionSize);
		MeshComponent->UpdateDynamicData(Positions, Normals);
	}
}
