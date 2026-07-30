#!/usr/bin/env python3
"""
Pick nine calibration circle centers from a Hikrobot camera frame or a local image.

Examples:
  python scripts/nine_point_circle_picker.py --image samples/images/calibration.jpg
  python scripts/nine_point_circle_picker.py --camera
"""

from __future__ import annotations

import argparse
import ctypes
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import cv2
import numpy as np


MV_OK = 0
MV_GIGE_DEVICE = 0x00000001
MV_USB_DEVICE = 0x00000004
MV_ACCESS_EXCLUSIVE = 1
MV_ACQ_MODE_CONTINUOUS = 2
MV_TRIGGER_MODE_OFF = 0

PIX_MONO8 = 0x01080001
PIX_BAYER_GR8 = 0x01080008
PIX_BAYER_RG8 = 0x01080009
PIX_BAYER_GB8 = 0x0108000A
PIX_BAYER_BG8 = 0x0108000B
PIX_RGB8 = 0x02180014
PIX_BGR8 = 0x02180015

_DLL_DIR_HANDLES = []
TOOLBAR_HEIGHT = 42


class BitmapInfoHeader(ctypes.Structure):
    _fields_ = [
        ("biSize", ctypes.c_uint32),
        ("biWidth", ctypes.c_int32),
        ("biHeight", ctypes.c_int32),
        ("biPlanes", ctypes.c_uint16),
        ("biBitCount", ctypes.c_uint16),
        ("biCompression", ctypes.c_uint32),
        ("biSizeImage", ctypes.c_uint32),
        ("biXPelsPerMeter", ctypes.c_int32),
        ("biYPelsPerMeter", ctypes.c_int32),
        ("biClrUsed", ctypes.c_uint32),
        ("biClrImportant", ctypes.c_uint32),
    ]


class RgbQuad(ctypes.Structure):
    _fields_ = [
        ("rgbBlue", ctypes.c_ubyte),
        ("rgbGreen", ctypes.c_ubyte),
        ("rgbRed", ctypes.c_ubyte),
        ("rgbReserved", ctypes.c_ubyte),
    ]


class BitmapInfo(ctypes.Structure):
    _fields_ = [
        ("bmiHeader", BitmapInfoHeader),
        ("bmiColors", RgbQuad * 1),
    ]


class Rect(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long),
        ("top", ctypes.c_long),
        ("right", ctypes.c_long),
        ("bottom", ctypes.c_long),
    ]


class MvDeviceInfo(ctypes.Structure):
    pass


class MvDeviceInfoList(ctypes.Structure):
    _fields_ = [
        ("nDeviceNum", ctypes.c_uint),
        ("pDeviceInfo", ctypes.POINTER(MvDeviceInfo) * 256),
    ]


class MvPtrUnion(ctypes.Union):
    _fields_ = [
        ("p", ctypes.c_void_p),
        ("nAligning", ctypes.c_longlong),
    ]


class MvFrameOutInfoEx(ctypes.Structure):
    _fields_ = [
        ("nWidth", ctypes.c_ushort),
        ("nHeight", ctypes.c_ushort),
        ("enPixelType", ctypes.c_uint),
        ("nFrameNum", ctypes.c_uint),
        ("nDevTimeStampHigh", ctypes.c_uint),
        ("nDevTimeStampLow", ctypes.c_uint),
        ("nReserved0", ctypes.c_uint),
        ("nHostTimeStamp", ctypes.c_longlong),
        ("nFrameLen", ctypes.c_uint),
        ("nSecondCount", ctypes.c_uint),
        ("nCycleCount", ctypes.c_uint),
        ("nCycleOffset", ctypes.c_uint),
        ("fGain", ctypes.c_float),
        ("fExposureTime", ctypes.c_float),
        ("nAverageBrightness", ctypes.c_uint),
        ("nRed", ctypes.c_uint),
        ("nGreen", ctypes.c_uint),
        ("nBlue", ctypes.c_uint),
        ("nFrameCounter", ctypes.c_uint),
        ("nTriggerIndex", ctypes.c_uint),
        ("nInput", ctypes.c_uint),
        ("nOutput", ctypes.c_uint),
        ("nOffsetX", ctypes.c_ushort),
        ("nOffsetY", ctypes.c_ushort),
        ("nChunkWidth", ctypes.c_ushort),
        ("nChunkHeight", ctypes.c_ushort),
        ("nLostPacket", ctypes.c_uint),
        ("nUnparsedChunkNum", ctypes.c_uint),
        ("UnparsedChunkList", MvPtrUnion),
        ("nExtendWidth", ctypes.c_uint),
        ("nExtendHeight", ctypes.c_uint),
        ("nFrameLenEx", ctypes.c_ulonglong),
        ("nExtraType", ctypes.c_uint),
        ("nSubImageNum", ctypes.c_uint),
        ("SubImageList", MvPtrUnion),
        ("UserPtr", MvPtrUnion),
        ("nFirstLineEncoderCount", ctypes.c_uint),
        ("nLastLineEncoderCount", ctypes.c_uint),
        ("nReserved", ctypes.c_uint * 24),
    ]


