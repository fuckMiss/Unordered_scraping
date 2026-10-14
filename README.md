# TankEye-Iris

TankEye-Iris is a Windows-based industrial vision application for robotic grasping. It closes the production workflow from image acquisition to target detection, machine-coordinate conversion, grasp validation, and PLC register output.

```text
material image -> target and pose -> image coordinates -> machine coordinates -> grasp validity -> PLC registers
```

The system is designed around a few hard requirements: the camera must provide reliable frames, the OpenVINO models must return trustworthy targets, calibration must convert pixels into machine coordinates, safety limits must reject unsafe picks, and the PLC contract must remain stable.

## System Boundary

Inputs:

- Hikrobot industrial camera frames. OpenCV camera 0 can be used as a debug fallback.
- OBB/SEG model files from the active engineering profile. `DG_8` is created only when the profile list is empty and initially uses `models/DG_8_weights/best_obb.xml` and `models/DG_8_weights/best_seg.xml`; it can be deleted when another profile exists. Runtime packages can also include `DG_10_weights`.
- Site configuration from `config/tankeye.json`, Qt `QSettings`, and nine-point calibration output.
- PLC trigger register for image capture.

Outputs:

- Qt UI visualization of targets, segmentation regions, angles, head type, grasp status, and runtime state.
- Modbus TCP writes to PLC registers for front/back axis, left/right axis, angle, pick status, and head type.
- Runtime logs written by default to `logs/tankeye_*.log`.

## Runtime Flow

1. Use `launch_tankeye.ps1` for manual development launches. Double-clicking `launch_tankeye_main_only.vbs` uses `StartupProfile AutoStart`.
2. The launcher prepares DLL search paths, OpenVINO device settings, model paths, log path, PLC overrides, window settings, and camera IP.
3. `tankeye-openvino_qt_app.exe` starts the Qt main window and reads `config/tankeye.json`.
4. `GraspWorkflow` loads the OBB and SEG OpenVINO models. The double-click entry requests the existing PLC grasp link after model loading; disabling startup in engineering settings still blocks that action.
5. The camera thread receives frames from the Hikrobot camera. If `TANKEYE_CAMERA_FALLBACK=1` is set and Hikrobot capture is unavailable, it can fall back to OpenCV camera 0.
6. Each frame keeps its original color BGR image for display, saving, and post-processing, while OBB and SEG receive a derived `BGR -> grayscale -> three-channel grayscale BGR` image. Shared preprocessing emits `[1, 1, H, W]` or `[1, 3, H, W]` according to the loaded model input shape.
7. Post-processing passes only structurally valid targets to the grasp workflow. OBB `left` is the pick point, `big/small` are head helper points, and SEG provides the material region.
8. Nine-point calibration converts the primary target center from image `X/Y` to machine `X/Y`.
9. Machine ROI, axis limits, and angle rules reject unsafe or invalid targets.
10. In PLC grasp mode, after the PLC capture trigger is received, the program writes the result registers and clears the trigger.

## Current 2.2.3 Notes

- The main UI uses an approximately 75% image area and 25% right-side control panel.
- The right-side panel uses a compact two-column layout and scales with the window.
- Images are shown in full, with edge blank space allowed. The display does not crop the source image.
- Image loading runs on a background thread to reduce UI stalls.
- The display mode selector has three modes: `隐藏` for normal hidden duplicate/debug overlays, `全显` for full debug overlays, and `无显示` for the raw image only.
- The `开始保存/停止保存` button records PLC-triggered detection frames to the runtime `images/` directory. The save session locks the display mode selected when saving starts and does not save manual image-detection or preview frames.
- PLC frame storage retains at most 1000 managed images by default. The administrator can change the limit from 1 to 100000; cleanup only removes the oldest `images/tankeye_plc_*.png` files after a new image is saved successfully.
- The `保存错误图片` action is available in normal and administrator modes after the first PLC frame completes. It stores the latest completed frame as a raw PNG plus a fixed `AllDebug` overlay under `error_images/`; these files are outside the normal image-retention limit.
- The 2.2.3 geometry restores the legacy rules used as the 2.1.9 candidate baseline: Canny-based three-edge refinement, rectangle reconstruction, second perpendicular-foot selection, fixed 45-pixel A construction, base-box reconstruction, and 2x expansion perpendicular to AC. PoseDetection::corners contains the base box reconstructed around final A/C. The real mechanical gripper uses its configured physical dimensions and normalized AC angle + 90 degrees. Type display compensation rotates the displayed frames and AC before command-center translation without modifying safety geometry.
- Login, administrator setup/reset, model paths, project naming, and relevant combo boxes have explicit Return/Enter behavior so Enter advances or submits only the intended action.
- The primary X/Y display prefers machine coordinates when calibration is available and falls back to image coordinates only when machine coordinates are unavailable.
- PLC writes still use machine coordinates when valid calibration exists.
- Runtime logs are timestamped and can be viewed in the app with latest-log auto refresh, search, level filtering, time filtering, and pagination.
- Packaged runs preserve `openvino_cache` by default. First OpenVINO GPU/CPU model compilation can still be slow, but later launches with the same package and models should usually be faster.
- The UI starts in normal mode. Admin and engineering settings require a valid machine-bound `config/admin_license.json`; ordinary detection can continue without admin authorization.
- `GraspMainWindow` implementation is currently split into shell, runtime, settings, common, and scale helpers under `app/qt/ui/` instead of one large `grasp_main_window.cpp`.

