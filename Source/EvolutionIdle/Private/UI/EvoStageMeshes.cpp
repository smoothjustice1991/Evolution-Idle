#include "UI/EvoStageMeshes.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>
#include <utility>

namespace EvoMesh
{
	namespace
	{
		constexpr float kPi = 3.14159265358979f;
		constexpr float kTwoPi = 2.f * kPi;
	}

	float Dot(const FV3& A, const FV3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	FV3 Cross(const FV3& A, const FV3& B) { return FV3(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X); }
	float Length(const FV3& V) { return std::sqrt(Dot(V, V)); }
	FV3 Normalize(const FV3& V)
	{
		const float L = Length(V);
		return L > 1e-8f ? V * (1.f / L) : FV3(0.f, 1.f, 0.f);
	}
	FV3 Mul(const FV3& A, const FV3& B) { return FV3(A.X * B.X, A.Y * B.Y, A.Z * B.Z); }

	namespace
	{
		/** Rotation by Euler angles in radians, applied roll (Z), then pitch (X), then yaw (Y). */
		struct FRot
		{
			float Pitch = 0.f;
			float Yaw = 0.f;
			float Roll = 0.f;

			FRot() = default;
			FRot(float InPitch, float InYaw, float InRoll) : Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}

			FV3 Apply(FV3 V) const
			{
				if (Roll != 0.f)
				{
					const float C = std::cos(Roll), S = std::sin(Roll);
					V = FV3(V.X * C - V.Y * S, V.X * S + V.Y * C, V.Z);
				}
				if (Pitch != 0.f)
				{
					const float C = std::cos(Pitch), S = std::sin(Pitch);
					V = FV3(V.X, V.Y * C - V.Z * S, V.Y * S + V.Z * C);
				}
				if (Yaw != 0.f)
				{
					const float C = std::cos(Yaw), S = std::sin(Yaw);
					V = FV3(V.X * C + V.Z * S, V.Y, -V.X * S + V.Z * C);
				}
				return V;
			}
		};

		/** Small deterministic random generator so every model is identical on every run. */
		struct FRng
		{
			uint32_t State;
			explicit FRng(uint32_t Seed) : State(Seed * 2654435761u + 1u) {}
			float Next01()
			{
				State ^= State << 13;
				State ^= State >> 17;
				State ^= State << 5;
				return static_cast<float>(State & 0xFFFFFFu) / static_cast<float>(0x1000000u);
			}
			float Range(float Lo, float Hi) { return Lo + (Hi - Lo) * Next01(); }
		};

		class FBuilder
		{
		public:
			FMesh Mesh;

			uint32_t AddVertex(const FV3& P, float InFlex)
			{
				Mesh.Positions.push_back(P);
				Mesh.Flex.push_back(InFlex);
				return static_cast<uint32_t>(Mesh.Positions.size() - 1);
			}

			/** Adds a triangle, flipping its winding if needed so the normal points away from Inside. */
			void AddTri(uint32_t A, uint32_t B, uint32_t C, uint8_t Slot, const FV3& Inside)
			{
				const FV3& PA = Mesh.Positions[A];
				const FV3& PB = Mesh.Positions[B];
				const FV3& PC = Mesh.Positions[C];
				const FV3 N = Cross(PB - PA, PC - PA);
				if (Length(N) < 1e-9f)
				{
					return; // degenerate (e.g. at a sphere pole)
				}
				const FV3 Centroid = (PA + PB + PC) * (1.f / 3.f);
				if (Dot(N, Centroid - Inside) < 0.f)
				{
					std::swap(B, C);
				}
				Mesh.Indices.push_back(A);
				Mesh.Indices.push_back(B);
				Mesh.Indices.push_back(C);
				Mesh.TriSlot.push_back(Slot);
				Mesh.TriTwoSided.push_back(0);
			}

			void AddTwoSidedTri(uint32_t A, uint32_t B, uint32_t C, uint8_t Slot)
			{
				Mesh.Indices.push_back(A);
				Mesh.Indices.push_back(B);
				Mesh.Indices.push_back(C);
				Mesh.TriSlot.push_back(Slot);
				Mesh.TriTwoSided.push_back(1);
			}

			void Ellipsoid(const FV3& Center, const FV3& Radii, uint8_t Slot, float InFlex = 0.f,
				const FRot& Rot = FRot(), int32_t Segments = 14, int32_t Rings = 9)
			{
				const uint32_t Base = static_cast<uint32_t>(Mesh.Positions.size());
				for (int32_t J = 0; J <= Rings; ++J)
				{
					const float Lat = -kPi * 0.5f + kPi * static_cast<float>(J) / static_cast<float>(Rings);
					for (int32_t I = 0; I < Segments; ++I)
					{
						const float Lon = kTwoPi * static_cast<float>(I) / static_cast<float>(Segments);
						const FV3 Unit(std::cos(Lat) * std::cos(Lon), std::sin(Lat), std::cos(Lat) * std::sin(Lon));
						AddVertex(Center + Rot.Apply(Mul(Unit, Radii)), InFlex);
					}
				}
				for (int32_t J = 0; J < Rings; ++J)
				{
					for (int32_t I = 0; I < Segments; ++I)
					{
						const uint32_t A = Base + J * Segments + I;
						const uint32_t B = Base + J * Segments + (I + 1) % Segments;
						const uint32_t C = Base + (J + 1) * Segments + (I + 1) % Segments;
						const uint32_t D = Base + (J + 1) * Segments + I;
						AddTri(A, D, C, Slot, Center);
						AddTri(A, C, B, Slot, Center);
					}
				}
			}

			void Sphere(const FV3& Center, float Radius, uint8_t Slot, float InFlex = 0.f, int32_t Segments = 12, int32_t Rings = 8)
			{
				Ellipsoid(Center, FV3(Radius, Radius, Radius), Slot, InFlex, FRot(), Segments, Rings);
			}

			/** A tube along a path of points with per-point radius and flex. Radius 0 makes a point (cone tip). */
			void Tube(const std::vector<FV3>& Points, const std::vector<float>& Radii, uint8_t Slot,
				const std::vector<float>& Flexes = {}, int32_t Sides = 8, bool bCapStart = true, bool bCapEnd = true)
			{
				const int32_t Count = static_cast<int32_t>(Points.size());
				if (Count < 2)
				{
					return;
				}
				auto FlexAt = [&Flexes](int32_t Index) { return Flexes.empty() ? 0.f : Flexes[std::min<size_t>(Index, Flexes.size() - 1)]; };
				auto RadiusAt = [&Radii](int32_t Index) { return Radii[std::min<size_t>(Index, Radii.size() - 1)]; };

				// Parallel-transport frames so the tube doesn't twist.
				std::vector<FV3> Tangents(Count);
				for (int32_t I = 0; I < Count; ++I)
				{
					const FV3 Prev = Points[std::max(0, I - 1)];
					const FV3 Next = Points[std::min(Count - 1, I + 1)];
					Tangents[I] = Normalize(Next - Prev);
				}
				FV3 Up = std::fabs(Tangents[0].Y) < 0.9f ? FV3(0.f, 1.f, 0.f) : FV3(1.f, 0.f, 0.f);
				FV3 Normal = Normalize(Cross(Cross(Tangents[0], Up), Tangents[0]));

				const uint32_t Base = static_cast<uint32_t>(Mesh.Positions.size());
				for (int32_t I = 0; I < Count; ++I)
				{
					const FV3& T = Tangents[I];
					Normal = Normalize(Normal - T * Dot(Normal, T));
					const FV3 Binormal = Cross(T, Normal);
					for (int32_t S = 0; S < Sides; ++S)
					{
						const float A = kTwoPi * static_cast<float>(S) / static_cast<float>(Sides);
						const FV3 Offset = (Normal * std::cos(A) + Binormal * std::sin(A)) * RadiusAt(I);
						AddVertex(Points[I] + Offset, FlexAt(I));
					}
				}
				for (int32_t I = 0; I + 1 < Count; ++I)
				{
					const FV3 Inside = (Points[I] + Points[I + 1]) * 0.5f;
					for (int32_t S = 0; S < Sides; ++S)
					{
						const uint32_t A = Base + I * Sides + S;
						const uint32_t B = Base + I * Sides + (S + 1) % Sides;
						const uint32_t C = Base + (I + 1) * Sides + (S + 1) % Sides;
						const uint32_t D = Base + (I + 1) * Sides + S;
						AddTri(A, B, C, Slot, Inside);
						AddTri(A, C, D, Slot, Inside);
					}
				}
				auto Cap = [&](int32_t I, float Direction)
				{
					if (RadiusAt(I) <= 1e-4f)
					{
						return;
					}
					const uint32_t Center = AddVertex(Points[I], FlexAt(I));
					const FV3 Inside = Points[I] - Tangents[I] * (0.01f * Direction);
					for (int32_t S = 0; S < Sides; ++S)
					{
						AddTri(Center, Base + I * Sides + S, Base + I * Sides + (S + 1) % Sides, Slot, Inside);
					}
				};
				if (bCapStart) { Cap(0, -1.f); }
				if (bCapEnd) { Cap(Count - 1, 1.f); }
			}

			void Cone(const FV3& BaseCenter, const FV3& Tip, float Radius, uint8_t Slot, float InFlex = 0.f, int32_t Sides = 7)
			{
				Tube({ BaseCenter, Tip }, { Radius, 0.f }, Slot, { InFlex, InFlex }, Sides, true, false);
			}

			void Box(const FV3& Center, const FV3& Half, uint8_t Slot, const FRot& Rot = FRot(), float InFlex = 0.f)
			{
				uint32_t V[8];
				for (int32_t I = 0; I < 8; ++I)
				{
					const FV3 Corner((I & 1) ? Half.X : -Half.X, (I & 2) ? Half.Y : -Half.Y, (I & 4) ? Half.Z : -Half.Z);
					V[I] = AddVertex(Center + Rot.Apply(Corner), InFlex);
				}
				static const int32_t Faces[6][4] = { {0,1,3,2}, {4,6,7,5}, {0,4,5,1}, {2,3,7,6}, {0,2,6,4}, {1,5,7,3} };
				for (const auto& F : Faces)
				{
					AddTri(V[F[0]], V[F[1]], V[F[2]], Slot, Center);
					AddTri(V[F[0]], V[F[2]], V[F[3]], Slot, Center);
				}
			}

			/** Torus lying in its local XZ plane. */
			void Torus(const FV3& Center, float Major, float Minor, uint8_t Slot, const FRot& Rot = FRot(), int32_t Segments = 28, int32_t Sides = 6)
			{
				const uint32_t Base = static_cast<uint32_t>(Mesh.Positions.size());
				for (int32_t I = 0; I < Segments; ++I)
				{
					const float U = kTwoPi * static_cast<float>(I) / static_cast<float>(Segments);
					const FV3 Dir(std::cos(U), 0.f, std::sin(U));
					for (int32_t S = 0; S < Sides; ++S)
					{
						const float V = kTwoPi * static_cast<float>(S) / static_cast<float>(Sides);
						const FV3 Local = Dir * (Major + Minor * std::cos(V)) + FV3(0.f, Minor * std::sin(V), 0.f);
						AddVertex(Center + Rot.Apply(Local), 0.f);
					}
				}
				for (int32_t I = 0; I < Segments; ++I)
				{
					const float UMid = kTwoPi * (static_cast<float>(I) + 0.5f) / static_cast<float>(Segments);
					const FV3 Inside = Center + Rot.Apply(FV3(std::cos(UMid), 0.f, std::sin(UMid)) * Major);
					for (int32_t S = 0; S < Sides; ++S)
					{
						const uint32_t A = Base + I * Sides + S;
						const uint32_t B = Base + I * Sides + (S + 1) % Sides;
						const uint32_t C = Base + ((I + 1) % Segments) * Sides + (S + 1) % Sides;
						const uint32_t D = Base + ((I + 1) % Segments) * Sides + S;
						AddTri(A, B, C, Slot, Inside);
						AddTri(A, C, D, Slot, Inside);
					}
				}
			}

			/** Elongated octahedron (crystal shard) pointing along Axis. */
			void Gem(const FV3& Center, const FV3& Axis, float Length, float Width, uint8_t Slot, float InFlex = 0.f)
			{
				const FV3 A = Normalize(Axis);
				const FV3 Helper = std::fabs(A.Y) < 0.9f ? FV3(0.f, 1.f, 0.f) : FV3(1.f, 0.f, 0.f);
				const FV3 U = Normalize(Cross(A, Helper));
				const FV3 W = Cross(A, U);
				const uint32_t Top = AddVertex(Center + A * Length, InFlex);
				const uint32_t Bottom = AddVertex(Center - A * (Length * 0.45f), InFlex);
				uint32_t Ring[4];
				for (int32_t I = 0; I < 4; ++I)
				{
					const float Ang = kTwoPi * static_cast<float>(I) / 4.f;
					Ring[I] = AddVertex(Center + (U * std::cos(Ang) + W * std::sin(Ang)) * Width, InFlex);
				}
				for (int32_t I = 0; I < 4; ++I)
				{
					AddTri(Top, Ring[I], Ring[(I + 1) % 4], Slot, Center);
					AddTri(Bottom, Ring[(I + 1) % 4], Ring[I], Slot, Center);
				}
			}

			/** Flat two-sided polygon (fin), fanned from the first point. */
			void Blade(const std::vector<FV3>& Points, uint8_t Slot, const std::vector<float>& Flexes = {})
			{
				const uint32_t Base = static_cast<uint32_t>(Mesh.Positions.size());
				for (size_t I = 0; I < Points.size(); ++I)
				{
					AddVertex(Points[I], Flexes.empty() ? 0.f : Flexes[std::min(I, Flexes.size() - 1)]);
				}
				for (size_t I = 1; I + 1 < Points.size(); ++I)
				{
					AddTwoSidedTri(Base, Base + static_cast<uint32_t>(I), Base + static_cast<uint32_t>(I + 1), Slot);
				}
			}

			/** Faceted sphere (subdivided icosahedron). */
			void Icosphere(const FV3& Center, float Radius, uint8_t Slot, int32_t Subdivisions)
			{
				const float T = (1.f + std::sqrt(5.f)) * 0.5f;
				std::vector<FV3> Verts = {
					{-1, T, 0}, {1, T, 0}, {-1, -T, 0}, {1, -T, 0}, {0, -1, T}, {0, 1, T},
					{0, -1, -T}, {0, 1, -T}, {T, 0, -1}, {T, 0, 1}, {-T, 0, -1}, {-T, 0, 1} };
				std::vector<std::tuple<int, int, int>> Faces = {
					{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},{1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
					{3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1} };
				for (FV3& V : Verts) { V = Normalize(V); }
				for (int32_t Level = 0; Level < Subdivisions; ++Level)
				{
					std::vector<std::tuple<int, int, int>> Next;
					auto Mid = [&Verts](int A, int B)
					{
						Verts.push_back(Normalize((Verts[A] + Verts[B]) * 0.5f));
						return static_cast<int>(Verts.size() - 1);
					};
					for (const auto& F : Faces)
					{
						const int A = std::get<0>(F), B = std::get<1>(F), C = std::get<2>(F);
						const int AB = Mid(A, B), BC = Mid(B, C), CA = Mid(C, A);
						Next.emplace_back(A, AB, CA);
						Next.emplace_back(B, BC, AB);
						Next.emplace_back(C, CA, BC);
						Next.emplace_back(AB, BC, CA);
					}
					Faces.swap(Next);
				}
				const uint32_t Base = static_cast<uint32_t>(Mesh.Positions.size());
				for (const FV3& V : Verts)
				{
					AddVertex(Center + V * Radius, 0.f);
				}
				for (const auto& F : Faces)
				{
					AddTri(Base + std::get<0>(F), Base + std::get<1>(F), Base + std::get<2>(F), Slot, Center);
				}
			}

			/** Scales and recentres the mesh so it fits in a unit sphere. */
			void Normalize01()
			{
				if (Mesh.Positions.empty())
				{
					return;
				}
				FV3 Min = Mesh.Positions[0], Max = Mesh.Positions[0];
				for (const FV3& P : Mesh.Positions)
				{
					Min = FV3(std::min(Min.X, P.X), std::min(Min.Y, P.Y), std::min(Min.Z, P.Z));
					Max = FV3(std::max(Max.X, P.X), std::max(Max.Y, P.Y), std::max(Max.Z, P.Z));
				}
				const FV3 Center = (Min + Max) * 0.5f;
				float Radius = 0.f;
				for (const FV3& P : Mesh.Positions)
				{
					Radius = std::max(Radius, Length(P - Center));
				}
				const float Scale = Radius > 1e-6f ? 1.f / Radius : 1.f;
				for (FV3& P : Mesh.Positions)
				{
					P = (P - Center) * Scale;
				}
			}
		};

		// -------------------------------------------------------------------
		// Shared parts
		// -------------------------------------------------------------------

		void AddCilia(FBuilder& B, const FV3& Center, const FV3& Radii, int32_t Count, uint32_t Seed, uint8_t Slot)
		{
			FRng Rng(Seed);
			for (int32_t I = 0; I < Count; ++I)
			{
				// Spread roughly evenly over the surface (golden spiral), skipping the very top and bottom.
				const float Fi = (static_cast<float>(I) + 0.5f) / static_cast<float>(Count);
				const float Y = 0.8f - 1.6f * Fi;
				const float R = std::sqrt(std::max(0.f, 1.f - Y * Y));
				const float Ang = static_cast<float>(I) * 2.39996f;
				const FV3 Dir(R * std::cos(Ang), Y, R * std::sin(Ang));
				const FV3 Root = Center + Mul(Dir, Radii) * 0.97f;
				const float Len = Rng.Range(0.18f, 0.28f);
				const FV3 Bend(Rng.Range(-0.05f, 0.05f), Rng.Range(-0.05f, 0.05f), Rng.Range(-0.05f, 0.05f));
				B.Tube({ Root, Root + Dir * (Len * 0.5f) + Bend, Root + Dir * Len + Bend * 2.f },
					{ 0.022f, 0.016f, 0.006f }, Slot, { 0.1f, 0.6f, 1.f }, 5, false, false);
			}
		}

		struct FHumanoid
		{
			float Hunch = 0.f;       // forward lean of the upper body (radians)
			float ArmScale = 1.f;    // arm length multiplier
			float HeadScale = 1.f;
			float BrowRidge = 0.f;   // 0..1
			float Jaw = 0.f;         // 0..1 snout/jaw protrusion
			float KneeBend = 0.f;    // 0..1
			uint8_t BodySlot = SlotPrimary;
			uint8_t SkinSlot = SlotMid;
			bool bCyberArm = false;
			bool bCyberLeg = false;
		};

		/** Upright figure facing +Z, feet at y = -1, head top near y = +1. Returns key anchor points. */
		struct FHumanoidAnchors
		{
			FV3 HandL, HandR, Head, Chest, ShoulderL, ShoulderR, HipL, HipR, KneeL, KneeR;
			float HeadRadius = 0.15f;
		};

		FHumanoidAnchors AddHumanoid(FBuilder& B, const FHumanoid& H)
		{
			const FV3 Pivot(0.f, -0.05f, 0.f);
			const FRot Lean(H.Hunch, 0.f, 0.f);
			auto Upper = [&](const FV3& P) { return Pivot + Lean.Apply(P - Pivot); };

			FHumanoidAnchors Out;

			// Hips and legs
			B.Ellipsoid(FV3(0.f, -0.06f, 0.f), FV3(0.2f, 0.13f, 0.13f), H.BodySlot);
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				const FV3 Hip(0.11f * S, -0.1f, 0.f);
				const FV3 Knee(0.12f * S, -0.52f, 0.04f + 0.14f * H.KneeBend);
				const FV3 Ankle(0.12f * S, -0.93f, 0.f);
				const bool bCyber = H.bCyberLeg && Side > 0;
				if (bCyber)
				{
					B.Tube({ Hip, Knee }, { 0.09f, 0.07f }, H.BodySlot);
					B.Sphere(Knee, 0.06f, SlotAccent);
					const FV3 Shin = (Knee + Ankle) * 0.5f;
					B.Box(Shin, FV3(0.06f, 0.2f, 0.07f), SlotDark, FRot(std::atan2(Ankle.Z - Knee.Z, Knee.Y - Ankle.Y), 0.f, 0.f));
				}
				else
				{
					B.Tube({ Hip, Knee, Ankle }, { 0.09f, 0.07f, 0.05f }, H.BodySlot);
				}
				B.Ellipsoid(FV3(0.12f * S, -0.97f, 0.06f), FV3(0.055f, 0.035f, 0.11f), SlotDark);
				if (Side < 0) { Out.HipL = Hip; Out.KneeL = Knee; } else { Out.HipR = Hip; Out.KneeR = Knee; }
			}

			// Torso
			B.Ellipsoid(Upper(FV3(0.f, 0.07f, 0.f)), FV3(0.18f, 0.2f, 0.12f), H.BodySlot, 0.f, Lean);
			B.Ellipsoid(Upper(FV3(0.f, 0.33f, 0.f)), FV3(0.25f, 0.24f, 0.15f), H.BodySlot, 0.f, Lean);
			Out.Chest = Upper(FV3(0.f, 0.35f, 0.15f));

			// Neck and head
			const float HeadR = 0.14f * H.HeadScale;
			const FV3 NeckTop = Upper(FV3(0.f, 0.66f, 0.02f + 0.06f * H.Jaw));
			B.Tube({ Upper(FV3(0.f, 0.55f, 0.f)), NeckTop }, { 0.065f, 0.06f }, H.SkinSlot);
			const FV3 HeadC = NeckTop + FV3(0.f, HeadR * 1.05f, 0.03f);
			B.Ellipsoid(HeadC, FV3(HeadR * 0.92f, HeadR * 1.12f, HeadR), H.SkinSlot);
			if (H.Jaw > 0.f)
			{
				B.Ellipsoid(HeadC + FV3(0.f, -HeadR * 0.55f, HeadR * 0.55f), FV3(HeadR * 0.6f, HeadR * 0.42f, HeadR * (0.45f + 0.3f * H.Jaw)), H.SkinSlot);
			}
			if (H.BrowRidge > 0.f)
			{
				B.Ellipsoid(HeadC + FV3(0.f, HeadR * 0.28f, HeadR * 0.78f), FV3(HeadR * 0.85f, HeadR * 0.16f * (0.6f + H.BrowRidge), HeadR * 0.3f), SlotDark);
			}
			Out.Head = HeadC;
			Out.HeadRadius = HeadR;

			// Arms
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				const FV3 Shoulder = Upper(FV3(0.3f * S, 0.5f, 0.f));
				const FV3 Elbow = Shoulder + FV3(0.06f * S, -0.33f * H.ArmScale, 0.03f);
				const FV3 Wrist = Elbow + FV3(0.02f * S, -0.3f * H.ArmScale, 0.06f);
				const bool bCyber = H.bCyberArm && Side < 0;
				if (bCyber)
				{
					B.Box(Shoulder + FV3(0.02f * S, 0.03f, 0.f), FV3(0.1f, 0.07f, 0.11f), SlotDark);
					B.Box((Shoulder + Elbow) * 0.5f, FV3(0.055f, 0.15f * H.ArmScale, 0.06f), SlotDark);
					B.Sphere(Elbow, 0.055f, SlotAccent);
					B.Box((Elbow + Wrist) * 0.5f, FV3(0.05f, 0.14f * H.ArmScale, 0.055f), SlotDark, FRot(-0.2f, 0.f, 0.f));
					B.Sphere(Wrist, 0.045f, SlotAccent);
				}
				else
				{
					B.Tube({ Shoulder, Elbow, Wrist }, { 0.065f, 0.05f, 0.04f }, H.SkinSlot);
				}
				const FV3 Hand = Wrist + FV3(0.f, -0.06f, 0.01f);
				B.Ellipsoid(Hand, FV3(0.038f, 0.065f, 0.032f), bCyber ? static_cast<uint8_t>(SlotDark) : H.SkinSlot);
				if (Side < 0) { Out.HandL = Hand; Out.ShoulderL = Shoulder; } else { Out.HandR = Hand; Out.ShoulderR = Shoulder; }
			}
			return Out;
		}

