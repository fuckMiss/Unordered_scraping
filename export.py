from ultralytics import YOLO


# Load the YOLO segmentation model
model = YOLO("/home/cll/下载/best_obb.pt")

# Export the model to ONNX format
export_path = model.export(format="onnx")

print(f"SEG model exported to {export_path}")