## Core Principles

- **The coordinate system is the source of truth.** Models output pixels; the machine needs calibrated machine coordinates. Nine-point calibration generates the homography and should use all 9 points on site when possible.
- **The PLC contract is more important than UI presentation.** The UI can change, but the meanings of `D500/D502/D504/D506/D508` must stay aligned with the PLC program.
- **Rejecting is safer than guessing.** If there is no unique primary target, no valid coordinate, an out-of-limit result, or an invalid structure, the system should write a reject status instead of inventing a pick.
- **Configuration belongs to the site.** `config/tankeye.json` provides defaults, engineering settings persist local overrides, and environment variables are for temporary launch or deployment overrides.
- **Runtime packages must be self-contained.** Target PCs should not depend on the source tree. A release package must include the EXE, models, config, Qt/OpenCV/OpenVINO/Hikrobot runtime DLLs, launcher scripts, and required tools.

## PLC Contract

The default register map comes from `config/tankeye.json`. If site addresses change, the JSON configuration is the reference.

| Register | Meaning |
| --- | --- |
| `D1500` | Capture trigger. PLC writes `1`; vision clears it to `0` after processing. |
| `D500` | Front/back axis. Defaults to machine `Y`; can be mapped to machine `X`. |
| `D502` | Left/right axis. Defaults to machine `X`; can be mapped to machine `Y`. |
| `D504` | Calibrated machine rotation angle. |
| `D506` | Pick status: `1` single valid target, `2` multiple valid targets, `3` rejected / not pickable. |
| `D508` | Head type: `0` unknown, `1..4` recognized classes. |

When a target is rejected, the program writes only `D506=3`. It does not overwrite the previous front/back, left/right, angle, or head-type registers.

## Configuration

By default, the application reads:

```text
config/tankeye.json
```

Common environment variables:

| Variable | Purpose |
| --- | --- |
| `TANKEYE_CONFIG_PATH` | Override the config file path. |
| `TANKEYE_PLC_HOST` | Override the PLC IP address. |
| `TANKEYE_PLC_PORT` | Override the PLC port. |
| `TANKEYE_PLC_SIM` | Enable simulated PLC mode with `1/true/yes/on`. |
| `TANKEYE_OPENVINO_DEVICE` | Select `AUTO`, `GPU`, or `CPU`. |
| `TANKEYE_OPENVINO_CACHE_DIR` | Set the OpenVINO compile cache directory. |
| `TANKEYE_DEBUG_POSTPROCESS` | Enable post-processing debug logs. |
| `TANKEYE_LOG_FILE` | Set the runtime log file path. |
| `TANKEYE_CAMERA_FALLBACK` | Allow fallback to OpenCV camera 0. |
| `TANKEYE_ADMIN_AUTH_KEY_FILE` | Override the local admin authorization key file. |

`tankeye.json` is the default layer. Values saved from the engineering settings window override site-tunable settings such as camera exposure, limits, axis mapping, compensation, angle calibration, and coordinate transform settings.

## Admin Mode

- Normal mode keeps production controls available and exposes the display mode selector, PLC frame saving, and target list.
- Admin mode unlocks engineering settings and runtime logs.
- First-time admin creation requires a valid machine-bound `config/admin_license.json`. On the target PC, open the admin authorization dialog and click `保存授权申请` to create a request JSON file such as:

