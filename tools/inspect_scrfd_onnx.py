#!/usr/bin/env python3

import argparse
from pathlib import Path

import onnx
import onnxruntime as ort


def get_dims(value_info):
    dims = []
    tensor = value_info.type.tensor_type
    for dim in tensor.shape.dim:
        if dim.dim_value:
            dims.append(dim.dim_value)
        elif dim.dim_param:
            dims.append(dim.dim_param)
        else:
            dims.append("?")
    return dims


def main():
    parser = argparse.ArgumentParser(
        description="Inspect SCRFD ONNX model inputs and outputs."
    )
    parser.add_argument("model_path", help="Path to SCRFD ONNX model.")
    args = parser.parse_args()

    model_path = Path(args.model_path)
    if not model_path.is_file():
      raise SystemExit(f"model not found: {model_path}")

    model = onnx.load(str(model_path))
    print("graph_inputs")
    for item in model.graph.input:
        print(item.name, get_dims(item))

    print("graph_outputs")
    for item in model.graph.output:
        print(item.name, get_dims(item))

    print("--- session io ---")
    session = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
    for item in session.get_inputs():
        print("input", item.name, item.shape, item.type)
    for item in session.get_outputs():
        print("output", item.name, item.shape, item.type)

    print("--- model meta ---")
    meta = session.get_modelmeta()
    print("producer", meta.producer_name)
    print("graph_name", meta.graph_name)
    print("domain", meta.domain)
    print("description", meta.description)


if __name__ == "__main__":
    main()
