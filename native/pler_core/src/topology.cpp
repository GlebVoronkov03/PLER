#include "pler/topology.hpp"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <utility>

namespace pler {
namespace {

using Edge = std::pair<int, int>;
Edge undirected(int a, int b) { return a < b ? Edge{a, b} : Edge{b, a}; }

}  // namespace

TopologyFeatures compute_topology(const Mesh& mesh) {
  TopologyFeatures f;
  if (mesh.empty()) return f;

  const int nv = static_cast<int>(mesh.vertices.size());
  const int nf = static_cast<int>(mesh.triangles.size());

  std::set<Edge> edges;
  std::map<Edge, int> edge_face_count;
  std::vector<std::vector<int>> face_adj(static_cast<size_t>(nf));

  std::map<Edge, std::vector<int>> edge_to_faces;
  for (int fi = 0; fi < nf; fi++) {
    const auto& t = mesh.triangles[static_cast<size_t>(fi)];
    Edge e[3] = {undirected(t[0], t[1]), undirected(t[1], t[2]), undirected(t[2], t[0])};
    for (auto& ed : e) {
      edges.insert(ed);
      edge_face_count[ed]++;
      edge_to_faces[ed].push_back(fi);
    }
  }

  for (const auto& kv : edge_to_faces) {
    const auto& faces = kv.second;
    for (size_t i = 0; i < faces.size(); i++) {
      for (size_t j = i + 1; j < faces.size(); j++) {
        face_adj[static_cast<size_t>(faces[i])].push_back(faces[j]);
        face_adj[static_cast<size_t>(faces[j])].push_back(faces[i]);
      }
    }
  }

  // connected components on faces
  std::vector<char> seen(static_cast<size_t>(nf), 0);
  int components = 0;
  for (int i = 0; i < nf; i++) {
    if (seen[static_cast<size_t>(i)]) continue;
    components++;
    std::queue<int> q;
    q.push(i);
    seen[static_cast<size_t>(i)] = 1;
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      for (int v : face_adj[static_cast<size_t>(u)]) {
        if (!seen[static_cast<size_t>(v)]) {
          seen[static_cast<size_t>(v)] = 1;
          q.push(v);
        }
      }
    }
  }

  // boundary edges: count == 1; walk boundary loops
  std::set<Edge> boundary;
  for (const auto& kv : edge_face_count) {
    if (kv.second == 1) boundary.insert(kv.first);
  }

  std::map<int, std::vector<int>> adj;
  for (const auto& e : boundary) {
    adj[e.first].push_back(e.second);
    adj[e.second].push_back(e.first);
  }

  std::set<Edge> used;
  int loops = 0;
  for (const auto& e0 : boundary) {
    if (used.count(e0)) continue;
    loops++;
    int start = e0.first;
    int prev = -1;
    int cur = e0.first;
    int guard = 0;
    while (guard++ < nv * 4) {
      const auto& nbrs = adj[cur];
      int next = -1;
      for (int n : nbrs) {
        Edge ed = undirected(cur, n);
        if (used.count(ed)) continue;
        if (n == prev) continue;
        next = n;
        used.insert(ed);
        break;
      }
      if (next < 0) {
        // pick any unused
        for (int n : nbrs) {
          Edge ed = undirected(cur, n);
          if (!used.count(ed)) {
            next = n;
            used.insert(ed);
            break;
          }
        }
      }
      if (next < 0) break;
      prev = cur;
      cur = next;
      if (cur == start) break;
    }
  }

  int ne = static_cast<int>(edges.size());
  int euler = nv - ne + nf;
  int genus = 0;
  if (euler <= 2) genus = std::max(0, (2 - euler) / 2);

  f.connected_components = std::max(1, components);
  f.boundary_loops = loops;
  f.euler = euler;
  f.genus_estimate = genus;
  return f;
}

double topological_similarity(const TopologyFeatures& a, const TopologyFeatures& b) {
  // Jaccard-style on feature bags: treat (components, loops) as sets of labels
  // |A∩B| / |A∪B| using multiset of two integers via min/max
  auto jaccard1 = [](int x, int y) {
    int mn = std::min(x, y);
    int mx = std::max(x, y);
    if (mx <= 0) return 1.0;
    return static_cast<double>(mn) / static_cast<double>(mx);
  };
  // Paper: N_ref ∩ N_test / N_ref ∪ N_test over counts of components and cycles
  int inter = std::min(a.connected_components, b.connected_components) +
              std::min(a.boundary_loops, b.boundary_loops);
  int uni = std::max(a.connected_components, b.connected_components) +
            std::max(a.boundary_loops, b.boundary_loops);
  if (uni == 0) return 1.0;
  double tsi = static_cast<double>(inter) / static_cast<double>(uni);
  // blend with genus agreement lightly
  double g = jaccard1(a.genus_estimate + 1, b.genus_estimate + 1);
  return 0.85 * tsi + 0.15 * g;
}

}  // namespace pler
