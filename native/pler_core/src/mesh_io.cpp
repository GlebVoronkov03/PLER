#include "pler/mesh_io.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace pler {
namespace {

Mesh load_obj(const std::string& path) {
  tinyobj::ObjReaderConfig config;
  config.triangulate = true;
  config.vertex_color = false;

  tinyobj::ObjReader reader;
  if (!reader.ParseFromFile(path, config)) {
    std::string err = reader.Error();
    if (err.empty()) err = "unknown OBJ parse error";
    throw PlerError("Failed to load mesh: " + path + " (" + err + ")");
  }
  if (!reader.Warning().empty()) {
    // warnings only
  }

  const auto& attrib = reader.GetAttrib();
  const auto& shapes = reader.GetShapes();

  Mesh mesh;
  mesh.source_path = path;
  mesh.vertices.reserve(attrib.vertices.size() / 3);
  for (size_t i = 0; i + 2 < attrib.vertices.size(); i += 3) {
    mesh.vertices.emplace_back(attrib.vertices[i], attrib.vertices[i + 1],
                               attrib.vertices[i + 2]);
  }

  for (const auto& shape : shapes) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      int fv = shape.mesh.num_face_vertices[f];
      if (fv < 3) {
        index_offset += static_cast<size_t>(fv);
        continue;
      }
      // fan triangulation for safety
      tinyobj::index_t i0 = shape.mesh.indices[index_offset];
      for (int v = 1; v + 1 < fv; v++) {
        tinyobj::index_t i1 = shape.mesh.indices[index_offset + v];
        tinyobj::index_t i2 = shape.mesh.indices[index_offset + v + 1];
        mesh.triangles.push_back(
            {i0.vertex_index, i1.vertex_index, i2.vertex_index});
      }
      index_offset += static_cast<size_t>(fv);
    }
  }

  // Optional vertex colors / UVs (chip 20; unused by score)
  if (!attrib.colors.empty() && attrib.colors.size() >= mesh.vertices.size() * 3) {
    mesh.colors.resize(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
      mesh.colors[i] = {attrib.colors[i * 3], attrib.colors[i * 3 + 1],
                        attrib.colors[i * 3 + 2]};
    }
  }
  if (!attrib.texcoords.empty()) {
    mesh.uvs.resize(mesh.vertices.size(), {0.f, 0.f});
    for (const auto& shape : shapes) {
      for (const auto& idx : shape.mesh.indices) {
        if (idx.texcoord_index >= 0 &&
            static_cast<size_t>(idx.vertex_index) < mesh.uvs.size()) {
          size_t ti = static_cast<size_t>(idx.texcoord_index) * 2;
          if (ti + 1 < attrib.texcoords.size()) {
            mesh.uvs[static_cast<size_t>(idx.vertex_index)] = {
                attrib.texcoords[ti], attrib.texcoords[ti + 1]};
          }
        }
      }
    }
  }

  validate_mesh(mesh, path);
  return mesh;
}

Mesh load_stl_ascii_or_bin(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw PlerError("Cannot open file: " + path);

  // Detect binary STL: 80-byte header + uint32 tri count
  char header[80];
  in.read(header, 80);
  if (!in) throw PlerError("Invalid STL: " + path);

  uint32_t tri_count = 0;
  in.read(reinterpret_cast<char*>(&tri_count), 4);
  if (!in) throw PlerError("Invalid STL header: " + path);

  auto pos = in.tellg();
  in.seekg(0, std::ios::end);
  auto file_size = static_cast<std::uint64_t>(in.tellg());
  in.seekg(pos);

  const std::uint64_t expected = 84ull + 50ull * tri_count;
  Mesh mesh;
  mesh.source_path = path;

  if (file_size == expected && tri_count > 0) {
    mesh.vertices.reserve(static_cast<size_t>(tri_count) * 3);
    mesh.triangles.reserve(tri_count);
    for (uint32_t t = 0; t < tri_count; t++) {
      float n[3], v[9];
      uint16_t attr = 0;
      in.read(reinterpret_cast<char*>(n), 12);
      in.read(reinterpret_cast<char*>(v), 36);
      in.read(reinterpret_cast<char*>(&attr), 2);
      if (!in) throw PlerError("Truncated binary STL: " + path);
      int base = static_cast<int>(mesh.vertices.size());
      for (int i = 0; i < 3; i++) {
        mesh.vertices.emplace_back(v[i * 3], v[i * 3 + 1], v[i * 3 + 2]);
      }
      mesh.triangles.push_back({base, base + 1, base + 2});
    }
  } else {
    // ASCII fallback
    in.clear();
    in.seekg(0);
    std::string line;
    std::vector<Vec3> face;
    while (std::getline(in, line)) {
      std::istringstream ss(line);
      std::string tag;
      ss >> tag;
      if (tag == "vertex") {
        double x, y, z;
        ss >> x >> y >> z;
        face.emplace_back(x, y, z);
        if (face.size() == 3) {
          int base = static_cast<int>(mesh.vertices.size());
          mesh.vertices.push_back(face[0]);
          mesh.vertices.push_back(face[1]);
          mesh.vertices.push_back(face[2]);
          mesh.triangles.push_back({base, base + 1, base + 2});
          face.clear();
        }
      }
    }
  }

  validate_mesh(mesh, path);
  return mesh;
}

std::string lower_ext(const std::string& path) {
  auto dot = path.find_last_of('.');
  if (dot == std::string::npos) return "";
  std::string e = path.substr(dot);
  for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return e;
}

}  // namespace

void validate_mesh(const Mesh& mesh, const std::string& path) {
  if (mesh.vertices.empty())
    throw PlerError("No vertices in mesh: " + path);
  if (mesh.triangles.empty())
    throw PlerError("No triangles in mesh: " + path);
  const int n = static_cast<int>(mesh.vertices.size());
  for (const auto& t : mesh.triangles) {
    for (int i = 0; i < 3; i++) {
      if (t[i] < 0 || t[i] >= n)
        throw PlerError("Invalid triangle index in: " + path);
    }
  }
}

Mesh load_mesh(const std::string& path) {
  std::ifstream probe(path);
  if (!probe) throw PlerError("File not found: " + path);
  probe.close();

  const std::string ext = lower_ext(path);
  if (ext == ".obj") return load_obj(path);
  if (ext == ".stl") return load_stl_ascii_or_bin(path);
#if defined(PLER_HAS_ASSIMP) && PLER_HAS_ASSIMP
  // Assimp path for glTF/PLY/FBX/etc.
  return load_assimp(path);
#else
  if (ext == ".ply" || ext == ".gltf" || ext == ".glb" || ext == ".fbx" ||
      ext == ".dae") {
    throw PlerError(
        "Format requires Assimp (rebuild with -DPLER_ASSIMP=ON): " + path);
  }
  return load_obj(path);
#endif
}

}  // namespace pler