class MvFrameOut(ctypes.Structure):
    _fields_ = [
        ("pBufAddr", ctypes.POINTER(ctypes.c_ubyte)),
        ("stFrameInfo", MvFrameOutInfoEx),
        ("nRes", ctypes.c_uint * 16),
    ]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def runtime_root() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return repo_root()


def default_output_dir() -> Path:
    return runtime_root() / "calibration_output"


def require_ok(ret: int, name: str) -> None:
    if ret != MV_OK:
        raise RuntimeError(f"{name} failed: 0x{ret:08X}")


def load_mvs_dll():
    root = runtime_root()
    runtime_dirs = [
        root,
        root / "vendor" / "hik_mvs" / "Runtime" / "Win64_x64",
        repo_root() / "vendor" / "hik_mvs" / "Runtime" / "Win64_x64",
        repo_root() / "build" / "Release",
    ]
    for directory in runtime_dirs:
        if directory.exists() and hasattr(os, "add_dll_directory"):
            _DLL_DIR_HANDLES.append(os.add_dll_directory(str(directory)))

    candidates = [Path("MvCameraControl.dll")]
    candidates.extend(directory / "MvCameraControl.dll" for directory in runtime_dirs)
    last_error: Optional[Exception] = None
    for dll_path in candidates:
        try:
            dll = ctypes.WinDLL(str(dll_path))
            break
        except Exception as exc:  # pragma: no cover - depends on installed SDK.
            last_error = exc
    else:
        raise RuntimeError(f"Could not load MvCameraControl.dll: {last_error}")

    dll.MV_CC_Initialize.restype = ctypes.c_int
    dll.MV_CC_Finalize.restype = ctypes.c_int
    dll.MV_CC_EnumDevices.argtypes = [ctypes.c_uint, ctypes.POINTER(MvDeviceInfoList)]
    dll.MV_CC_EnumDevices.restype = ctypes.c_int
    dll.MV_CC_CreateHandle.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(MvDeviceInfo)]
    dll.MV_CC_CreateHandle.restype = ctypes.c_int
    dll.MV_CC_OpenDevice.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_ushort]
    dll.MV_CC_OpenDevice.restype = ctypes.c_int
    dll.MV_CC_CloseDevice.argtypes = [ctypes.c_void_p]
    dll.MV_CC_CloseDevice.restype = ctypes.c_int
    dll.MV_CC_DestroyHandle.argtypes = [ctypes.c_void_p]
    dll.MV_CC_DestroyHandle.restype = ctypes.c_int
    dll.MV_CC_StartGrabbing.argtypes = [ctypes.c_void_p]
    dll.MV_CC_StartGrabbing.restype = ctypes.c_int
    dll.MV_CC_StopGrabbing.argtypes = [ctypes.c_void_p]
    dll.MV_CC_StopGrabbing.restype = ctypes.c_int
    dll.MV_CC_GetImageBuffer.argtypes = [ctypes.c_void_p, ctypes.POINTER(MvFrameOut), ctypes.c_uint]
    dll.MV_CC_GetImageBuffer.restype = ctypes.c_int
    dll.MV_CC_FreeImageBuffer.argtypes = [ctypes.c_void_p, ctypes.POINTER(MvFrameOut)]
    dll.MV_CC_FreeImageBuffer.restype = ctypes.c_int
    dll.MV_CC_SetEnumValue.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint]
    dll.MV_CC_SetEnumValue.restype = ctypes.c_int
    dll.MV_CC_SetEnumValueByString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
    dll.MV_CC_SetEnumValueByString.restype = ctypes.c_int
    return dll


