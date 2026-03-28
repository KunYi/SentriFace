#include "tracker/association.hpp"

#include <algorithm>
#include <cmath>

namespace sentriface::tracker {
namespace {

float Area(const RectF& r) {
  return std::max(0.0f, r.w) * std::max(0.0f, r.h);
}

float Diag(const RectF& r) {
  return std::sqrt(r.w * r.w + r.h * r.h);
}

Point2f Center(const RectF& r) {
  return Point2f {r.x + 0.5f * r.w, r.y + 0.5f * r.h};
}

float IoU(const RectF& a, const RectF& b) {
  const float x1 = std::max(a.x, b.x);
  const float y1 = std::max(a.y, b.y);
  const float x2 = std::min(a.x + a.w, b.x + b.w);
  const float y2 = std::min(a.y + a.h, b.y + b.h);
  const float iw = std::max(0.0f, x2 - x1);
  const float ih = std::max(0.0f, y2 - y1);
  const float inter = iw * ih;
  const float uni = Area(a) + Area(b) - inter;
  return uni > 0.0f ? inter / uni : 0.0f;
}

}  // namespace

Association::Association(const TrackerConfig& config) : config_(config) {}

std::vector<MatchCandidate> Association::BuildCandidates(
    const std::vector<Track>& tracks,
    const std::vector<Detection>& detections) const {
  std::vector<MatchCandidate> out;
  for (int ti = 0; ti < static_cast<int>(tracks.size()); ++ti) {
    if (tracks[ti].state == TrackState::kRemoved) {
      continue;
    }
    const RectF& pred = tracks[ti].bbox_pred;
    const Point2f pred_center = Center(pred);
    const float pred_diag = std::max(1.0f, Diag(pred));
    const float pred_area = std::max(1.0f, Area(pred));

    for (int di = 0; di < static_cast<int>(detections.size()); ++di) {
      const RectF& det = detections[di].bbox;
      const float iou = IoU(pred, det);
      if (iou < config_.iou_min) {
        continue;
      }

      const Point2f det_center = Center(det);
      const float dx = det_center.x - pred_center.x;
      const float dy = det_center.y - pred_center.y;
      const float center_distance = std::sqrt(dx * dx + dy * dy);
      if (center_distance > config_.center_gate_ratio * pred_diag) {
        continue;
      }

      const float det_area = std::max(1.0f, Area(det));
      const float area_ratio = det_area / pred_area;
      if (area_ratio < config_.area_ratio_min ||
          area_ratio > config_.area_ratio_max) {
        continue;
      }

      const float norm_center = center_distance / pred_diag;
      const float scale_change = std::abs(std::log(area_ratio));
      const float cost =
          0.55f * (1.0f - iou) + 0.30f * norm_center + 0.15f * scale_change;
      out.push_back(MatchCandidate {ti, di, cost});
    }
  }

  std::sort(out.begin(), out.end(),
            [](const MatchCandidate& a, const MatchCandidate& b) {
              return a.cost < b.cost;
            });
  return out;
}

}  // namespace sentriface::tracker
