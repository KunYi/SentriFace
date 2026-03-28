#!/usr/bin/env python3

import argparse
import csv
from pathlib import Path

import numpy as np
import onnxruntime as ort
from PIL import Image


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run face embedding ONNX from a baseline embedding input manifest."
    )
    parser.add_argument("model_path", help="Path to face embedding ONNX model.")
    parser.add_argument("input_manifest", help="Path to baseline embedding input manifest.")
    parser.add_argument("output_csv", help="Path to output embedding CSV.")
    parser.add_argument(
        "--input-width", type=int, default=112, help="Recognizer input width. Default: 112."
    )
    parser.add_argument(
        "--input-height", type=int, default=112, help="Recognizer input height. Default: 112."
    )
    return parser.parse_args()


def load_manifest(path: Path):
    user_id = ""
    user_name = ""
    records = []
    current = None

    with path.open("r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line:
                continue
            if line == "[record]":
                if current is not None:
                    records.append(current)
                current = {}
                continue
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            if current is None:
                if key == "user_id":
                    user_id = value
                elif key == "user_name":
                    user_name = value
                continue
            current[key] = value

    if current is not None:
        records.append(current)
    return user_id, user_name, records


class BaselineEmbeddingOnnxRunner:
    def __init__(self, model_path, input_width, input_height):
        self.session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
        self.input_name = self.session.get_inputs()[0].name
        self.output_name = self.session.get_outputs()[0].name
        self.input_width = input_width
        self.input_height = input_height

    def preprocess(self, image: Image.Image):
        resized = image.resize((self.input_width, self.input_height), Image.BILINEAR)
        array = np.asarray(resized, dtype=np.float32)
        if array.ndim == 2:
            array = np.stack([array, array, array], axis=-1)
        if array.shape[2] == 4:
            array = array[:, :, :3]
        array = array[:, :, ::-1]
        array = (array - 127.5) / 127.5
        array = np.transpose(array, (2, 0, 1))
        return np.expand_dims(array, axis=0)

    def run(self, image: Image.Image):
        blob = self.preprocess(image)
        outputs = self.session.run([self.output_name], {self.input_name: blob})
        embedding = outputs[0].reshape(-1).astype(np.float32)
        norm = np.linalg.norm(embedding)
        if norm > 1e-6:
            embedding = embedding / norm
        return embedding


def main():
    args = parse_args()
    model_path = Path(args.model_path)
    input_manifest = Path(args.input_manifest)
    output_csv = Path(args.output_csv)

    if not model_path.is_file():
        raise SystemExit(f"model not found: {model_path}")
    if not input_manifest.is_file():
        raise SystemExit(f"input_manifest not found: {input_manifest}")

    runner = BaselineEmbeddingOnnxRunner(str(model_path), args.input_width, args.input_height)
    user_id, user_name, records = load_manifest(input_manifest)
    embedding_dim = runner.session.get_outputs()[0].shape[-1]

    fieldnames = [
        "user_id",
        "user_name",
        "sample_index",
        "slot",
        "image_path",
        "quality_score",
        "yaw_deg",
        "pitch_deg",
        "roll_deg",
    ] + [f"e{i}" for i in range(int(embedding_dim))]

    output_csv.parent.mkdir(parents=True, exist_ok=True)
    written = 0
    with output_csv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for record in records:
            image_path = Path(record["image_path"])
            if not image_path.is_file():
                continue
            image = Image.open(image_path).convert("RGB")
            embedding = runner.run(image)

            row = {
                "user_id": user_id,
                "user_name": user_name,
                "sample_index": record.get("sample_index", ""),
                "slot": record.get("slot", ""),
                "image_path": str(image_path),
                "quality_score": record.get("quality_score", ""),
                "yaw_deg": record.get("yaw_deg", ""),
                "pitch_deg": record.get("pitch_deg", ""),
                "roll_deg": record.get("roll_deg", ""),
            }
            for index, value in enumerate(embedding.tolist()):
                row[f"e{index}"] = value
            writer.writerow(row)
            written += 1

    print(f"user_id={user_id}")
    print(f"user_name={user_name}")
    print(f"rows_written={written}")
    print(f"output_csv={output_csv}")


if __name__ == "__main__":
    main()
