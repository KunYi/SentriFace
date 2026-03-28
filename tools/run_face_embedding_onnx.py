#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import numpy as np
import onnxruntime as ort
from PIL import Image


TEMPLATE_LANDMARKS = np.array(
    [
        [38.2946, 51.6963],
        [73.5318, 51.5014],
        [56.0252, 71.7366],
        [41.5493, 92.3655],
        [70.7299, 92.2041],
    ],
    dtype=np.float32,
)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run InsightFace face recognition ONNX on detected/aligned faces and export embeddings."
    )
    parser.add_argument("model_path", help="Path to face recognition ONNX model.")
    parser.add_argument("detection_csv", help="Path to detection CSV with 5 landmarks.")
    parser.add_argument("output_csv", help="Path to output embedding CSV.")
    parser.add_argument(
        "--input-width", type=int, default=112, help="Recognizer input width. Default: 112."
    )
    parser.add_argument(
        "--input-height", type=int, default=112, help="Recognizer input height. Default: 112."
    )
    parser.add_argument(
        "--score-threshold",
        type=float,
        default=0.5,
        help="Skip detections below this score. Default: 0.5.",
    )
    return parser.parse_args()


def estimate_similarity_transform(src_points, dst_points):
    src_mean = np.mean(src_points, axis=0)
    dst_mean = np.mean(dst_points, axis=0)

    src_centered = src_points - src_mean
    dst_centered = dst_points - dst_mean
    src_norm = np.sum(src_centered * src_centered)
    if src_norm < 1e-6:
        return None

    a_num = 0.0
    b_num = 0.0
    for i in range(src_points.shape[0]):
        sx, sy = src_centered[i]
        dx, dy = dst_centered[i]
        a_num += sx * dx + sy * dy
        b_num += sx * dy - sy * dx

    a = a_num / src_norm
    b = b_num / src_norm
    tx = dst_mean[0] - a * src_mean[0] + b * src_mean[1]
    ty = dst_mean[1] - b * src_mean[0] - a * src_mean[1]
    return np.array([[a, -b, tx], [b, a, ty]], dtype=np.float32)


def bilinear_sample(image_array, x, y):
    height, width, channels = image_array.shape
    x = min(max(x, 0.0), width - 1.0)
    y = min(max(y, 0.0), height - 1.0)

    x0 = int(np.floor(x))
    y0 = int(np.floor(y))
    x1 = min(x0 + 1, width - 1)
    y1 = min(y0 + 1, height - 1)
    dx = x - x0
    dy = y - y0

    top = image_array[y0, x0] * (1.0 - dx) + image_array[y0, x1] * dx
    bottom = image_array[y1, x0] * (1.0 - dx) + image_array[y1, x1] * dx
    return top * (1.0 - dy) + bottom * dy


def warp_affine(image, transform, output_width, output_height):
    image_array = np.asarray(image, dtype=np.float32)
    matrix = np.vstack([transform, [0.0, 0.0, 1.0]])
    inv = np.linalg.inv(matrix)[:2, :]

    aligned = np.zeros((output_height, output_width, 3), dtype=np.float32)
    for y in range(output_height):
        for x in range(output_width):
            src_x = inv[0, 0] * x + inv[0, 1] * y + inv[0, 2]
            src_y = inv[1, 0] * x + inv[1, 1] * y + inv[1, 2]
            aligned[y, x] = bilinear_sample(image_array, src_x, src_y)
    return aligned


class FaceEmbeddingOnnxRunner:
    def __init__(self, model_path, input_width, input_height):
        self.session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
        self.input_name = self.session.get_inputs()[0].name
        self.output_name = self.session.get_outputs()[0].name
        self.input_width = input_width
        self.input_height = input_height

    def preprocess(self, aligned_rgb):
        array = aligned_rgb.astype(np.float32)
        array = array[:, :, ::-1]
        array = (array - 127.5) / 127.5
        array = np.transpose(array, (2, 0, 1))
        return np.expand_dims(array, axis=0)

    def run(self, aligned_rgb):
        blob = self.preprocess(aligned_rgb)
        outputs = self.session.run([self.output_name], {self.input_name: blob})
        embedding = outputs[0].reshape(-1).astype(np.float32)
        norm = np.linalg.norm(embedding)
        if norm > 1e-6:
            embedding = embedding / norm
        return embedding


def read_detection_rows(path):
    with open(path, "r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def main():
    args = parse_args()
    model_path = Path(args.model_path)
    detection_csv = Path(args.detection_csv)
    output_csv = Path(args.output_csv)

    if not model_path.is_file():
        raise SystemExit(f"model not found: {model_path}")
    if not detection_csv.is_file():
        raise SystemExit(f"detection_csv not found: {detection_csv}")

    runner = FaceEmbeddingOnnxRunner(str(model_path), args.input_width, args.input_height)
    rows = read_detection_rows(detection_csv)

    embedding_dim = runner.session.get_outputs()[0].shape[-1]
    fieldnames = [
        "frame_index",
        "image_path",
        "x",
        "y",
        "w",
        "h",
        "score",
    ] + [f"e{i}" for i in range(int(embedding_dim))]

    output_csv.parent.mkdir(parents=True, exist_ok=True)
    images = {}
    written = 0
    with open(output_csv, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()

        for row in rows:
            score = float(row["score"])
            if score < args.score_threshold:
                continue

            image_path = row["image_path"]
            if image_path not in images:
                images[image_path] = Image.open(image_path).convert("RGB")
            image = images[image_path]

            src_landmarks = np.array(
                [
                    [float(row["l0x"]), float(row["l0y"])],
                    [float(row["l1x"]), float(row["l1y"])],
                    [float(row["l2x"]), float(row["l2y"])],
                    [float(row["l3x"]), float(row["l3y"])],
                    [float(row["l4x"]), float(row["l4y"])],
                ],
                dtype=np.float32,
            )
            transform = estimate_similarity_transform(src_landmarks, TEMPLATE_LANDMARKS)
            if transform is None:
                continue

            aligned = warp_affine(image, transform, args.input_width, args.input_height)
            embedding = runner.run(aligned)

            output_row = {
                "frame_index": row["frame_index"],
                "image_path": row["image_path"],
                "x": row["x"],
                "y": row["y"],
                "w": row["w"],
                "h": row["h"],
                "score": row["score"],
            }
            for index, value in enumerate(embedding.tolist()):
                output_row[f"e{index}"] = value
            writer.writerow(output_row)
            written += 1

    print(f"rows_written={written}")
    print(f"output_csv={output_csv}")


if __name__ == "__main__":
    main()