def configure_camera(dll, handle: ctypes.c_void_p) -> None:
    # These calls may be read-only on some user sets. They are best-effort.
    dll.MV_CC_SetEnumValueByString(handle, b"AcquisitionMode", b"Continuous")
    dll.MV_CC_SetEnumValueByString(handle, b"TriggerMode", b"Off")
    dll.MV_CC_SetEnumValue(handle, b"AcquisitionMode", MV_ACQ_MODE_CONTINUOUS)
    dll.MV_CC_SetEnumValue(handle, b"TriggerMode", MV_TRIGGER_MODE_OFF)
    for pixel_format in (b"BGR8", b"BGR8_Packed", b"Mono8"):
        if dll.MV_CC_SetEnumValueByString(handle, b"PixelFormat", pixel_format) == MV_OK:
            break


def bayer_to_bgr(raw: np.ndarray, pixel_type: int) -> np.ndarray:
    code_map = {
        PIX_BAYER_GR8: cv2.COLOR_BAYER_GR2BGR,
        PIX_BAYER_RG8: cv2.COLOR_BAYER_RG2BGR,
        PIX_BAYER_GB8: cv2.COLOR_BAYER_GB2BGR,
        PIX_BAYER_BG8: cv2.COLOR_BAYER_BG2BGR,
    }
    code = code_map.get(pixel_type)
    if code is None:
        raise RuntimeError(f"Unsupported camera pixel type: 0x{pixel_type:08X}")
    return cv2.cvtColor(raw, code)


def frame_to_bgr(frame: MvFrameOut) -> np.ndarray:
    info = frame.stFrameInfo
    width = int(info.nExtendWidth or info.nWidth)
    height = int(info.nExtendHeight or info.nHeight)
    frame_len = int(info.nFrameLenEx or info.nFrameLen)
    if width <= 0 or height <= 0 or frame_len <= 0 or not frame.pBufAddr:
        raise RuntimeError("Invalid frame returned by Hikrobot SDK.")

    pixel_type = int(info.enPixelType)
    data = ctypes.string_at(frame.pBufAddr, frame_len)
    if pixel_type == PIX_BGR8:
        need = width * height * 3
        return np.frombuffer(data, dtype=np.uint8, count=need).reshape((height, width, 3)).copy()
    if pixel_type == PIX_RGB8:
        need = width * height * 3
        rgb = np.frombuffer(data, dtype=np.uint8, count=need).reshape((height, width, 3))
        return cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)
    if pixel_type == PIX_MONO8:
        need = width * height
        mono = np.frombuffer(data, dtype=np.uint8, count=need).reshape((height, width))
        return cv2.cvtColor(mono, cv2.COLOR_GRAY2BGR)
    if pixel_type in (PIX_BAYER_GR8, PIX_BAYER_RG8, PIX_BAYER_GB8, PIX_BAYER_BG8):
        need = width * height
        raw = np.frombuffer(data, dtype=np.uint8, count=need).reshape((height, width))
        return bayer_to_bgr(raw, pixel_type)
    raise RuntimeError(f"Unsupported camera pixel type: 0x{pixel_type:08X}")


def capture_hikrobot_frame(camera_index: int) -> np.ndarray:
    dll = load_mvs_dll()
    handle = ctypes.c_void_p()
    grabbing = False
    opened = False
    initialized = False
    frame = MvFrameOut()
    got_frame = False
    try:
        require_ok(dll.MV_CC_Initialize(), "MV_CC_Initialize")
        initialized = True

        devices = MvDeviceInfoList()
        require_ok(dll.MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, ctypes.byref(devices)), "MV_CC_EnumDevices")
        if devices.nDeviceNum <= 0:
            raise RuntimeError("No Hikrobot camera found.")
        if camera_index < 0 or camera_index >= int(devices.nDeviceNum):
            raise RuntimeError(f"Camera index {camera_index} is out of range; found {devices.nDeviceNum} device(s).")

        require_ok(dll.MV_CC_CreateHandle(ctypes.byref(handle), devices.pDeviceInfo[camera_index]), "MV_CC_CreateHandle")
        require_ok(dll.MV_CC_OpenDevice(handle, MV_ACCESS_EXCLUSIVE, 0), "MV_CC_OpenDevice")
        opened = True
        configure_camera(dll, handle)
        require_ok(dll.MV_CC_StartGrabbing(handle), "MV_CC_StartGrabbing")
        grabbing = True

        ret = dll.MV_CC_GetImageBuffer(handle, ctypes.byref(frame), 3000)
        require_ok(ret, "MV_CC_GetImageBuffer")
        got_frame = True
        return frame_to_bgr(frame)
    finally:
        if got_frame:
            dll.MV_CC_FreeImageBuffer(handle, ctypes.byref(frame))
        if grabbing:
            dll.MV_CC_StopGrabbing(handle)
        if opened:
            dll.MV_CC_CloseDevice(handle)
        if handle:
            dll.MV_CC_DestroyHandle(handle)
        if initialized:
            dll.MV_CC_Finalize()


