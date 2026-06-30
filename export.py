from ultralytics import YOLO


# Load the YOLO segmentation model
model = YOLO("weights\\best_seg.pt")

# Export the model to ONNX format
export_path = model.export(format="onnx")

print(f"SEG model exported to {export_path}")

#/usr/src/tensorrt/bin/trtexec --onnx=./weights/best_obb.onnx --saveEngine=./weights/best_obb.engine --fp16

# /usr/src/tensorrt/bin/trtexec --onnx=./weights/best_seg.onnx --saveEngine=./weights/best_seg.engine --fp16