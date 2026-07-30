from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export YOLO PT weights to OpenVINO XML/BIN pairs."
    )
    parser.add_argument("--obb-pt", type=Path, default=None, help="Path to the OBB .pt file.")
    parser.add_argument("--seg-pt", type=Path, default=None, help="Path to the SEG .pt file.")
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("models/weights"),
        help="Output directory for the final XML/BIN files.",
    )
    parser.add_argument("--obb-name", default="best_obb", help="Output base name for the OBB model.")
    parser.add_argument("--seg-name", default="best_seg", help="Output base name for the SEG model.")
    parser.add_argument("--imgsz", type=int, default=640, help="Export image size.")
    parser.add_argument("--device", default="cpu", help="Export device for Ultralytics.")
    parser.add_argument("--no-half", action="store_true", help="Disable FP16 export.")
    parser.add_argument("--no-simplify", action="store_true", help="Disable graph simplification.")
    parser.add_argument("--overwrite", action="store_true", help="Overwrite existing XML/BIN files.")
    return parser.parse_args()


def require_ultralytics():
    try:
        from ultralytics import YOLO
    except Exception as exc:  # pragma: no cover - runtime dependency guard
        raise SystemExit(
            "Ultralytics is required. Install it first, for example: pip install ultralytics"
        ) from exc
    return YOLO


def find_openvino_pair(export_root: Path) -> tuple[Path, Path]:
    if export_root.is_file() and export_root.suffix.lower() == ".xml":
        xml_path = export_root
        bin_path = export_root.with_suffix(".bin")
        if bin_path.exists():
            return xml_path, bin_path
        raise FileNotFoundError(f"Missing BIN file next to {xml_path}")

    if export_root.is_dir():
        xml_candidates = sorted(export_root.rglob("*.xml"))
        for xml_path in xml_candidates:
            bin_path = xml_path.with_suffix(".bin")
            if bin_path.exists():
                return xml_path, bin_path
            if xml_path.stem == "model":
                candidate_bin = xml_path.parent / "model.bin"
                if candidate_bin.exists():
                    return xml_path, candidate_bin

    raise FileNotFoundError(f"Could not locate OpenVINO XML/BIN pair in {export_root}")


def export_one(yolo_cls, pt_path: Path, out_dir: Path, out_name: str, imgsz: int, device: str,
               half: bool, simplify: bool, overwrite: bool) -> None:
    if not pt_path.exists():
        raise FileNotFoundError(f"PT file not found: {pt_path}")

    print(f"[export] loading {pt_path}")
    model = yolo_cls(str(pt_path))
    export_result = model.export(
        format="openvino",
        imgsz=imgsz,
        device=device,
        half=half,
        simplify=simplify,
        dynamic=False,
        verbose=False,
    )

    export_root = Path(export_result)
    xml_src, bin_src = find_openvino_pair(export_root)

    out_dir.mkdir(parents=True, exist_ok=True)
    xml_dst = out_dir / f"{out_name}.xml"
    bin_dst = out_dir / f"{out_name}.bin"

    if not overwrite and (xml_dst.exists() or bin_dst.exists()):
        raise FileExistsError(f"Target files already exist: {xml_dst} / {bin_dst}")

    shutil.copy2(xml_src, xml_dst)
    shutil.copy2(bin_src, bin_dst)
    print(f"[export] wrote {xml_dst}")
    print(f"[export] wrote {bin_dst}")


def main() -> int:
    args = parse_args()
    YOLO = require_ultralytics()

    jobs: list[tuple[Path, str]] = []
    if args.obb_pt is not None:
        jobs.append((args.obb_pt, args.obb_name))
    if args.seg_pt is not None:
        jobs.append((args.seg_pt, args.seg_name))

    if not jobs:
        print("Nothing to export. Provide --obb-pt and/or --seg-pt.", file=sys.stderr)
        return 2

    half = not args.no_half
    simplify = not args.no_simplify

    for pt_path, out_name in jobs:
        export_one(
            YOLO,
            pt_path,
            args.out_dir,
            out_name,
            args.imgsz,
            args.device,
            half,
            simplify,
            args.overwrite,
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
