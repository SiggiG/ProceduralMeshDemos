// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example cylinder strip mesh with smooth joints at corners

#include "SmoothCylinderStripActor.h"

ASmoothCylinderStripActor::ASmoothCylinderStripActor()
{
	PrimaryActorTick.bCanEverTick = false;
	MeshComponent = CreateDefaultSubobject<URuntimeProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(MeshComponent);
}

void ASmoothCylinderStripActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PreCacheCrossSection();
	GenerateMesh();
}

void ASmoothCylinderStripActor::PostLoad()
{
	Super::PostLoad();
	PreCacheCrossSection();
	GenerateMesh();
}

void ASmoothCylinderStripActor::PreCacheCrossSection()
{
	if (LastCachedCrossSectionCount == RadialSegmentCount)
	{
		return;
	}

	const float AngleBetweenQuads = (2.0f / static_cast<float>(RadialSegmentCount)) * PI;
	CachedCrossSectionPoints.Empty();
	CachedCrossSectionPoints.Reserve(RadialSegmentCount + 1);

	// Generate cross-section points including the wrap-around point for the UV seam
	for (int32 i = 0; i <= RadialSegmentCount; ++i)
	{
		const float Angle = static_cast<float>(i) * AngleBetweenQuads;
		CachedCrossSectionPoints.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f));
	}

	LastCachedCrossSectionCount = RadialSegmentCount;
}

