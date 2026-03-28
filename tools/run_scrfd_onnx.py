#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import numpy as np
import onnxruntime as ort
from PIL import Image


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run InsightFace SCRFD ONNX on images and export detections to CSV."
    )
    parser.add_argument("model_path", help="Path to SCRFD ONNX model.")
    parser.add_argument("input_path", help="Path to an image file or a directory of images.")
    parser.add_argument("output_csv", help="Path to output CSV.")
    parser.add_argument(
        "--input-width", type=int, default=640, help="Detector input width. Default: 640."
    )
    parser.add_argument(
        "--input-height", type=int, default=640, help="Detector input height. Default: 640."
    )
    parser.add_argument(
        "--score-threshold",
        type=float,
        default=0.5,
        help="Detection score threshold. Default: 0.5.",
    )
    parser.add_argument(
        "--nms-threshold", type=float, default=0.4, help="NMS threshold. Default: 0.4."
    )
    parser.add_argument(
        "--max-num", type=int, default=0, help="Keep at most top-k detections per image."
    )
    return parser.parse_args()


def softmax(values):
    max_values = np.max(values, axis=1, keepdims=True)
    exp_values = np.exp(values - max_values)
    return exp_values / np.sum(exp_values, axis=1, keepdims=True)


def distance2bbox(points, distance):
    x1 = points[:, 0] - distance[:, 0]
    y1 = points[:, 1] - distance[:, 1]
    x2 = points[:, 0] + distance[:, 2]
    y2 = points[:, 1] + distance[:, 3]
    return np.stack([x1, y1, x2, y2], axis=-1)


def distance2kps(points, distance):
    preds = []
    for index in range(0, distance.shape[1], 2):
      px = points[:, index % 2] + distance[:, index]
      py = points[:, (index % 2) + 1] + distance[:, index + 1]
      preds.append(px)
      preds.append(py)
    return np.stack(preds, axis=-1)


def nms(dets, threshold):
    x1 = dets[:, 0]
    y1 = dets[:, 1]
    x2 = dets[:, 2]
    y2 = dets[:, 3]
    scores = dets[:, 4]
    areas = (x2 - x1 + 1.0) * (y2 - y1 + 1.0)
    order = scores.argsort()[::-1]
    keep = []
    while order.size > 0:
        index = order[0]
        keep.append(index)
        xx1 = np.maximum(x1[index], x1[order[1:]])
        yy1 = np.maximum(y1[index], y1[order[1:]])
        xx2 = np.minimum(x2[index], x2[order[1:]])
        yy2 = np.minimum(y2[index], y2[order[1:]])
        width = np.maximum(0.0, xx2 - xx1 + 1.0)
        height = np.maximum(0.0, yy2 - yy1 + 1.0)
        inter = width * height
        overlap = inter / (areas[index] + areas[order[1:]] - inter)
        remaining = np.where(overlap <= threshold)[0]
        order = order[remaining + 1]
    return keep


