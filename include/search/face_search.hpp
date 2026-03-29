#ifndef SENTRIFACE_SEARCH_FACE_SEARCH_HPP_
#define SENTRIFACE_SEARCH_FACE_SEARCH_HPP_

#include <string>
#include <vector>

namespace sentriface::search {

struct SearchConfig {
  int embedding_dim = 512;
  int top_k = 5;
};

struct FacePrototype {
  int person_id = -1;
  std::string label;
  std::vector<float> embedding;
};

struct FacePrototypeV2 {
  int person_id = -1;
  int prototype_id = -1;
  int zone = 0;
  std::string label;
  std::vector<float> embedding;
  float prototype_weight = 1.0f;
};

struct SearchHit {
  int person_id = -1;
  std::string label;
  float score = 0.0f;
};

struct PrototypeHit {
  int person_id = -1;
  int prototype_id = -1;
  int zone = 0;
  std::string label;
  float raw_score = 0.0f;
  float weighted_score = 0.0f;
};

struct SearchHitV2 {
  int person_id = -1;
  std::string label;
  float score = 0.0f;
  int best_prototype_id = -1;
  int best_zone = 0;
};

struct SearchResult {
  bool ok = false;
  std::vector<SearchHit> hits;
  float top1_top2_margin = 0.0f;
};

struct SearchResultV2 {
  bool ok = false;
  std::vector<SearchHitV2> hits;
  std::vector<PrototypeHit> prototype_hits;
  float top1_top2_margin = 0.0f;
};

struct SearchIndexIoDiagnostic {
  bool ok = false;
  std::string error_message;
};

struct FaceSearchV2IndexRecord {
  int person_id = -1;
  int prototype_id = -1;
  int zone = 0;
  std::string label;
  float prototype_weight = 1.0f;
};

struct FaceSearchV2IndexPackage {
  int embedding_dim = 512;
  std::vector<FaceSearchV2IndexRecord> entries;
  std::vector<float> normalized_matrix;
};

constexpr const char* kFaceSearchV2IndexBinaryExtension = ".sfsi";

struct FaceSearchIndexEntry {
  int person_id = -1;
  std::string label;
  std::vector<float> embedding;
};

struct FaceSearchV2IndexEntry {
  int person_id = -1;
  int prototype_id = -1;
  int zone = 0;
  std::string label;
  float prototype_weight = 1.0f;
  std::vector<float> embedding;
};

class FaceSearch {
 public:
  explicit FaceSearch(const SearchConfig& config = SearchConfig {});

  bool AddPrototype(const FacePrototype& prototype);
  bool RebuildFromPrototypes(const std::vector<FacePrototype>& prototypes);
  void Clear();
  std::size_t PrototypeCount() const;
  SearchResult Query(const std::vector<float>& embedding) const;

 private:
  float CosineSimilarity(const std::vector<float>& a,
                         const std::vector<float>& b) const;
  bool UseDotKernelPath() const;

  SearchConfig config_;
  std::vector<FaceSearchIndexEntry> entries_;
  std::vector<float> normalized_matrix_;
};

class FaceSearchV2 {
 public:
  explicit FaceSearchV2(const SearchConfig& config = SearchConfig {});

  bool AddPrototype(const FacePrototypeV2& prototype);
  bool RebuildFromPrototypes(const std::vector<FacePrototypeV2>& prototypes);
  bool LoadFromIndexPackage(const FaceSearchV2IndexPackage& package);
  bool LoadFromIndexPackagePath(const std::string& input_path);
  bool ExportIndexPackage(FaceSearchV2IndexPackage* out_package) const;
  bool SaveIndexPackageBinary(const std::string& output_path) const;
  void Clear();
  std::size_t PrototypeCount() const;
  SearchResultV2 Query(const std::vector<float>& embedding) const;

 private:
  bool UseDotKernelPath() const;

  SearchConfig config_;
  std::vector<FaceSearchV2IndexEntry> entries_;
  std::vector<float> normalized_matrix_;
};

SearchResult ToSearchResult(const SearchResultV2& result_v2);

SearchIndexIoDiagnostic BuildFaceSearchV2IndexPackage(
    const std::vector<FacePrototypeV2>& prototypes,
    int embedding_dim,
    FaceSearchV2IndexPackage* out_package);

SearchIndexIoDiagnostic BuildAndSaveFaceSearchV2IndexPackageBinary(
    const std::vector<FacePrototypeV2>& prototypes,
    int embedding_dim,
    const std::string& output_path);

SearchIndexIoDiagnostic SaveFaceSearchV2IndexPackageBinary(
    const FaceSearchV2IndexPackage& package,
    const std::string& output_path);

SearchIndexIoDiagnostic LoadFaceSearchV2IndexPackageBinary(
    const std::string& input_path,
    FaceSearchV2IndexPackage* out_package);

}  // namespace sentriface::search

#endif  // SENTRIFACE_SEARCH_FACE_SEARCH_HPP_