void ASmoothCylinderStripActor::GenerateMesh()
{
	if (!IsValid(MeshComponent))
	{
		return;
	}

	MeshComponent->ClearAllMeshSections();

	if (LinePoints.Num() < 2 || RadialSegmentCount < 3)
	{
		return;
	}

	const int32 NumSegments = LinePoints.Num() - 1;

	// Compute segment directions and lengths
	TArray<FVector> SegDirs;
	SegDirs.Reserve(NumSegments);

	for (int32 i = 0; i < NumSegments; ++i)
	{
		const FVector Delta = LinePoints[i + 1] - LinePoints[i];
		const float Len = Delta.Size();
		SegDirs.Add(Len > KINDA_SMALL_NUMBER ? Delta / Len : FVector::UpVector);
	}

	// Helper: create quaternion that rotates Z-up to the given direction
	auto MakeQuat = [](const FVector& Dir) -> FQuat
	{
		return FQuat::FindBetweenNormals(FVector::UpVector, Dir);
	};

	// --- Build ring list ---
	// Each ring is a cross-section placement: center position, orientation, and V texture coordinate.
	// Straight sections are implicitly formed by the quad strip between consecutive rings.
	// At corner joints, multiple rings with slerped orientations create a smooth arc.
	//
	// UV convention (matches CylinderStripActor):
	//   U: wraps around circumference, 1→0
	//   V: 1.0 at segment start, 0.0 at segment end — resets per segment
	// At each joint, the last ring of the outgoing segment has V=0 and the first ring
	// of the incoming segment has V=1. For smooth joints, intermediate rings interpolate
	// V from 0→1, tiling the texture once across the joint arc.
	struct FRing
	{
		FVector Center;
		FQuat Orientation;
		float V;
	};

	TArray<FRing> Rings;
	Rings.Reserve(LinePoints.Num() * 2 + (NumSegments - 1) * FMath::Max(JointSegments, 1));

	const float AngleThreshold = FMath::DegreesToRadians(2.f);

	// First endpoint ring — start of segment 0
	Rings.Add({LinePoints[0], MakeQuat(SegDirs[0]), 1.f});

	// Interior joints
	for (int32 i = 0; i < NumSegments - 1; ++i)
	{
		const FVector& DirIn = SegDirs[i];
		const FVector& DirOut = SegDirs[i + 1];
		const FVector& JointPt = LinePoints[i + 1];

		const float CosAlpha = FMath::Clamp(FVector::DotProduct(DirIn, DirOut), -1.f, 1.f);
		const float Alpha = FMath::Acos(CosAlpha);

		if (Alpha < AngleThreshold)
		{
			// Nearly straight — two coincident rings to reset V for the next segment
			Rings.Add({JointPt, MakeQuat(DirIn), 0.f});   // end of segment i
			Rings.Add({JointPt, MakeQuat(DirOut), 1.f});   // start of segment i+1
		}
		else if (JointSegments <= 0 || Alpha > PI - AngleThreshold)
		{
			// Sharp miter or near-180 turn — two rings at the miter orientation
			FVector MiterDir = (DirIn + DirOut);
			if (MiterDir.SizeSquared() < KINDA_SMALL_NUMBER)
			{
				MiterDir = DirOut;
			}
			const FQuat MiterQ = MakeQuat(MiterDir.GetSafeNormal());
			Rings.Add({JointPt, MiterQ, 0.f});   // end of segment i
			Rings.Add({JointPt, MiterQ, 1.f});   // start of segment i+1
		}
		else
		{
			// Smooth spherical joint: slerp orientation from incoming to outgoing.
			// V goes from 0 (end of incoming segment) to 1 (start of outgoing segment),
			// tiling the texture once across the joint arc.
			const FQuat QIn = MakeQuat(DirIn);
			const FQuat QOut = MakeQuat(DirOut);

			for (int32 j = 0; j <= JointSegments; ++j)
			{
				const float T = static_cast<float>(j) / static_cast<float>(JointSegments);
				const FQuat Q = FQuat::Slerp(QIn, QOut, T);
				Rings.Add({JointPt, Q, T});
			}
		}
	}

	// Last endpoint ring — end of last segment
	Rings.Add({LinePoints.Last(), MakeQuat(SegDirs.Last()), 0.f});

	// --- Allocate mesh buffers ---
	const int32 NumRings = Rings.Num();
	const int32 VertsPerRing = RadialSegmentCount + 1; // +1 for UV seam duplicate
	const int32 TotalVerts = NumRings * VertsPerRing;
	const int32 TotalIndices = (NumRings - 1) * RadialSegmentCount * 6;

	Positions.SetNumUninitialized(TotalVerts);
	Normals.SetNumUninitialized(TotalVerts);
	Tangents.SetNumUninitialized(TotalVerts);
	TexCoords.SetNumUninitialized(TotalVerts);
	Triangles.SetNumUninitialized(TotalIndices);

	// --- Fill vertex data ---
	const float UStep = 1.f / static_cast<float>(RadialSegmentCount);

	for (int32 RingIdx = 0; RingIdx < NumRings; ++RingIdx)
	{
		const FRing& Ring = Rings[RingIdx];
		const int32 BaseVert = RingIdx * VertsPerRing;

		// Tube direction for tangent computation
		const FVector TubeDir = Ring.Orientation.RotateVector(FVector::UpVector);

		for (int32 j = 0; j <= RadialSegmentCount; ++j)
		{
			const int32 VI = BaseVert + j;

			// Position: rotate cached cross-section point into world space
			const FVector LocalPos = CachedCrossSectionPoints[j] * Radius;
			const FVector WorldOffset = Ring.Orientation.RotateVector(LocalPos);
			Positions[VI] = Ring.Center + WorldOffset;

			// Normal: radial outward from tube center (smooth normals)
			Normals[VI] = WorldOffset.GetSafeNormal();

			// Tangent: along the tube direction (for correct normal mapping)
			Tangents[VI] = FProcMeshTangent(TubeDir, false);

			// UV: U wraps around circumference 1→0, V runs along each segment 1→0
			TexCoords[VI] = FVector2D(1.f - static_cast<float>(j) * UStep, Ring.V);
		}
	}

	// --- Fill index data ---
	// Connect each pair of adjacent rings with a quad strip
	int32 TriIdx = 0;
	for (int32 RingIdx = 0; RingIdx < NumRings - 1; ++RingIdx)
	{
		const int32 Base1 = RingIdx * VertsPerRing;
		const int32 Base2 = (RingIdx + 1) * VertsPerRing;

		for (int32 j = 0; j < RadialSegmentCount; ++j)
		{
			const int32 V0 = Base1 + j;       // current ring, current quad
			const int32 V1 = Base1 + j + 1;   // current ring, next quad
			const int32 V2 = Base2 + j + 1;   // next ring, next quad
			const int32 V3 = Base2 + j;        // next ring, current quad

			// Two triangles per quad — winding matches CylinderStripActor
			Triangles[TriIdx++] = V3;
			Triangles[TriIdx++] = V2;
			Triangles[TriIdx++] = V0;

			Triangles[TriIdx++] = V2;
			Triangles[TriIdx++] = V1;
			Triangles[TriIdx++] = V0;
		}
	}

	MeshComponent->CreateMeshSection_LinearColor(0, Positions, Triangles, Normals, TexCoords, {}, {}, {}, {}, Tangents, false);
	MeshComponent->SetMaterial(0, Material);
}