class ScrfdOnnxRunner:
    def __init__(self, model_path, input_width, input_height, score_threshold, nms_threshold):
        self.session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
        self.input_name = self.session.get_inputs()[0].name
        self.output_names = [item.name for item in self.session.get_outputs()]
        self.input_width = input_width
        self.input_height = input_height
        self.score_threshold = score_threshold
        self.nms_threshold = nms_threshold
        self.input_mean = 127.5
        self.input_std = 128.0
        self.feature_strides = [8, 16, 32]
        self.anchors_per_location = 2
        self.use_kps = len(self.output_names) == 9
        self.center_cache = {}

    def _blob_from_image(self, image):
        resized = image.resize((self.input_width, self.input_height), Image.BILINEAR)
        array = np.asarray(resized, dtype=np.float32)
        array = array[:, :, ::-1]
        array = (array - self.input_mean) / self.input_std
        array = np.transpose(array, (2, 0, 1))
        return np.expand_dims(array, axis=0)

    def _anchor_centers(self, height, width, stride):
        key = (height, width, stride)
        if key in self.center_cache:
            return self.center_cache[key]

        centers = np.stack(np.mgrid[:height, :width][::-1], axis=-1).astype(np.float32)
        centers = (centers * stride).reshape((-1, 2))
        if self.anchors_per_location > 1:
            centers = np.stack([centers] * self.anchors_per_location, axis=1).reshape((-1, 2))
        self.center_cache[key] = centers
        return centers

    def detect(self, image):
        original_width, original_height = image.size
        image_ratio = float(original_height) / float(original_width)
        model_ratio = float(self.input_height) / float(self.input_width)
        if image_ratio > model_ratio:
            new_height = self.input_height
            new_width = int(new_height / image_ratio)
        else:
            new_width = self.input_width
            new_height = int(new_width * image_ratio)
        det_scale = float(new_height) / float(original_height)

        resized = image.resize((new_width, new_height), Image.BILINEAR)
        canvas = Image.new("RGB", (self.input_width, self.input_height))
        canvas.paste(resized, (0, 0))

        blob = self._blob_from_image(canvas)
        outputs = self.session.run(self.output_names, {self.input_name: blob})

        score_list = []
        bbox_list = []
        kps_list = []
        fmc = len(self.feature_strides)
        for index, stride in enumerate(self.feature_strides):
            scores = outputs[index]
            bbox_preds = outputs[index + fmc] * stride
            kps_preds = outputs[index + fmc * 2] * stride if self.use_kps else None

            height = self.input_height // stride
            width = self.input_width // stride
            centers = self._anchor_centers(height, width, stride)

            scores = scores.reshape((-1,))
            positive = np.where(scores >= self.score_threshold)[0]
            if positive.size == 0:
                continue

            bboxes = distance2bbox(centers, bbox_preds)
            score_list.append(scores[positive][:, np.newaxis])
            bbox_list.append(bboxes[positive])

            if self.use_kps and kps_preds is not None:
                kpss = distance2kps(centers, kps_preds).reshape((-1, 5, 2))
                kps_list.append(kpss[positive])

        if not score_list:
            return np.empty((0, 5), dtype=np.float32), np.empty((0, 5, 2), dtype=np.float32)

        scores = np.vstack(score_list)
        order = scores.ravel().argsort()[::-1]
        bboxes = np.vstack(bbox_list) / det_scale
        kpss = np.vstack(kps_list) / det_scale if self.use_kps else None

        det = np.hstack((bboxes, scores)).astype(np.float32, copy=False)
        det = det[order, :]
        keep = nms(det, self.nms_threshold)
        det = det[keep, :]
        if kpss is None:
            kpss = np.empty((0, 5, 2), dtype=np.float32)
        else:
            kpss = kpss[order, :, :]
            kpss = kpss[keep, :, :]

        return det, kpss


def collect_images(path: Path):
    if path.is_file():
        return [path]
    if path.is_dir():
        exts = {".jpg", ".jpeg", ".png", ".bmp"}
        return sorted([item for item in path.iterdir() if item.suffix.lower() in exts])
    raise FileNotFoundError(f"input path not found: {path}")


def main():
    args = parse_args()
    model_path = Path(args.model_path)
    input_path = Path(args.input_path)
    output_csv = Path(args.output_csv)

    if not model_path.is_file():
        raise SystemExit(f"model not found: {model_path}")

    runner = ScrfdOnnxRunner(
        str(model_path),
        args.input_width,
        args.input_height,
        args.score_threshold,
        args.nms_threshold,
    )
    images = collect_images(input_path)
    output_csv.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "frame_index",
        "image_path",
        "x",
        "y",
        "w",
        "h",
        "score",
        "l0x",
        "l0y",
        "l1x",
        "l1y",
        "l2x",
        "l2y",
        "l3x",
        "l3y",
        "l4x",
        "l4y",
    ]

    rows = 0
    with open(output_csv, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()

        for frame_index, image_path in enumerate(images):
            image = Image.open(image_path).convert("RGB")
            dets, kpss = runner.detect(image)
            if args.max_num > 0 and dets.shape[0] > args.max_num:
                dets = dets[: args.max_num]
                kpss = kpss[: args.max_num]

            for det_index in range(dets.shape[0]):
                x1, y1, x2, y2, score = dets[det_index].tolist()
                kps = kpss[det_index].reshape(-1).tolist() if kpss.size > 0 else [0.0] * 10
                writer.writerow(
                    {
                        "frame_index": frame_index,
                        "image_path": str(image_path),
                        "x": x1,
                        "y": y1,
                        "w": x2 - x1,
                        "h": y2 - y1,
                        "score": score,
                        "l0x": kps[0],
                        "l0y": kps[1],
                        "l1x": kps[2],
                        "l1y": kps[3],
                        "l2x": kps[4],
                        "l2y": kps[5],
                        "l3x": kps[6],
                        "l3y": kps[7],
                        "l4x": kps[8],
                        "l4y": kps[9],
                    }
                )
                rows += 1

    print(f"images_processed={len(images)}")
    print(f"rows_written={rows}")
    print(f"output_csv={output_csv}")


if __name__ == "__main__":
    main()
