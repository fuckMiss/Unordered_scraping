from pathlib import Path
import os
import subprocess
import sys

import openvino as ov


WEIGHTS_DIR = Path("weights")
ONNX_MODELS = {
    "obb": WEIGHTS_DIR / "best_obb.onnx",
    "seg": WEIGHTS_DIR / "best_seg.onnx",
}


def add_openvino_dll_directories() -> None:
    if os.name != "nt":
        return

    candidates = []
    for package_path in getattr(ov, "__path__", []):
        root = Path(package_path)
        candidates.extend([
            root,
            root / "libs",
            root / "runtime" / "bin" / "intel64" / "Release",
        ])

    env_root = os.environ.get("OPENVINO_ROOT")
    if env_root:
        root = Path(env_root)
        candidates.extend([
            root / "runtime" / "bin" / "intel64" / "Release",
            root / "runtime" / "3rdparty" / "tbb" / "bin",
        ])

    added = []
    for candidate in candidates:
        if candidate.exists():
            candidate_text = str(candidate)
            added.append(candidate_text)
            if hasattr(os, "add_dll_directory"):
                try:
                    os.add_dll_directory(candidate_text)
                except OSError:
                    pass

    if added:
        os.environ["PATH"] = os.pathsep.join(added + [os.environ.get("PATH", "")])
        print("[info] Added OpenVINO DLL search paths:")
        for path in added:
            print(f"       {path}")


def convert_to_openvino_ir(onnx_path: Path) -> Path:
    output_xml = onnx_path.with_suffix(".xml")
    errors = []

    try:
        if hasattr(ov, "convert_model"):
            model = ov.convert_model(str(onnx_path))
            ov.save_model(model, str(output_xml))
            return output_xml
    except Exception as exc:
        errors.append(f"openvino.convert_model failed: {exc}")

    try:
        from openvino.tools.mo import convert_model
        from openvino.runtime import serialize
        model = convert_model(str(onnx_path))
        serialize(model, str(output_xml), str(output_xml.with_suffix(".bin")))
        return output_xml
    except Exception as exc:
        errors.append(f"openvino.tools.mo.convert_model failed: {exc}")

    mo_cmd = [
        "mo",
        "--input_model",
        str(onnx_path),
        "--output_dir",
        str(onnx_path.parent),
    ]
    try:
        subprocess.run(mo_cmd, check=True)
        if output_xml.exists():
            return output_xml
    except FileNotFoundError:
        errors.append("mo command not found in PATH")
    except subprocess.CalledProcessError as exc:
        errors.append(f"mo command failed with exit code {exc.returncode}")

    raise RuntimeError(
        "Failed to convert ONNX to OpenVINO IR.\n"
        + "\n".join(f"- {error}" for error in errors)
        + "\n\nRecommended fixes:\n"
        "1. Reinstall OpenVINO in the active conda environment:\n"
        f"   {sys.executable} -m pip uninstall -y openvino openvino-dev openvino-telemetry\n"
        f"   {sys.executable} -m pip install -U openvino openvino-dev\n"
        "2. Install Microsoft Visual C++ Redistributable 2015-2022 x64 if DLL loading fails.\n"
        "3. Or run Model Optimizer manually:\n"
        f"   mo --input_model {onnx_path} --output_dir {onnx_path.parent}"
    )


def main() -> None:
    add_openvino_dll_directories()
    for name, onnx_path in ONNX_MODELS.items():
        if not onnx_path.exists():
            raise FileNotFoundError(f"{name} ONNX model not found: {onnx_path}")

        print(f"[ok] {name}: reuse ONNX {onnx_path}")
        xml_path = convert_to_openvino_ir(onnx_path)
        print(f"[ok] {name}: OpenVINO IR saved to {xml_path}")


if __name__ == "__main__":
    main()
