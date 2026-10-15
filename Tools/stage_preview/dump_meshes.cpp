// Dumps every stage mesh as JSON so Tools/stage_preview/render.py can draw previews without Unreal.
// Build: g++ -std=c++17 -O2 -I ../../Source/EvolutionIdle/Private dump_meshes.cpp ../../Source/EvolutionIdle/Private/UI/EvoStageMeshes.cpp -o dump_meshes
#include "UI/EvoStageMeshes.h"
#include <cstdio>

int main()
{
	std::printf("[");
	for (int Stage = 1; Stage <= 20; ++Stage)
	{
		const EvoMesh::FMesh M = EvoMesh::BuildStageMesh(Stage);
		std::printf("%s{\"stage\":%d,\"pos\":[", Stage > 1 ? "," : "", Stage);
		for (size_t I = 0; I < M.Positions.size(); ++I)
		{
			std::printf("%s[%.4f,%.4f,%.4f,%.3f]", I ? "," : "", M.Positions[I].X, M.Positions[I].Y, M.Positions[I].Z, M.Flex[I]);
		}
		std::printf("],\"tri\":[");
		for (int T = 0; T < M.NumTriangles(); ++T)
		{
			std::printf("%s[%u,%u,%u,%u,%u]", T ? "," : "", M.Indices[T * 3], M.Indices[T * 3 + 1], M.Indices[T * 3 + 2], M.TriSlot[T], M.TriTwoSided[T]);
		}
		std::printf("]}");
	}
	std::printf("]\n");
	return 0;
}