```text
TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json
```

- On the maintainer PC, generate the license file with the private key:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate_admin_auth_code.ps1 -RequestFile .\TankEye_admin_license_request_TK-B57A-B3ED-5AA9-3F90.json -Output .\config\admin_license.json
```

- Put the generated `admin_license.json` into the target runtime `config` directory. The same PC normally needs this authorization only once; after that, admin login uses the admin account password.
- Admin account creation requires a recovery question and answer. Forgotten-password reset still requires a valid local license and the correct recovery answer.
- The private release key is `config/admin_auth.key`. It is intentionally ignored by git but is copied into formal runtime packages when present. Keep this file private and use the same key for packaging and license generation.

## Engineering Settings

- Camera IP, exposure, nine-point coordinate transform, limits, axis mapping, compensation, and angle calibration are persisted and reused on the next launch.
- Engineering profiles such as `DG_8` and `DG_10` keep model paths and site-tunable settings together. Save/stage/apply/create profile actions are routed through explicit lifecycle entry points so the UI dialog is not the authority for runtime switching.
- `DG_8` is an empty-list initialization default, not a permanent system profile. Deletion is allowed when another profile remains; deleting the active profile first loads a replacement and preserves the old profile if that load fails.
- `Exposure us` can be entered manually or updated through a single automatic exposure action.
- Limit protection filters targets in machine coordinates. ROI margin shrinks the allowed region inward to avoid edge picks.
- The mechanical gripper length and width settings draw the real gripper rectangle on the image when calibration is valid. Gripper-corner bounds and mask collision filtering can reject unsafe targets before PLC output.
- Axis mapping can switch between `front/back = machine Y, left/right = machine X` and `front/back = machine X, left/right = machine Y`. This affects PLC `D500/D502`, machine ROI, and limit checks.
- Front/back and left/right compensation are added only to the final PLC output and PLC test display. They do not change the calibration matrix or mechanical gripper geometry; final axis-limit decisions evaluate the compensated PLC values.
- Angle calibration uses:

```text
mechanical_angle = image_angle * direction + offset
```

Angle output can use either `0..360` or `-180..180`.

## Nine-Point Calibration

In the runtime package, start:

```text
nine_point_circle_picker.exe
```

When launched from the runtime directory, the tool writes:

```text
calibration_output/calibration_image_points.txt
```

Points are recorded in successful click order. The first successful point is `P1`, the second is `P2`, and so on. A click counts only when it lands inside, or close enough to, the black circle. Toolbar clicks and blank-area clicks do not consume point numbers. Use the toolbar undo action to remove the previous point.

## Build

This project currently targets Windows.

Main dependencies:

- CMake 3.12+
- MSVC x64
- Qt 5.15.2 msvc2019_64
- OpenCV
- OpenVINO C++ Runtime / Dev package
- Hikrobot MVS SDK. If `vendor/hik_mvs` exists locally, the project can use it, but this vendor SDK should generally not be committed to a public Git repository.

Common build commands:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Run from source:

```powershell
.\launch_tankeye.ps1 -Configuration Release -BuildDir build -WindowMode Maximized
```

Manual launches load default model paths only with -AutoLoadModels; AutoStart requests model loading and the existing PLC link. Check the active engineering profile and actual launch arguments when diagnosing model paths.

## Tests

The script below runs the default tests. Isolate the authentication settings and temporary directories first; do not run its original executable directly against user-level settings:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_build_tests.ps1 -BuildDir build -Configuration Release
```

Current CMake tests cover:

- Config loading: `app_config_service_test`
- Nine-point coordinate transform: `coordinate_transform_test`
- Engineering settings: `engineering_settings_service_test`
- Frame post-processing smoke test: `frame_postprocess_smoke`
- Grab limit evaluation: `grab_limit_evaluator_test`
- PLC output contract: `plc_result_contract_test`
- PLC register mapping: `robot_controller_register_map_test`
- UI runtime state presentation: `runtime_status_presenter_test`, `plc_runtime_state_test`
- Administrator authentication and Return/Enter behavior: `admin_auth_helpers_test`
- Engineering-widget wheel and Return/Enter behavior: `engineering_settings_spinbox_wheel_test`
- PLC image FIFO retention and error-image storage: `image_retention_test`, `error_image_store_test`
- OpenVINO one/three-channel inputs and model-folder detection: `openvino_input_test`, `model_folder_detector_test`
- Calibration and project-profile persistence: `calibration_profile_store_test`, `project_profile_store_test`