def crop_roi(image: np.ndarray, x: float, y: float, roi_size: int):
    h, w = image.shape[:2]
    half = max(8, roi_size // 2)
    left = max(0, int(round(x)) - half)
    top = max(0, int(round(y)) - half)
    right = min(w, int(round(x)) + half)
    bottom = min(h, int(round(y)) + half)
    return image[top:bottom, left:right], left, top


def dark_blob_circle_center(gray: np.ndarray, click_local: Tuple[float, float]):
    blur = cv2.GaussianBlur(gray, (5, 5), 0)
    area_limit = gray.shape[0] * gray.shape[1] * 0.35
    min_area = max(18.0, gray.shape[0] * gray.shape[1] * 0.002)
    candidates = []

    for mode in (cv2.THRESH_BINARY_INV, cv2.THRESH_BINARY):
        _, binary = cv2.threshold(blur, 0, 255, mode | cv2.THRESH_OTSU)
        binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
        binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, np.ones((5, 5), np.uint8))
        num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(binary)
        for label in range(1, num_labels):
            area = float(stats[label, cv2.CC_STAT_AREA])
            if area < min_area or area > area_limit:
                continue

            cx, cy = centroids[label]
            dist = float(np.hypot(cx - click_local[0], cy - click_local[1]))
            component_mask = (labels == label).astype(np.uint8) * 255
            contours, _ = cv2.findContours(component_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            if not contours:
                continue

            contour = max(contours, key=cv2.contourArea)
            perimeter = float(cv2.arcLength(contour, True))
            if perimeter <= 1.0:
                continue
            circularity = 4.0 * np.pi * area / (perimeter * perimeter)
            (ex, ey), radius = cv2.minEnclosingCircle(contour)

            # Real calibration dots are compact and dark. Reject giant background regions.
            if radius < 3.0 or radius > min(gray.shape[:2]) * 0.35:
                continue
            if circularity < 0.35:
                continue
            if float(np.mean(gray[labels == label])) > 180.0:
                continue

            score = dist - circularity * 20.0 + abs(radius - min(gray.shape[:2]) * 0.12) * 0.5
            candidates.append((score, float(cx), float(cy), float(radius), "blob"))

    if not candidates:
        return None
    _, cx, cy, radius, method = min(candidates, key=lambda item: item[0])
    return cx, cy, radius, method


def hough_circle_center(gray: np.ndarray, click_local: Tuple[float, float]):
    blur = cv2.medianBlur(gray, 5)
    max_radius = max(8, min(gray.shape[:2]) // 2)
    circles = cv2.HoughCircles(
        blur,
        cv2.HOUGH_GRADIENT,
        dp=1.2,
        minDist=max(12, min(gray.shape[:2]) // 3),
        param1=100,
        param2=14,
        minRadius=3,
        maxRadius=max(8, min(max_radius, int(min(gray.shape[:2]) * 0.35))),
    )
    if circles is None:
        return None
    circles = np.round(circles[0, :]).astype(float)
    cx, cy, radius = min(circles, key=lambda c: np.hypot(c[0] - click_local[0], c[1] - click_local[1]))
    return float(cx), float(cy), float(radius), "hough"


def find_circle_center(image: np.ndarray, x: float, y: float, roi_size: int):
    roi, left, top = crop_roi(image, x, y, roi_size)
    if roi.size == 0:
        return None
    gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)
    click_local = (x - left, y - top)
    result = dark_blob_circle_center(gray, click_local)
    if result is None:
        result = hough_circle_center(gray, click_local)
    if result is None:
        return None
    cx, cy, radius, method = result
    click_distance = float(np.hypot(cx - click_local[0], cy - click_local[1]))
    allowed_distance = max(float(radius) + 5.0, float(radius) * 1.25)
    if click_distance > allowed_distance:
        return None
    return left + cx, top + cy, radius, method


def put_label(image: np.ndarray,
              text: str,
              origin: Tuple[int, int],
              scale: float = 0.62,
              color: Tuple[int, int, int] = (255, 255, 255),
              thickness: int = 2) -> None:
    if os.name == "nt" and any(ord(ch) > 127 for ch in text):
        if put_label_windows(image, text, origin, scale, color):
            return
    cv2.putText(image, text, origin, cv2.FONT_HERSHEY_SIMPLEX, scale, color, thickness, cv2.LINE_AA)


def put_label_windows(image: np.ndarray,
                      text: str,
                      origin: Tuple[int, int],
                      scale: float,
                      color: Tuple[int, int, int]) -> bool:
    try:
        x, baseline_y = origin
        font_height = max(14, int(round(28 * scale)))
        width = min(max(60, len(text) * font_height), image.shape[1] - x)
        height = min(max(24, font_height + 12), image.shape[0] - max(0, baseline_y - font_height - 4))
        top = max(0, baseline_y - font_height - 4)
        if width <= 0 or height <= 0 or x < 0 or x >= image.shape[1]:
            return False

        roi = image[top:top + height, x:x + width]
        if roi.size == 0:
            return False

        bgra = np.zeros((height, width, 4), dtype=np.uint8)
        bgra[:, :, :3] = roi
        bgra[:, :, 3] = 255

        gdi32 = ctypes.windll.gdi32
        user32 = ctypes.windll.user32
        user32.GetDC.argtypes = [ctypes.c_void_p]
        user32.GetDC.restype = ctypes.c_void_p
        user32.ReleaseDC.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        user32.DrawTextW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int, ctypes.POINTER(Rect), ctypes.c_uint]
        user32.DrawTextW.restype = ctypes.c_int
        gdi32.CreateCompatibleDC.argtypes = [ctypes.c_void_p]
        gdi32.CreateCompatibleDC.restype = ctypes.c_void_p
        gdi32.CreateDIBSection.argtypes = [ctypes.c_void_p, ctypes.POINTER(BitmapInfo), ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p), ctypes.c_void_p, ctypes.c_uint]
        gdi32.CreateDIBSection.restype = ctypes.c_void_p
        gdi32.SelectObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        gdi32.SelectObject.restype = ctypes.c_void_p
        gdi32.CreateFontW.restype = ctypes.c_void_p
        gdi32.SetBkMode.argtypes = [ctypes.c_void_p, ctypes.c_int]
        gdi32.SetTextColor.argtypes = [ctypes.c_void_p, ctypes.c_uint]
        gdi32.DeleteObject.argtypes = [ctypes.c_void_p]
        gdi32.DeleteDC.argtypes = [ctypes.c_void_p]
        hdc = user32.GetDC(None)
        mem_dc = gdi32.CreateCompatibleDC(hdc)
        bits = ctypes.c_void_p()
        bmi = BitmapInfo()
        bmi.bmiHeader.biSize = ctypes.sizeof(BitmapInfoHeader)
        bmi.bmiHeader.biWidth = width
        bmi.bmiHeader.biHeight = -height
        bmi.bmiHeader.biPlanes = 1
        bmi.bmiHeader.biBitCount = 32
        bmi.bmiHeader.biCompression = 0
        bitmap = gdi32.CreateDIBSection(mem_dc, ctypes.byref(bmi), 0, ctypes.byref(bits), None, 0)
        if not bitmap:
            gdi32.DeleteDC(mem_dc)
            user32.ReleaseDC(None, hdc)
            return False

        ctypes.memmove(bits, bgra.ctypes.data, bgra.nbytes)
        old_bitmap = gdi32.SelectObject(mem_dc, bitmap)
        font = gdi32.CreateFontW(
            font_height,
            0,
            0,
            0,
            600,
            0,
            0,
            0,
            1,
            0,
            0,
            5,
            0,
            "Microsoft YaHei",
        )
        old_font = gdi32.SelectObject(mem_dc, font)
        gdi32.SetBkMode(mem_dc, 1)
        b, g, r = color
        gdi32.SetTextColor(mem_dc, int(r) | (int(g) << 8) | (int(b) << 16))
        rect = Rect(0, 0, width, height)
        ctypes.windll.user32.DrawTextW(mem_dc, text, -1, ctypes.byref(rect), 0x00000000 | 0x00000004 | 0x00000020)

        result = np.ctypeslib.as_array((ctypes.c_ubyte * bgra.nbytes).from_address(bits.value))
        result = result.reshape((height, width, 4))
        roi[:, :, :] = result[:, :, :3]

        gdi32.SelectObject(mem_dc, old_font)
        gdi32.DeleteObject(font)
        gdi32.SelectObject(mem_dc, old_bitmap)
        gdi32.DeleteObject(bitmap)
        gdi32.DeleteDC(mem_dc)
        user32.ReleaseDC(None, hdc)
        return True
    except Exception:
        return False


