#pragma once

// Procedural low-poly 3D models, one per evolution stage.
//
// Plain C++ with no engine dependencies, so the geometry can also be compiled and previewed outside
// Unreal (see Tools/stage_preview). The UI widget SEvoStageModel transforms, shades and draws them.
//
// Model space: +Y up, +X is the "front" of creatures, +Z points toward the viewer.
// Every model is normalised to fit inside a sphere of radius 1 around the origin.

#include <cstdint>
#include <vector>

namespace EvoMesh
{
	struct FV3
	{
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;

		FV3() = default;
		FV3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}

		FV3 operator+(const FV3& O) const { return FV3(X + O.X, Y + O.Y, Z + O.Z); }
		FV3 operator-(const FV3& O) const { return FV3(X - O.X, Y - O.Y, Z - O.Z); }
		FV3 operator*(float S) const { return FV3(X * S, Y * S, Z * S); }
		FV3 operator-() const { return FV3(-X, -Y, -Z); }
		FV3& operator+=(const FV3& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
	};

	float Dot(const FV3& A, const FV3& B);
	FV3 Cross(const FV3& A, const FV3& B);
	float Length(const FV3& V);
	FV3 Normalize(const FV3& V);
	FV3 Mul(const FV3& A, const FV3& B);

	/** Colour slots; the widget maps them to the current era's palette. */
	enum ESlot : uint8_t
	{
		SlotPrimary = 0,
		SlotSecondary = 1,
		SlotAccent = 2,   // drawn as glowing / emissive
		SlotDark = 3,
		SlotMid = 4,
		SlotCount = 5
	};

	struct FMesh
	{
		std::vector<FV3> Positions;
		std::vector<float> Flex;          // per vertex, 0..1: how much the vertex sways in the idle animation
		std::vector<uint32_t> Indices;    // 3 per triangle, wound so the normal points outward
		std::vector<uint8_t> TriSlot;     // per triangle colour slot
		std::vector<uint8_t> TriTwoSided; // per triangle: 1 = draw from both sides (fins, blades)

		int32_t NumTriangles() const { return static_cast<int32_t>(TriSlot.size()); }
	};

	/** Builds the model for a stage id (1..20). Unknown ids return a simple sphere. */
	FMesh BuildStageMesh(int32_t StageId);
}