The 2026-10-12 release check completed 15 of the 17 default Release tests. `admin_auth_helpers_test` and `engineering_settings_spinbox_wheel_test` blocked in their Qt UI paths and were terminated, so the current release must not be described as a fully passing 17/17 suite. The geometry smoke test was also run separately and passed. The original authentication test clears user-level Windows QSettings; isolate its settings and temporary directories before execution. The 2.2.3 packaging check reran six related tests successfully; site GUI, camera and PLC acceptance remains unverified.

## Packaging

Build the Release executable first:

```powershell
cmake --build build --config Release --target tankeye-openvino_qt_app
```

Create a self-contained runtime package:

```powershell
.\scripts\package_runtime.ps1 -BuildDir build -ReleaseName TankEye-Iris_2.2.3 -Force
```

Output:

```text
dist/TankEye-Iris_2.2.3
dist/TankEye-Iris_2.2.3.zip
```

The runtime package includes the main executable, models, config, calibration output, sample image, runtime DLLs, launcher scripts, desktop shortcut helper, and usage guide. It should not include source code, CMake projects, Python scripts, tests, `.lib`, `.pdb`, `.pt`, or other development artifacts.

If `config/admin_auth.key` exists, the packaging script copies it into the runtime package so `admin_license.json` files generated with the same key are accepted on the target PC. The key file itself must not be committed to git.

When uploading to GitHub, do not commit `dist/`, `_deps/`, `vendor/hik_mvs/`, `samples/Data/`, or actual model files under `models/DG_8_weights/` and `models/DG_10_weights/`. Distribute model files through GitHub Releases, Git LFS, or a private deployment channel.

Create a desktop shortcut:

```powershell
.\scripts\create_desktop_shortcut.ps1
```

If the runtime folder is renamed or moved, recreate the desktop shortcut.

## Directory Map

| Path | Responsibility |
| --- | --- |
| `app/qt/main.cpp` | Qt entry point, log initialization, and startup wiring. |
| `app/qt/ui/` | Main window shell/runtime/settings modules, engineering settings window, status presentation, and frame overlays. |
| `app/qt/hardware/` | Hikrobot camera wrapper and PLC Modbus TCP control. |
| `app/qt/workflow/` | Grasp workflow, config services, coordinate transform, limits, PLC output contract, and post-processing. |
| `app/cli/` | OBB/SEG command-line inference entry points. |
| `core/inference/` | YOLOv11 OBB/SEG OpenVINO inference wrappers. |
| `core/common/` | Model, deployment, and shared utilities. |
| `config/` | Default site configuration. |
| `models/DG_8_weights/`, `models/DG_10_weights/` | OpenVINO model weight locations used by engineering profiles and runtime packages. |
| `scripts/` | Launch, deploy, package, test, calibration, and shortcut scripts. |
| `tests/` | C++ tests. |
| `runtime/` | Runtime-package documentation assets. |
| `vendor/hik_mvs/` | Optional local Hikrobot MVS headers, libraries, and runtime files. |
| `dist/` | Generated runtime packages. |

## Troubleshooting

- App does not start: check `logs/tankeye_*.log`.
- Models are not found: verify the active engineering profile model paths, usually `models/DG_8_weights/best_obb.xml` and `models/DG_8_weights/best_seg.xml`.
- Camera is not found: check the MVS driver, camera power, network segment, and camera IP. For debugging, set `TANKEYE_CAMERA_FALLBACK=1`.
- GPU startup fails: verify the main workflow with `-Device CPU`, then check Intel GPU drivers and OpenVINO runtime deployment.
- PLC cannot connect: verify the vision workflow with `-SimulatePlc`, then check PLC IP, port, register addresses, and network cabling.
- Target coordinates look wrong: check nine-point calibration order, machine coordinate entry, homography validity, ROI, and axis mapping.
- Admin authorization is rejected: verify `config/admin_license.json` exists, belongs to this PC, and was generated with the same `config/admin_auth.key`. Forgotten-password reset also requires the recovery answer configured when the admin account was created.

## Final Maintenance Baseline

The final local package is 2.2.3. Current status, package SHA256, verification limits and site acceptance items are maintained in [HANDOFF](docs/project-memory/HANDOFF.md). The remote legacy main and V7 have unrelated histories; publication preserves both histories with the final Windows source tree, using no force push.