		// -------------------------------------------------------------------
		// Stage models
		// -------------------------------------------------------------------

		void BuildPrimordialCell(FBuilder& B)
		{
			const FV3 R(0.62f, 0.56f, 0.6f);
			B.Ellipsoid(FV3(0.f, 0.f, 0.f), R, SlotMid, 0.f, FRot(), 18, 12);
			B.Ellipsoid(FV3(0.12f, 0.1f, 0.38f), FV3(0.26f, 0.24f, 0.24f), SlotSecondary);
			B.Sphere(FV3(0.17f, 0.15f, 0.58f), 0.08f, SlotAccent);
			B.Sphere(FV3(-0.38f, -0.2f, 0.4f), 0.07f, SlotPrimary);
			B.Sphere(FV3(-0.1f, -0.38f, 0.42f), 0.06f, SlotPrimary);
			B.Sphere(FV3(0.45f, -0.22f, 0.32f), 0.065f, SlotPrimary);
			B.Sphere(FV3(-0.45f, 0.25f, -0.28f), 0.07f, SlotPrimary);
			AddCilia(B, FV3(0.f, 0.f, 0.f), R, 26, 11, SlotPrimary);
		}

		void BuildReplicatingCell(FBuilder& B)
		{
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				const FV3 C(0.42f * S, 0.f, 0.f);
				const FV3 R(0.5f, 0.47f, 0.47f);
				B.Ellipsoid(C, R, SlotMid, 0.f, FRot(), 16, 10);
				B.Ellipsoid(C + FV3(0.1f * S, 0.08f, 0.3f), FV3(0.19f, 0.18f, 0.18f), SlotSecondary);
				B.Sphere(C + FV3(0.12f * S, 0.1f, 0.45f), 0.06f, SlotAccent);
				AddCilia(B, C, R, 12, 20 + Side, SlotPrimary);
			}
			// Cleavage furrow where the cell pinches in two.
			B.Torus(FV3(0.f, 0.f, 0.f), 0.27f, 0.09f, SlotPrimary, FRot(0.f, 0.f, kPi * 0.5f), 24, 8);
		}

		void BuildMulticellular(FBuilder& B)
		{
			const float T = (1.f + std::sqrt(5.f)) * 0.5f;
			const FV3 Ico[12] = { {-1,T,0},{1,T,0},{-1,-T,0},{1,-T,0},{0,-1,T},{0,1,T},{0,-1,-T},{0,1,-T},{T,0,-1},{T,0,1},{-T,0,-1},{-T,0,1} };
			B.Sphere(FV3(0.f, 0.f, 0.f), 0.32f, SlotSecondary, 0.f, 14, 9);
			for (int32_t I = 0; I < 12; ++I)
			{
				const FV3 Dir = Normalize(Ico[I]);
				const FV3 C = Dir * 0.5f;
				B.Sphere(C, 0.27f, (I % 3 == 0) ? SlotPrimary : SlotMid, 0.f, 12, 8);
				B.Sphere(C + Dir * 0.22f, 0.07f, SlotAccent, 0.f, 8, 5);
			}
		}

		void BuildJellyfish(FBuilder& B)
		{
			// Bell as a revolved profile.
			B.Tube({ {0.f, 0.8f, 0.f}, {0.f, 0.74f, 0.f}, {0.f, 0.62f, 0.f}, {0.f, 0.47f, 0.f}, {0.f, 0.32f, 0.f}, {0.f, 0.2f, 0.f} },
				{ 0.02f, 0.26f, 0.46f, 0.57f, 0.62f, 0.6f }, SlotPrimary, {}, 18, false, true);
			B.Torus(FV3(0.f, 0.19f, 0.f), 0.6f, 0.03f, SlotAccent, FRot(), 32, 5);
			// Glowing spots on the bell.
			for (int32_t I = 0; I < 8; ++I)
			{
				const float A = kTwoPi * static_cast<float>(I) / 8.f;
				B.Sphere(FV3(std::cos(A) * 0.5f, 0.42f, std::sin(A) * 0.5f), 0.05f, SlotAccent, 0.f, 6, 4);
			}
			// Central mouth arms.
			for (int32_t I = 0; I < 4; ++I)
			{
				const float A = kTwoPi * static_cast<float>(I) / 4.f + 0.4f;
				const FV3 D(std::cos(A), 0.f, std::sin(A));
				B.Tube({ FV3(0.f, 0.2f, 0.f) + D * 0.06f, FV3(0.f, -0.1f, 0.f) + D * 0.12f, FV3(0.f, -0.4f, 0.f) + D * 0.08f, FV3(0.f, -0.62f, 0.f) + D * 0.14f },
					{ 0.07f, 0.06f, 0.045f, 0.02f }, SlotSecondary, { 0.f, 0.4f, 0.8f, 1.f }, 6);
			}
			// Trailing tentacles.
			for (int32_t I = 0; I < 12; ++I)
			{
				const float A = kTwoPi * static_cast<float>(I) / 12.f;
				const FV3 D(std::cos(A), 0.f, std::sin(A));
				std::vector<FV3> Pts;
				std::vector<float> Rad, Fl;
				for (int32_t K = 0; K < 6; ++K)
				{
					const float F = static_cast<float>(K) / 5.f;
					Pts.push_back(D * (0.57f - 0.12f * F) + FV3(0.f, 0.18f - 1.05f * F, 0.f));
					Rad.push_back(0.022f * (1.f - 0.7f * F));
					Fl.push_back(F);
				}
				B.Tube(Pts, Rad, SlotMid, Fl, 4, false, false);
			}
		}

		void BuildFish(FBuilder& B)
		{
			B.Tube({ {0.86f, 0.f, 0.f}, {0.77f, 0.01f, 0.f}, {0.56f, 0.02f, 0.f}, {0.25f, 0.02f, 0.f}, {-0.1f, 0.01f, 0.f}, {-0.4f, 0.f, 0.f}, {-0.62f, 0.f, 0.f}, {-0.74f, 0.f, 0.f} },
				{ 0.02f, 0.15f, 0.24f, 0.29f, 0.26f, 0.18f, 0.09f, 0.05f }, SlotPrimary,
				{ 0.f, 0.f, 0.f, 0.05f, 0.15f, 0.35f, 0.7f, 0.9f }, 12);
			// Belly stripe
			B.Ellipsoid(FV3(0.15f, -0.13f, 0.f), FV3(0.5f, 0.12f, 0.2f), SlotMid, 0.08f);
			// Tail fin (forked)
			B.Blade({ {-0.7f, 0.02f, 0.f}, {-1.02f, 0.38f, 0.f}, {-0.9f, 0.02f, 0.f} }, SlotSecondary, { 0.9f, 1.f, 1.f });
			B.Blade({ {-0.7f, -0.02f, 0.f}, {-0.9f, -0.02f, 0.f}, {-1.f, -0.3f, 0.f} }, SlotSecondary, { 0.9f, 1.f, 1.f });
			// Dorsal and anal fins
			B.Blade({ {0.22f, 0.27f, 0.f}, {0.02f, 0.56f, 0.f}, {-0.26f, 0.21f, 0.f} }, SlotSecondary, { 0.05f, 0.15f, 0.25f });
			B.Blade({ {-0.3f, -0.14f, 0.f}, {-0.46f, -0.33f, 0.f}, {-0.53f, -0.1f, 0.f} }, SlotSecondary, { 0.3f, 0.5f, 0.5f });
			// Pectoral fins
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				B.Blade({ {0.38f, -0.12f, 0.2f * S}, {0.12f, -0.32f, 0.44f * S}, {0.16f, -0.1f, 0.25f * S} }, SlotSecondary, { 0.f, 0.3f, 0.1f });
				B.Sphere(FV3(0.63f, 0.07f, 0.16f * S), 0.055f, SlotAccent, 0.f, 8, 6);
			}
		}

		void BuildAmphibian(FBuilder& B)
		{
			B.Ellipsoid(FV3(0.74f, 0.f, 0.f), FV3(0.29f, 0.12f, 0.22f), SlotPrimary);
			B.Tube({ {0.55f, 0.f, 0.f}, {0.2f, 0.02f, 0.f}, {-0.2f, 0.02f, 0.f}, {-0.5f, 0.f, 0.f}, {-0.8f, 0.f, 0.f}, {-1.06f, 0.01f, 0.f} },
				{ 0.16f, 0.2f, 0.19f, 0.13f, 0.07f, 0.015f }, SlotPrimary, { 0.f, 0.f, 0.05f, 0.25f, 0.6f, 1.f }, 10);
			B.Ellipsoid(FV3(0.05f, -0.09f, 0.f), FV3(0.45f, 0.12f, 0.16f), SlotMid, 0.05f);
			B.Blade({ {-0.42f, 0.11f, 0.f}, {-0.72f, 0.24f, 0.f}, {-1.05f, 0.03f, 0.f} }, SlotSecondary, { 0.25f, 0.6f, 1.f });
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				for (float X : { 0.36f, -0.34f })
				{
					B.Tube({ {X, -0.02f, 0.15f * S}, {X + 0.05f, -0.12f, 0.36f * S}, {X + 0.09f, -0.33f, 0.38f * S} }, { 0.06f, 0.05f, 0.04f }, SlotMid);
					B.Ellipsoid(FV3(X + 0.13f, -0.35f, 0.4f * S), FV3(0.08f, 0.025f, 0.065f), SlotDark);
				}
				B.Sphere(FV3(0.84f, 0.1f, 0.1f * S), 0.05f, SlotAccent, 0.f, 8, 6);
			}
		}

		void BuildReptile(FBuilder& B)
		{
			B.Ellipsoid(FV3(0.96f, 0.22f, 0.f), FV3(0.2f, 0.1f, 0.11f), SlotPrimary);
			B.Ellipsoid(FV3(0.99f, 0.15f, 0.f), FV3(0.17f, 0.05f, 0.09f), SlotMid);
			B.Tube({ {0.8f, 0.2f, 0.f}, {0.6f, 0.13f, 0.f}, {0.3f, 0.1f, 0.f}, {-0.05f, 0.08f, 0.f}, {-0.35f, 0.05f, 0.f} },
				{ 0.08f, 0.13f, 0.21f, 0.2f, 0.14f }, SlotPrimary, { 0.f, 0.f, 0.f, 0.05f, 0.1f }, 10);
			B.Tube({ {-0.35f, 0.05f, 0.f}, {-0.65f, 0.f, 0.f}, {-0.95f, -0.02f, 0.f}, {-1.22f, 0.03f, 0.f} },
				{ 0.13f, 0.08f, 0.045f, 0.01f }, SlotPrimary, { 0.1f, 0.4f, 0.8f, 1.f }, 8);
			B.Ellipsoid(FV3(0.15f, -0.04f, 0.f), FV3(0.42f, 0.12f, 0.17f), SlotMid, 0.03f);
			// Back spikes
			for (int32_t I = 0; I < 9; ++I)
			{
				const float X = 0.6f - 0.15f * static_cast<float>(I);
				const float BodyTop = (X > -0.35f) ? 0.1f + 0.2f * std::exp(-X * X * 3.f) : 0.05f + 0.13f * std::max(0.f, 1.f + (X + 0.35f) * 1.4f);
				const float Flex = X < -0.35f ? std::min(1.f, -(X + 0.35f) * 1.2f) : 0.f;
				B.Cone(FV3(X, BodyTop - 0.02f, 0.f), FV3(X - 0.05f, BodyTop + 0.11f, 0.f), 0.04f, SlotSecondary, Flex, 5);
			}
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				for (float X : { 0.42f, -0.22f })
				{
					B.Tube({ {X, 0.05f, 0.14f * S}, {X + 0.02f, -0.02f, 0.34f * S}, {X + 0.08f, -0.26f, 0.37f * S} }, { 0.06f, 0.05f, 0.035f }, SlotMid);
					B.Ellipsoid(FV3(X + 0.12f, -0.28f, 0.39f * S), FV3(0.08f, 0.022f, 0.06f), SlotDark);
				}
				B.Sphere(FV3(1.04f, 0.27f, 0.08f * S), 0.032f, SlotAccent, 0.f, 8, 6);
			}
		}

		void BuildMammal(FBuilder& B)
		{
			B.Ellipsoid(FV3(0.f, 0.f, 0.f), FV3(0.5f, 0.32f, 0.3f), SlotPrimary, 0.f, FRot(), 16, 10);
			B.Ellipsoid(FV3(0.05f, -0.1f, 0.f), FV3(0.42f, 0.22f, 0.25f), SlotMid);
			B.Ellipsoid(FV3(0.5f, 0.12f, 0.f), FV3(0.24f, 0.2f, 0.2f), SlotPrimary);
			B.Tube({ {0.66f, 0.1f, 0.f}, {0.9f, 0.05f, 0.f} }, { 0.11f, 0.025f }, SlotPrimary, {}, 10, false, true);
			B.Sphere(FV3(0.91f, 0.05f, 0.f), 0.035f, SlotDark, 0.f, 6, 4);
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				B.Ellipsoid(FV3(0.44f, 0.34f, 0.12f * S), FV3(0.03f, 0.1f, 0.08f), SlotSecondary, 0.f, FRot(0.3f * S, 0.f, 0.1f));
				B.Sphere(FV3(0.66f, 0.18f, 0.11f * S), 0.04f, SlotAccent, 0.f, 8, 6);
				for (float X : { 0.3f, -0.28f })
				{
					B.Tube({ {X, -0.18f, 0.15f * S}, {X + 0.03f, -0.4f, 0.16f * S} }, { 0.065f, 0.045f }, SlotMid);
					B.Ellipsoid(FV3(X + 0.06f, -0.42f, 0.16f * S), FV3(0.065f, 0.025f, 0.05f), SlotDark);
				}
			}
			B.Tube({ {-0.45f, 0.f, 0.f}, {-0.7f, -0.05f, 0.f}, {-0.95f, 0.f, 0.f}, {-1.15f, 0.13f, 0.f} },
				{ 0.05f, 0.035f, 0.025f, 0.01f }, SlotMid, { 0.2f, 0.5f, 0.8f, 1.f }, 6);
		}

		void BuildPrimate(FBuilder& B)
		{
			// Branch
			B.Tube({ {-1.f, -0.62f, 0.f}, {0.f, -0.56f, 0.f}, {1.f, -0.5f, 0.f} }, { 0.07f, 0.065f, 0.05f }, SlotDark, {}, 8);
			B.Tube({ {0.55f, -0.53f, 0.f}, {0.8f, -0.35f, -0.1f}, {0.95f, -0.25f, -0.12f} }, { 0.035f, 0.025f, 0.01f }, SlotDark, {}, 5);
			// Body
			B.Ellipsoid(FV3(0.f, -0.08f, 0.f), FV3(0.22f, 0.3f, 0.2f), SlotMid, 0.f, FRot(0.f, 0.f, -0.15f));
			B.Ellipsoid(FV3(0.04f, -0.1f, 0.08f), FV3(0.15f, 0.22f, 0.14f), SlotSecondary, 0.f, FRot(0.f, 0.f, -0.15f));
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				B.Tube({ {0.f, -0.3f, 0.12f * S}, {0.2f, -0.36f, 0.16f * S}, {0.14f, -0.5f, 0.14f * S} }, { 0.08f, 0.06f, 0.05f }, SlotMid);
				B.Ellipsoid(FV3(0.18f, -0.52f, 0.14f * S), FV3(0.07f, 0.03f, 0.05f), SlotDark);
				B.Tube({ {0.02f, 0.14f, 0.19f * S}, {0.18f, -0.08f, 0.23f * S}, {0.3f, -0.42f, 0.16f * S} }, { 0.055f, 0.045f, 0.035f }, SlotMid);
				B.Ellipsoid(FV3(0.31f, -0.48f, 0.15f * S), FV3(0.04f, 0.05f, 0.04f), SlotDark);
				B.Ellipsoid(FV3(0.f, 0.54f, 0.12f * S), FV3(0.03f, 0.07f, 0.05f), SlotSecondary, 0.f, FRot(0.4f * S, 0.f, 0.f));
				B.Sphere(FV3(0.18f, 0.42f, 0.07f * S), 0.05f, SlotAccent, 0.f, 8, 6);
			}
			// Head
			B.Ellipsoid(FV3(0.05f, 0.38f, 0.f), FV3(0.17f, 0.16f, 0.16f), SlotMid);
			B.Ellipsoid(FV3(0.2f, 0.32f, 0.f), FV3(0.1f, 0.07f, 0.08f), SlotSecondary);
			// Ringed tail, curling down behind the branch.
			const std::vector<FV3> Tail = { {-0.15f, -0.3f, 0.f}, {-0.38f, -0.44f, 0.f}, {-0.56f, -0.66f, 0.f}, {-0.62f, -0.88f, 0.f},
				{-0.55f, -1.02f, 0.f}, {-0.42f, -1.04f, 0.f}, {-0.35f, -0.95f, 0.f}, {-0.38f, -0.86f, 0.f} };
			for (size_t I = 0; I + 1 < Tail.size(); ++I)
			{
				const float F0 = static_cast<float>(I) / static_cast<float>(Tail.size() - 1);
				const float F1 = static_cast<float>(I + 1) / static_cast<float>(Tail.size() - 1);
				B.Tube({ Tail[I], Tail[I + 1] }, { 0.07f - 0.04f * F0, 0.07f - 0.04f * F1 }, (I % 2) ? SlotDark : SlotSecondary,
					{ 0.2f + 0.6f * F0, 0.2f + 0.6f * F1 }, 7, I == 0, true);
			}
		}

		void BuildHominin(FBuilder& B)
		{
			FHumanoid H;
			H.Hunch = 0.42f;
			H.ArmScale = 1.22f;
			H.HeadScale = 0.95f;
			H.BrowRidge = 1.f;
			H.Jaw = 1.f;
			H.KneeBend = 1.f;
			H.BodySlot = SlotMid;
			H.SkinSlot = SlotMid;
			AddHumanoid(B, H);
		}

		void BuildErectus(FBuilder& B)
		{
			FHumanoid H;
			H.Hunch = 0.1f;
			H.ArmScale = 1.08f;
			H.BrowRidge = 0.6f;
			H.Jaw = 0.4f;
			H.KneeBend = 0.3f;
			H.BodySlot = SlotDark;
			H.SkinSlot = SlotMid;
			const FHumanoidAnchors A = AddHumanoid(B, H);
			// Torch held out in the right hand.
			const FV3 Hand = A.HandR;
			B.Tube({ Hand + FV3(0.f, -0.15f, -0.05f), Hand + FV3(0.04f, 0.32f, 0.12f) }, { 0.025f, 0.03f }, SlotDark, {}, 6);
			const FV3 Top = Hand + FV3(0.045f, 0.36f, 0.13f);
			B.Gem(Top + FV3(0.f, 0.08f, 0.f), FV3(0.f, 1.f, 0.f), 0.2f, 0.07f, SlotAccent, 0.3f);
			B.Gem(Top + FV3(0.04f, 0.05f, 0.02f), FV3(0.4f, 1.f, 0.2f), 0.13f, 0.045f, SlotSecondary, 0.5f);
			B.Gem(Top + FV3(-0.04f, 0.04f, -0.02f), FV3(-0.4f, 1.f, -0.1f), 0.12f, 0.04f, SlotSecondary, 0.5f);
		}

		void BuildSapiens(FBuilder& B)
		{
			FHumanoid H;
			H.HeadScale = 1.06f;
			H.BodySlot = SlotPrimary;
			H.SkinSlot = SlotMid;
			const FHumanoidAnchors A = AddHumanoid(B, H);
			// A small glowing spark of an idea above the open hand.
			B.Icosphere(A.HandR + FV3(0.05f, 0.18f, 0.12f), 0.055f, SlotAccent, 1);
		}

		void BuildEnhanced(FBuilder& B)
		{
			FHumanoid H;
			H.HeadScale = 1.08f;
			H.BodySlot = SlotSecondary;
			H.SkinSlot = SlotMid;
			const FHumanoidAnchors A = AddHumanoid(B, H);
			// Glowing gene-edit bands and a halo.
			B.Torus(FV3(0.f, 0.33f, 0.f), 0.29f, 0.018f, SlotAccent, FRot(0.15f, 0.f, 0.f), 28, 5);
			B.Torus(FV3(0.f, 0.08f, 0.f), 0.22f, 0.016f, SlotAccent, FRot(-0.1f, 0.f, 0.f), 24, 5);
			B.Torus(A.Head + FV3(0.f, A.HeadRadius * 1.6f, 0.f), 0.17f, 0.018f, SlotAccent, FRot(0.25f, 0.f, 0.f), 24, 5);
			for (const FV3& K : { A.KneeL, A.KneeR })
			{
				B.Torus(K, 0.09f, 0.014f, SlotAccent, FRot(), 16, 4);
			}
		}

		void BuildCyborg(FBuilder& B)
		{
			FHumanoid H;
			H.HeadScale = 1.05f;
			H.BodySlot = SlotPrimary;
			H.SkinSlot = SlotMid;
			H.bCyberArm = true;
			H.bCyberLeg = true;
			const FHumanoidAnchors A = AddHumanoid(B, H);
			B.Box(A.Head + FV3(0.f, A.HeadRadius * 0.2f, A.HeadRadius * 0.82f), FV3(A.HeadRadius * 0.85f, A.HeadRadius * 0.17f, A.HeadRadius * 0.22f), SlotAccent);
			B.Box(A.Chest + FV3(-0.06f, 0.f, -0.02f), FV3(0.13f, 0.12f, 0.05f), SlotDark);
			B.Sphere(A.Chest + FV3(-0.06f, 0.f, 0.04f), 0.045f, SlotAccent, 0.f, 8, 6);
			B.Box(A.Head + FV3(-A.HeadRadius * 0.9f, 0.f, 0.f), FV3(0.03f, 0.06f, 0.06f), SlotDark);
		}

		void BuildDigital(FBuilder& B)
		{
			// Voxel bust: only exposed faces of the occupied grid are emitted.
			const float V = 0.12f;
			auto Occupied = [](int X, int Y, int Z) -> bool
			{
				const float PX = static_cast<float>(X) * 0.12f, PY = static_cast<float>(Y) * 0.12f, PZ = static_cast<float>(Z) * 0.12f;
				const float Head = (PX * PX) / (0.5f * 0.5f) + ((PY - 0.25f) * (PY - 0.25f)) / (0.62f * 0.62f) + ((PZ - 0.03f) * (PZ - 0.03f)) / (0.55f * 0.55f);
				const float Neck = (PX * PX + PZ * PZ) / (0.24f * 0.24f);
				const float Shoulders = (PX * PX) / (0.78f * 0.78f) + ((PY + 0.78f) * (PY + 0.78f)) / (0.26f * 0.26f) + (PZ * PZ) / (0.32f * 0.32f);
				return Head <= 1.f || (PY > -0.62f && PY < -0.2f && Neck <= 1.f) || (Shoulders <= 1.f && PY <= -0.6f);
			};
			std::set<std::tuple<int, int, int>> Cells;
			for (int X = -9; X <= 9; ++X)
				for (int Y = -9; Y <= 9; ++Y)
					for (int Z = -6; Z <= 6; ++Z)
						if (Occupied(X, Y, Z)) { Cells.insert({ X, Y, Z }); }

			auto SlotFor = [](int X, int Y, int Z) -> uint8_t
			{
				// Eyes and a few random "data" cells glow.
				if (Z >= 4 && Y == 3 && (X == -2 || X == 2)) { return SlotAccent; }
				const uint32_t H = static_cast<uint32_t>((X * 73856093) ^ (Y * 19349663) ^ (Z * 83492791));
				if (H % 17 == 0) { return SlotAccent; }
				return (H % 3 == 0) ? SlotSecondary : SlotPrimary;
			};

			static const int Dirs[6][3] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
			for (const auto& Cell : Cells)
			{
				const int X = std::get<0>(Cell), Y = std::get<1>(Cell), Z = std::get<2>(Cell);
				const FV3 C(static_cast<float>(X) * V, static_cast<float>(Y) * V, static_cast<float>(Z) * V);
				const uint8_t Slot = SlotFor(X, Y, Z);
				const float H = V * 0.47f; // small gaps between voxels
				for (const auto& D : Dirs)
				{
					if (Cells.count({ X + D[0], Y + D[1], Z + D[2] }))
					{
						continue;
					}
					const FV3 N(static_cast<float>(D[0]), static_cast<float>(D[1]), static_cast<float>(D[2]));
					const FV3 U = std::fabs(N.Y) > 0.5f ? FV3(1.f, 0.f, 0.f) : FV3(0.f, 1.f, 0.f);
					const FV3 W = Cross(N, U);
					const FV3 FC = C + N * H;
					const uint32_t A = B.AddVertex(FC + (U + W) * H, 0.f);
					const uint32_t Bv = B.AddVertex(FC + (U - W) * H, 0.f);
					const uint32_t Cv = B.AddVertex(FC + (-U - W) * H, 0.f);
					const uint32_t Dv = B.AddVertex(FC + (-U + W) * H, 0.f);
					B.AddTri(A, Bv, Cv, Slot, C);
					B.AddTri(A, Cv, Dv, Slot, C);
				}
			}
			// A few loose voxels dissolving away from the shoulders.
			FRng Rng(1515);
			for (int32_t I = 0; I < 14; ++I)
			{
				const FV3 P(Rng.Range(-1.f, 1.f), Rng.Range(-1.f, -0.75f), Rng.Range(-0.4f, 0.4f));
				const float S = Rng.Range(0.02f, 0.045f);
				B.Box(P + FV3(0.f, -0.12f, 0.f), FV3(S, S, S), (I % 3 == 0) ? SlotAccent : SlotPrimary, FRot(), 0.6f);
			}
		}

		void BuildDisembodied(FBuilder& B)
		{
			// Brain hemispheres with folds (gyri).
			for (int32_t Side = -1; Side <= 1; Side += 2)
			{
				const float S = static_cast<float>(Side);
				const FV3 C(0.f, 0.08f, 0.2f * S);
				const FV3 R(0.6f, 0.4f, 0.22f);
				B.Ellipsoid(C, R, SlotPrimary, 0.f, FRot(), 16, 10);
				FRng Rng(70 + Side);
				for (int32_t I = 0; I < 26; ++I)
				{
					const float Lon = Rng.Range(0.f, kTwoPi);
					const float Lat = Rng.Range(-0.35f, 1.2f);
					FV3 Dir(std::cos(Lat) * std::cos(Lon), std::sin(Lat), std::cos(Lat) * std::sin(Lon));
					if (Dir.Z * S < -0.2f) { Dir.Z = -Dir.Z; } // keep folds off the inner face
					const FV3 P = C + Mul(Dir, R) * 0.96f;
					B.Ellipsoid(P, FV3(0.13f, 0.055f, 0.07f), (I % 4 == 0) ? SlotMid : SlotPrimary, 0.f, FRot(0.f, Lon, Lat * 0.5f), 7, 4);
				}
			}
			B.Ellipsoid(FV3(-0.42f, -0.2f, 0.f), FV3(0.2f, 0.13f, 0.3f), SlotMid);
			B.Tube({ {-0.18f, -0.18f, 0.f}, {-0.22f, -0.45f, 0.f}, {-0.2f, -0.72f, 0.f}, {-0.14f, -0.95f, 0.f} },
				{ 0.1f, 0.08f, 0.05f, 0.01f }, SlotMid, { 0.f, 0.2f, 0.6f, 1.f }, 8);
			// Orbiting neural ring with nodes.
			const FRot Tilt(0.45f, 0.f, 0.2f);
			B.Torus(FV3(0.f, 0.05f, 0.f), 0.88f, 0.015f, SlotAccent, Tilt, 40, 4);
			for (int32_t I = 0; I < 7; ++I)
			{
				const float A = kTwoPi * static_cast<float>(I) / 7.f;
				B.Icosphere(FV3(0.f, 0.05f, 0.f) + Tilt.Apply(FV3(std::cos(A), 0.f, std::sin(A)) * 0.88f), 0.05f, SlotAccent, 0);
			}
		}

		void BuildEnergyForm(FBuilder& B)
		{
			B.Icosphere(FV3(0.f, 0.f, 0.f), 0.3f, SlotAccent, 2);
			B.Torus(FV3(0.f, 0.f, 0.f), 0.62f, 0.035f, SlotPrimary, FRot(0.6f, 0.f, 0.3f), 36, 6);
			B.Torus(FV3(0.f, 0.f, 0.f), 0.72f, 0.03f, SlotSecondary, FRot(-0.7f, 0.f, -0.5f), 36, 6);
			const float T = (1.f + std::sqrt(5.f)) * 0.5f;
			const FV3 Dirs[6] = { {1,T,0},{-1,T,0},{0,1,T},{0,-1,T},{T,0,1},{-T,0,1} };
			for (int32_t I = 0; I < 6; ++I)
			{
				for (float Sign : { 1.f, -1.f })
				{
					const FV3 D = Normalize(Dirs[I]) * Sign;
					B.Gem(D * 0.42f, D, 0.42f, 0.07f, (I % 2) ? SlotPrimary : SlotSecondary, 0.15f);
				}
			}
		}

		void BuildStellar(FBuilder& B)
		{
			B.Icosphere(FV3(0.f, 0.f, 0.f), 0.42f, SlotAccent, 2);
			FRng Rng(1818);
			for (int32_t I = 0; I < 22; ++I)
			{
				const float Fi = (static_cast<float>(I) + 0.5f) / 22.f;
				const float Y = 1.f - 2.f * Fi;
				const float R = std::sqrt(std::max(0.f, 1.f - Y * Y));
				const float Ang = static_cast<float>(I) * 2.39996f;
				const FV3 D(R * std::cos(Ang), Y, R * std::sin(Ang));
				B.Cone(D * 0.38f, D * (0.55f + Rng.Range(0.f, 0.12f)), 0.08f, SlotSecondary, 0.2f, 5);
			}
			const FRot OrbitA(0.25f, 0.f, 0.1f), OrbitB(-0.2f, 0.f, -0.25f);
			B.Torus(FV3(0.f, 0.f, 0.f), 0.78f, 0.012f, SlotMid, OrbitA, 44, 4);
			B.Torus(FV3(0.f, 0.f, 0.f), 0.96f, 0.012f, SlotMid, OrbitB, 50, 4);
			B.Sphere(OrbitA.Apply(FV3(0.78f, 0.f, 0.f)), 0.07f, SlotPrimary, 0.f, 10, 7);
			B.Sphere(OrbitB.Apply(FV3(-0.4f, 0.f, 0.86f)), 0.09f, SlotSecondary, 0.f, 10, 7);
			B.Torus(OrbitB.Apply(FV3(-0.4f, 0.f, 0.86f)), 0.14f, 0.012f, SlotMid, FRot(0.4f, 0.f, 0.f), 18, 3);
		}

		void BuildCosmic(FBuilder& B)
		{
			const FRot Tilt(0.75f, 0.f, 0.15f);
			B.Ellipsoid(FV3(0.f, 0.f, 0.f), FV3(0.26f, 0.13f, 0.26f), SlotAccent, 0.f, Tilt, 14, 9);
			FRng Rng(1919);
			for (int32_t Arm = 0; Arm < 2; ++Arm)
			{
				for (int32_t K = 0; K < 70; ++K)
				{
					const float F = static_cast<float>(K) / 69.f;
					const float Theta = F * 4.2f + static_cast<float>(Arm) * kPi;
					const float Radius = 0.16f + F * 0.8f;
					const FV3 Local(Radius * std::cos(Theta) + Rng.Range(-0.05f, 0.05f), Rng.Range(-0.03f, 0.03f), Radius * std::sin(Theta) + Rng.Range(-0.05f, 0.05f));
					const FV3 P = Tilt.Apply(Local);
					const float Size = 0.055f * (1.f - 0.6f * F) + Rng.Range(0.f, 0.015f);
					const uint8_t Slot = (K % 7 == 0) ? SlotAccent : ((K + Arm) % 2 ? SlotPrimary : SlotSecondary);
					const FV3 Axis = Normalize(FV3(Rng.Range(-1.f, 1.f), Rng.Range(-1.f, 1.f), Rng.Range(-1.f, 1.f)));
					B.Gem(P, Axis, Size, Size * 0.5f, Slot, 0.f);
				}
			}
		}

		void BuildPureEnergy(FBuilder& B)
		{
			B.Icosphere(FV3(0.f, 0.f, 0.f), 0.5f, SlotAccent, 2);
			B.Torus(FV3(0.f, 0.f, 0.f), 0.72f, 0.025f, SlotPrimary, FRot(1.2f, 0.f, 0.2f), 44, 5);
			B.Torus(FV3(0.f, 0.f, 0.f), 0.84f, 0.022f, SlotSecondary, FRot(-0.5f, 0.f, 0.9f), 48, 5);
			B.Torus(FV3(0.f, 0.f, 0.f), 0.96f, 0.02f, SlotMid, FRot(0.3f, 0.f, -0.6f), 52, 5);
		}
	}

	void AppendGem(FMesh& Mesh, const FV3& Center, const FV3& Axis, float Length, float Width, uint8_t Slot)
	{
		FBuilder B;
		B.Mesh = std::move(Mesh);
		B.Gem(Center, Axis, Length, Width, Slot, 0.f);
		Mesh = std::move(B.Mesh);
	}

	FMesh BuildStageMesh(int32_t StageId)
	{
		FBuilder B;
		switch (StageId)
		{
		case 1: BuildPrimordialCell(B); break;
		case 2: BuildReplicatingCell(B); break;
		case 3: BuildMulticellular(B); break;
		case 4: BuildJellyfish(B); break;
		case 5: BuildFish(B); break;
		case 6: BuildAmphibian(B); break;
		case 7: BuildReptile(B); break;
		case 8: BuildMammal(B); break;
		case 9: BuildPrimate(B); break;
		case 10: BuildHominin(B); break;
		case 11: BuildErectus(B); break;
		case 12: BuildSapiens(B); break;
		case 13: BuildEnhanced(B); break;
		case 14: BuildCyborg(B); break;
		case 15: BuildDigital(B); break;
		case 16: BuildDisembodied(B); break;
		case 17: BuildEnergyForm(B); break;
		case 18: BuildStellar(B); break;
		case 19: BuildCosmic(B); break;
		case 20: BuildPureEnergy(B); break;
		default: B.Icosphere(FV3(0.f, 0.f, 0.f), 1.f, SlotPrimary, 1); break;
		}
		B.Normalize01();
		return B.Mesh;
	}
}
