# PLC Interface

TankEye-Iris writes grasp results to a PLC through Modbus TCP.

## Connection

- Protocol: Modbus TCP
- Default endpoint: `192.168.3.205:502`
- Implementation: `qt/robot_controller.cpp`

## Register Map

Current code writes consecutive 32-bit floating-point values:

```text
D500   center_x
D502   center_y
D504   angle_deg
D506   pick_status
D508   head_type
D1500  trigger word
```

## Written Values

`writeFrameResult()` writes the current primary target:

- `center_x`
- `center_y`
- `angle_deg`
- `pick_status`
- `head_type`

`writePlcTestValues()` writes fixed test values:

```text
D500 = 123.4
D502 = 56.7
D504 = 90.0
D506 = 1.0
D508 = 4.0
```

## Trigger Flow

When PLC link mode is enabled, the UI polls `D1500`.

```text
D1500 = 1
-> read trigger
-> run one detection
-> write D500/D502/D504/D506/D508
-> clear D1500
```

If there is no usable camera frame, the app writes an empty result. In the current logic this usually means `D506 = 3`.

## Pick Status

Current post-processing uses:

```text
1  pickable
2  multiple pickable targets
3  not pickable
```

## Notes

- PLC read/write calls are synchronous in the current implementation.
- `D1500` is cleared after a trigger is processed.
- If PLC register addresses, data type, or word order changes, update `qt/robot_controller.*`.