class PickerState:
    def __init__(self, image: np.ndarray, roi_size: int, max_display: int, output_dir: Path):
        self.original = image
        self.roi_size = roi_size
        self.max_display = max_display
        self.output_dir = output_dir
        self.points: List[dict] = []
        self.button_rects: Dict[str, Tuple[int, int, int, int]] = {}
        self.pending_action: Optional[str] = None
        h, w = self.original.shape[:2]
        self.scale = min(1.0, float(max_display) / float(max(h, w))) if max(h, w) > 0 else 1.0
        self.saved_after_full = False

    def reset_image(self, image: np.ndarray) -> None:
        self.original = image
        self.points.clear()
        self.saved_after_full = False
        h, w = self.original.shape[:2]
        self.scale = min(1.0, float(self.max_display) / float(max(h, w))) if max(h, w) > 0 else 1.0

    def button_at(self, display_x: int, display_y: int) -> Optional[str]:
        for name, rect in self.button_rects.items():
            left, top, right, bottom = rect
            if left <= display_x <= right and top <= display_y <= bottom:
                return name
        return None

    def is_rect_hit(self, rect: Tuple[int, int, int, int], display_x: int, display_y: int) -> bool:
        left, top, right, bottom = rect
        return left <= display_x <= right and top <= display_y <= bottom

    def add_click(self, display_x: int, display_y: int) -> None:
        button = self.button_at(display_x, display_y)
        if button == "undo":
            self.undo()
            return
        if button in ("save", "reload", "quit"):
            self.pending_action = button
            return
        if display_y < TOOLBAR_HEIGHT:
            print("Toolbar click ignored.")
            return
        if len(self.points) >= 9:
            print("已经有 9 个点。请使用撤销或保存。")
            return
        x = float(display_x) / self.scale
        y = float(display_y) / self.scale
        result = find_circle_center(self.original, x, y, self.roi_size)
        if result is None:
            print(f"P{len(self.points) + 1}: not on a black circle near ({x:.1f}, {y:.1f}). Point ignored.")
            return
        cx, cy, radius, method = result
        point = {
            "point": f"P{len(self.points) + 1}",
            "image_x": round(cx, 3),
            "image_y": round(cy, 3),
            "radius": round(radius, 3),
            "method": method,
        }
        self.points.append(point)
        print(f"{point['point']}: x={point['image_x']:.3f}, y={point['image_y']:.3f}, r={point['radius']:.3f}, {method}")

    def undo(self) -> None:
        if self.points:
            removed = self.points.pop()
            self.saved_after_full = False
            print(f"Removed {removed['point']}.")

    def save(self) -> None:
        self.output_dir.mkdir(parents=True, exist_ok=True)
        txt_path = self.output_dir / "calibration_image_points.txt"
        rows = [
            {"point": p["point"], "image_x": p["image_x"], "image_y": p["image_y"]}
            for p in self.points
        ]
        with txt_path.open("w", encoding="utf-8") as file:
            file.write("九点标定图像坐标\n")
            file.write("坐标单位：像素\n\n")
            for row in rows:
                file.write(f"{row['point']}: 图像X={row['image_x']}, 图像Y={row['image_y']}\n")
        print(f"Saved {len(rows)} point(s):")
        print(f"  {txt_path}")
        if len(rows) != 9:
            print("Warning: expected 9 points for calibration.")

    def save_points(self) -> None:
        self.output_dir.mkdir(parents=True, exist_ok=True)
        txt_path = self.output_dir / "calibration_image_points.txt"
        with txt_path.open("w", encoding="utf-8") as file:
            file.write("九点标定图像坐标\n")
            file.write("坐标单位：像素\n\n")
            for point in self.points:
                file.write(f"{point['point']}: 图像X={point['image_x']}, 图像Y={point['image_y']}\n")
        print(f"Saved {len(self.points)} point(s):")
        print(f"  {txt_path}")
        if len(self.points) != 9:
            print("Warning: expected 9 points for calibration.")

    def render(self) -> np.ndarray:
        image = self.original.copy()
        for point in self.points:
            cx = int(round(point["image_x"]))
            cy = int(round(point["image_y"]))
            label = f"{point['point']} ({point['image_x']:.1f},{point['image_y']:.1f})"
            cv2.drawMarker(image, (cx, cy), (0, 0, 255), markerType=cv2.MARKER_CROSS, markerSize=22, thickness=2)
            cv2.circle(image, (cx, cy), max(4, int(round(point["radius"]))), (0, 220, 0), 2)
            cv2.putText(image, label, (cx + 8, max(20, cy - 8)), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 0, 255), 2)

        if self.scale < 1.0:
            display = cv2.resize(image, None, fx=self.scale, fy=self.scale, interpolation=cv2.INTER_AREA)
        else:
            display = image

        cv2.rectangle(display, (0, 0), (display.shape[1], TOOLBAR_HEIGHT), (0, 0, 0), -1)
        next_label = "状态：已完成" if len(self.points) >= 9 else f"状态：请点击 P{len(self.points) + 1}"
        status = f"{next_label}  {len(self.points)}/9"
        put_label(display, status, (12, 27), 0.68)

        self.button_rects.clear()
        x = max(220, min(display.shape[1] - 390, 330))
        buttons = [
            ("save", "保存", 70),
            ("reload", "重拍", 70),
            ("undo", "撤销", 70),
            ("quit", "退出", 70),
        ]
        for name, label, width in buttons:
            left, top = x, 7
            right, bottom = min(display.shape[1] - 8, x + width), 34
            if right - left < 42:
                break
            self.button_rects[name] = (left, top, right, bottom)
            enabled = name != "undo" or bool(self.points)
            fill = (58, 58, 58) if enabled else (34, 34, 34)
            cv2.rectangle(display, (left, top), (right, bottom), fill, -1)
            cv2.rectangle(display, (left, top), (right, bottom), (120, 120, 120), 1)
            put_label(display, label, (left + 10, 27), 0.54, (235, 235, 235), 1)
            x = right + 8
        return display


