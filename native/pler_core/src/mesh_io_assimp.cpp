#include "pler/mesh_io.hpp"

#if defined(PLER_HAS_ASSIMP) && PLER_HAS_ASSIMP

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace pler {

Mesh load_assimp(const std::string& path) {
  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(
      path, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                aiProcess_ImproveCacheLocality);
  if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
    throw PlerError(std::string("Assimp failed: ") + importer.GetErrorString() +
                    " (" + path + ")");
  }

  Mesh mesh;
  mesh.source_path = path;
  for (unsigned mi = 0; mi < scene->mNumMeshes; mi++) {
    const aiMesh* am = scene->mMeshes[mi];
    int base = static_cast<int>(mesh.vertices.size());
    for (unsigned i = 0; i < am->mNumVertices; i++) {
      mesh.vertices.emplace_back(am->mVertices[i].x, am->mVertices[i].y,
                                 am->mVertices[i].z);
      if (am->HasVertexColors(0)) {
        mesh.colors.emplace_back(am->mColors[0][i].r, am->mColors[0][i].g,
                                 am->mColors[0][i].b);
      }
      if (am->HasTextureCoords(0)) {
        mesh.uvs.push_back(
            {am->mTextureCoords[0][i].x, am->mTextureCoords[0][i].y});
      }
    }
    for (unsigned f = 0; f < am->mNumFaces; f++) {
      const aiFace& face = am->mFaces[f];
      if (face.mNumIndices < 3) continue;
      for (unsigned k = 1; k + 1 < face.mNumIndices; k++) {
        mesh.triangles.push_back({base + static_cast<int>(face.mIndices[0]),
                                  base + static_cast<int>(face.mIndices[k]),
                                  base + static_cast<int>(face.mIndices[k + 1])});
      }
    }
  }
  validate_mesh(mesh, path);
  return mesh;
}

}  // namespace pler

#else

namespace pler {
Mesh load_assimp(const std::string& path) {
  throw PlerError("Assimp not compiled into this build: " + path);
}
}  // namespace pler

#endif
