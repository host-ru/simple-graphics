#include "utils/parsers/parsers.hpp"

#include "utils/parsers/obj_parser.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace parser {
void parse_model(const std::vector<char> &data,
                 std::vector<gl::Element> &elements,
                 std::vector<gl::Vertex> &vertices, std::string &texture_path) {
  Timer timer("Parsing both OBJ and MTL files took ");

  Color color = {1.0, 1.0, 1.0};
  std::vector<Point> points;
  std::vector<parser::obj::TextureCoords> uvs;
  std::vector<parser::obj::Face> faces;
  parser::obj::parse(data, points, uvs, faces, color, texture_path);

  for (const auto &face : faces) {
    auto v = std::array<gl::Vertex, 3>();
    for (int i = 0; i != 3; ++i) {
      unsigned int vertex_id = face.vertices.at(i).vertex_id;
      unsigned int uv_id = face.vertices.at(i).uv_id;

      auto vertex = points[vertex_id];
      auto uv = uvs[uv_id];

      v.at(i).coord = {vertex.x, vertex.y, vertex.z};
      v.at(i).color = {color.r, color.g, color.b};
      v.at(i).uv = {uv.u, uv.v};
    }
    vertices.insert(vertices.end(), v.begin(), v.end());
    auto e = gl::Element();
    for (int i = 0; i != 3; ++i) {
      e.vertices.at(i) = static_cast<unsigned int>(vertices.size() - i - 1);
    }
    elements.push_back(e);
  }
  throw std::runtime_error("Exit!");
}

void parse_model_fbx(const std::vector<char> &data,
                     std::vector<gl::Element> &elements,
                     std::vector<gl::Vertex> &vertices,
                     std::string &texture_path) {
  Timer timer("Parsing FBX file took ");

  Assimp::Importer importer;
  const aiScene *scene = importer.ReadFileFromMemory(
      data.data(), data.size(), aiProcess_Triangulate, "fbx");
  // const aiScene *scene = importer.ReadFile("./resources/AK-47.fbx", 0);

  if (scene == nullptr) {
    const std::string error = importer.GetErrorString();
    throw std::runtime_error(error);
  }

  for (size_t i = 0; i < scene->mNumMeshes; ++i) {
    const aiMesh *mesh = scene->mMeshes[i];

    for (size_t j = 0; j < mesh->mNumFaces; ++j) {
      const aiFace face = mesh->mFaces[j];
      auto e = gl::Element();
      for (size_t k = 0; k < face.mNumIndices; ++k) {
        e.vertices[k] = face.mIndices[k];
      }
      elements.push_back(e);
    }

    for (size_t j = 0; j < mesh->mNumVertices; ++j) {
      const auto point = mesh->mVertices[j];
      const auto tex = mesh->mTextureCoords[0][j];
      const auto material = scene->mMaterials[mesh->mMaterialIndex];

      aiColor3D color(0.F, 0.F, 0.F);
      if (AI_SUCCESS != material->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
        throw std::runtime_error(
            "Error accesssing diffuse color of a material!");
      }
      gl::Vertex v = {{point.x, point.y, point.z},
                      {color.r, color.g, color.b},
                      {tex.x, tex.y}};

      vertices.push_back(std::move(v));
    }
  }

  std::cout << vertices.size() << '\n';
  std::cout << elements.size() << '\n';
  for (const auto &i : vertices) {
    std::cout << i.coord.x << ' ' << i.coord.y << ' ' << i.coord.z << ' '
              << i.color.r << ' ' << i.color.g << ' ' << i.color.b << ' '
              << i.uv.x << ' ' << i.uv.y << '\n';
  }

  throw std::runtime_error("Exit!");
}
} // namespace parser