def read_image(path: Path) -> np.ndarray:
    image = cv2.imread(str(path), cv2.IMREAD_COLOR)
    if image is None:
        raise RuntimeError(f"Could not open image: {path}")
    return image


def prompt_source_from_gui() -> tuple[Optional[Path], bool]:
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox
    except Exception:
        return None, False

    root = tk.Tk()
    root.withdraw()
    use_camera = messagebox.askyesno(
        "九点取点",
        "是否从海康相机抓图？\n选择“否”将导入本地图片。",
        parent=root,
    )
    if use_camera:
        root.destroy()
        return None, True

    image_path = filedialog.askopenfilename(
        parent=root,
        title="选择标定图片",
        filetypes=[
            ("Image files", "*.png *.jpg *.jpeg *.bmp *.tif *.tiff"),
            ("All files", "*.*"),
        ],
    )
    root.destroy()
    if not image_path:
        return None, False
    return Path(image_path), False


def show_error_dialog(title: str, message: str) -> None:
    try:
        import tkinter as tk
        from tkinter import messagebox
    except Exception:
        return

    root = tk.Tk()
    root.withdraw()
    messagebox.showerror(title, message, parent=root)
    root.destroy()


def chinese_error_message(error: Exception) -> str:
    text = str(error)
    if "Could not load MvCameraControl.dll" in text:
        return (
            "无法加载海康相机运行库 MvCameraControl.dll。\n\n"
            "请确认：\n"
            "1. 九点标定工具在完整运行包目录内运行。\n"
            "2. 运行包内存在 MvCameraControl.dll。\n"
            "3. 如仍失败，请安装海康 MVS 运行环境。"
        )
    if "No Hikrobot camera found" in text:
        return (
            "没有找到海康工业相机。\n\n"
            "请确认：\n"
            "1. 相机已接电并连接到电脑网口。\n"
            "2. 电脑网卡 IP 与相机在同一网段。\n"
            "3. 相机没有被 MVS 或其他程序占用。\n"
            "4. 如果只是离线取点，请重新打开工具并选择“否”，导入本地图片。"
        )
    if "MV_CC_EnumDevices" in text:
        return (
            "枚举海康相机失败。\n\n"
            "请检查相机连接、网卡配置和海康驱动环境。\n"
            f"原始错误：{text}"
        )
    if "MV_CC_OpenDevice" in text or "MV_CC_CreateHandle" in text:
        return (
            "打开海康相机失败。\n\n"
            "请确认相机没有被 MVS、主程序或其他软件占用，并检查网络连接。\n"
            f"原始错误：{text}"
        )
    if "MV_CC_GetImageBuffer" in text:
        return (
            "相机取图失败。\n\n"
            "请确认相机在线、曝光正常、没有被其他程序占用。\n"
            f"原始错误：{text}"
        )
    if "Could not open image" in text:
        return (
            "无法打开选择的图片。\n\n"
            "请确认图片路径存在，格式为 png、jpg、bmp、tif 等常见图片格式。"
        )
    if "Unsupported camera pixel type" in text:
        return (
            "当前相机图像格式暂不支持。\n\n"
            "请在 MVS 中把 PixelFormat 调整为 Mono8、BGR8、RGB8 或常见 Bayer8 格式后重试。"
        )
    return f"九点标定工具运行失败。\n\n原始错误：{text}"


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Pick 9 calibration circle centers from an image or Hikrobot camera.")
    source = parser.add_mutually_exclusive_group(required=False)
    source.add_argument("--image", type=Path, help="Local image path for offline testing.")
    source.add_argument("--camera", action="store_true", help="Grab one frame from a Hikrobot industrial camera.")
    parser.add_argument("--camera-index", type=int, default=0, help="Hikrobot device index when multiple cameras are present.")
    parser.add_argument("--output-dir", type=Path, default=None, help="Output folder for CSV/JSON.")
    parser.add_argument("--roi-size", type=int, default=180, help="Local search square size around each click, in original image pixels.")
    parser.add_argument("--max-display", type=int, default=1400, help="Max display dimension. Exported coordinates remain original image coordinates.")
    return parser


def main() -> int:
    args = build_arg_parser().parse_args()
    if args.output_dir is None:
        args.output_dir = default_output_dir()

    def load_source() -> np.ndarray:
        if args.image is not None:
            return read_image(args.image)
        return capture_hikrobot_frame(args.camera_index)

    if args.image is None and not args.camera:
        gui_image, gui_camera = prompt_source_from_gui()
        if gui_camera:
            args.camera = True
        elif gui_image is not None:
            args.image = gui_image
        elif args.image is None and not args.camera:
            print("No input selected. Exiting.")
            return 1

    try:
        image = load_source()
    except Exception as exc:
        message = chinese_error_message(exc)
        print(message)
        show_error_dialog("九点标定错误", message)
        return 1
    state = PickerState(image, args.roi_size, args.max_display, args.output_dir)
    window_name = "Nine Point Circle Picker"

    def on_mouse(event, x, y, _flags, _userdata):
        if event == cv2.EVENT_LBUTTONDOWN:
            state.add_click(x, y)

    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    cv2.setMouseCallback(window_name, on_mouse)

    print("Click near each circle center in P1 -> P9 order.")
    print("Keys: s save, u undo, r reload/recapture, q quit")
    while True:
        try:
            if cv2.getWindowProperty(window_name, cv2.WND_PROP_VISIBLE) < 1:
                break
        except cv2.error:
            break
        cv2.imshow(window_name, state.render())
        if len(state.points) == 9 and not state.saved_after_full:
            state.save_points()
            state.saved_after_full = True
        if state.pending_action == "save":
            state.pending_action = None
            state.save_points()
        elif state.pending_action == "reload":
            state.pending_action = None
            try:
                image = load_source()
            except Exception as exc:
                message = chinese_error_message(exc)
                print(message)
                show_error_dialog("九点标定错误", message)
                continue
            state.reset_image(image)
            print("Image reloaded; points cleared.")
        elif state.pending_action == "quit":
            state.pending_action = None
            break
        key = cv2.waitKey(30) & 0xFF
        if key == ord("q") or key == 27:
            break
        if key == ord("s"):
            state.save_points()
        elif key == ord("u"):
            state.undo()
        elif key == ord("r"):
            try:
                image = load_source()
            except Exception as exc:
                message = chinese_error_message(exc)
                print(message)
                show_error_dialog("九点标定错误", message)
                continue
            state.reset_image(image)
            print("Image reloaded; points cleared.")

    cv2.destroyAllWindows()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
