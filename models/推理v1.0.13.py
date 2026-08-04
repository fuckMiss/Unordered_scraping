# 最顶部优先导入环境变量设置，必须放在所有import最前面
import os
os.environ["KMP_DUPLICATE_LIB_OK"] = "TRUE"

import matplotlib.pyplot as plt
import numpy as np
import torch
import cv2
from ultralytics import YOLO
from shapely.geometry import LineString, Polygon, Point
from shapely.ops import nearest_points

# -------------------------- 配置参数区 --------------------------
OBB_MODEL_PATH = "new_obb.pt"
SEG_MODEL_PATH = "best_seg.pt"
LONG_EDGE_SCALE = 2.0  # 长边放大倍数：2倍
# 类别配色
COLOR_MAP = {
    0: (1.0, 0.0, 0.0),    # left(A点): 红色
    1: (1.0, 1.0, 0.0),    # big(B点): 黄色
    2: (1.0, 1.0, 0.0)     # small(B点): 黄色
}
# 绘图配色常量
COLOR_O = (0, 0, 1.0)       # 蓝色：分组掩码最小外接矩形中心O
COLOR_OA = (0, 0.8, 1.0)    # 浅蓝：OA连线参考方向
COLOR_AC = (0, 1.0, 0)      # 绿色：最终选定垂线射线AC
COLOR_MASK_BOX = (0, 0, 1.0)# 蓝色实线：seg掩码倾斜最小外接矩形
RAY_EXTEND_LENGTH = 80      # 缩短射线长度
RIGHT_ANGLE_RAD = np.pi / 2 # 90度弧度值，用于筛选最接近直角的∠AOB
# ----------------------------------------------------------------

def get_obb_center(obb_8pts):
    """获取OBB框中心点坐标 (cx, cy)"""
    pts = np.array(obb_8pts).reshape(4, 2)
    cx = np.mean(pts[:, 0])
    cy = np.mean(pts[:, 1])
    return np.array([cx, cy])

def draw_obb_box(ax, obb_coords, color, linewidth=2, linestyle="-"):
    """绘制旋转OBB包围框"""
    pts = np.array(obb_coords).reshape(4, 2)
    pts = np.vstack([pts, pts[0]])
    ax.plot(pts[:, 0], pts[:, 1], color=color, linewidth=linewidth, linestyle=linestyle)

def get_obb_main_angle(obb_8pts):
    """
    获取OBB长边方向的角度（弧度）
    遍历两条邻边，区分长边/短边，返回长边朝向角度
    """
    pts = np.array(obb_8pts).reshape(4, 2)
    # 两条相邻边向量
    vec1 = pts[1] - pts[0]
    vec2 = pts[3] - pts[0]

    len1 = np.linalg.norm(vec1)
    len2 = np.linalg.norm(vec2)

    if len1 >= len2:
        main_vec = vec1
        long_idx = 1
        short_idx = 3
    else:
        main_vec = vec2
        long_idx = 3
        short_idx = 1
    angle = np.arctan2(main_vec[1], main_vec[0])
    return angle, long_idx, short_idx

def scale_obb_long_edge(obb_8pts, scale_factor):
    """
    长边延长N倍，中心点、角度、短边宽度完全保持不变
    :param obb_8pts: 原始obb八点数组
    :param scale_factor: 长边缩放倍数(2.0)
    :return: 缩放后四点坐标 (4,2)
    """
    pts = np.array(obb_8pts).reshape(4, 2)
    center = get_obb_center(obb_8pts)
    _, long_side_idx, short_side_idx = get_obb_main_angle(obb_8pts)

    # 以中心点为基准，拆分长边、短边方向向量
    p0 = pts[0]
    p_long = pts[long_side_idx]
    p_short = pts[short_side_idx]

    # 长边方向向量
    vec_long = p_long - p0
    # 短边方向向量（宽度方向，保持不变）
    vec_short = p_short - p0

    # 长边向量缩放
    vec_long_scaled = vec_long * scale_factor

    # 重新计算四个顶点
    new_p0 = center - (vec_long_scaled + vec_short) / 2
    new_p1 = new_p0 + vec_long_scaled
    new_p2 = new_p1 + vec_short
    new_p3 = new_p0 + vec_short

    scaled_4pts = np.array([new_p0, new_p1, new_p2, new_p3])
    return scaled_4pts

def rotate_polygon_optimal(polygon_pts, target_long_angle):
    """
    最优角度旋转：二选一
    方案1：红长边 || 黄长边（目标角度=target_long_angle）
    方案2：红长边 || 黄短边（目标角度=target_long_angle + np.pi/2）
    自动选择旋转角度差更小的方案返回旋转后的框
    :param polygon_pts: 原始obb四点坐标 (4,2)
    :param target_long_angle: 黄色框长边角度
    :return: 最优旋转后的四点坐标 (4,2)
    """
    # 获取红色框自身长边原始角度
    red_raw_angle, _, _ = get_obb_main_angle(polygon_pts.reshape(-1))
    # 两种目标角度
    angle_plan1 = target_long_angle          # 长边平行长边
    angle_plan2 = target_long_angle + np.pi/2# 长边平行短边

    # 计算环绕圆周最小角度差 (-π ~ π)
    def min_angle_diff(a, b):
        diff = (a - b) % (2 * np.pi)
        if diff > np.pi:
            diff -= 2 * np.pi
        return abs(diff)

    diff1 = min_angle_diff(red_raw_angle, angle_plan1)
    diff2 = min_angle_diff(red_raw_angle, angle_plan2)

    # 选择旋转幅度更小的目标角度
    if diff1 <= diff2:
        final_target_angle = angle_plan1
    else:
        final_target_angle = angle_plan2

    # 执行旋转
    delta_angle = final_target_angle - red_raw_angle
    center = np.mean(polygon_pts, axis=0)
    pts_shift = polygon_pts - center

    cos_theta = np.cos(delta_angle)
    sin_theta = np.sin(delta_angle)
    rot_mat = np.array([
        [cos_theta, -sin_theta],
        [sin_theta,  cos_theta]
    ])

    pts_rot = (rot_mat @ pts_shift.T).T
    pts_final = pts_rot + center
    return pts_final

def point_in_mask(point, seg_mask, orig_img_shape):
    """
    坐标归一化映射：将原图像素坐标映射到mask尺寸，杜绝越界报错
    point: [x, y] 原图像素坐标
    seg_mask: (mask_h, mask_w) 分割掩码浮点数组
    orig_img_shape: (H, W) 原图高、宽
    """
    img_h, img_w = orig_img_shape
    mask_h, mask_w = seg_mask.shape

    px, py = point[0], point[1]
    # 归一化 0~1
    norm_x = px / img_w
    norm_y = py / img_h

    # 映射到mask像素坐标
    x = int(round(norm_x * mask_w))
    y = int(round(norm_y * mask_h))

    # 严格裁剪边界，防止索引溢出
    x = np.clip(x, 0, mask_w - 1)
    y = np.clip(y, 0, mask_h - 1)

    return seg_mask[y, x] > 0.5

def get_mask_rotated_min_bbox(mask, orig_h, orig_w):
    """
    获取掩码倾斜最小外接矩形（旋转OBB），映射回原图尺寸
    返回：obb四点坐标(原图像素)、矩形中心点O
    """
    # 获取掩码所有前景像素坐标
    y_coords, x_coords = np.where(mask > 0.5)
    if len(x_coords) == 0:
        return None, None

    mask_h, mask_w = mask.shape
    # 构造mask下像素点集
    pts_mask = np.column_stack([x_coords, y_coords]).astype(np.float32)
    # OpenCV求取旋转最小包围矩形
    rect = cv2.minAreaRect(pts_mask)
    box_pts_mask = cv2.boxPoints(rect)  # mask坐标系下4个角点

    # 将mask坐标映射回原图像素坐标
    def map_to_original(pt):
        x = pt[0] / mask_w * orig_w
        y = pt[1] / mask_h * orig_h
        return np.array([x, y])

    box_pts_origin = np.array([map_to_original(p) for p in box_pts_mask])
    center_O = np.mean(box_pts_origin, axis=0)
    return box_pts_origin, center_O

def get_perpendicular_foot_points(obb_4pts, center_A):
    """
    中心点A向OBB四条边分别作垂线，获取4个垂足C列表
    :param obb_4pts: OBB四个顶点 (4,2)
    :param center_A: OBB中心点A
    :return: [(垂足坐标, 边线段shapely对象), ...]
    """
    foot_list = []
    edges = []
    for i in range(4):
        p_start = obb_4pts[i]
        p_end = obb_4pts[(i+1)%4]
        edges.append((p_start, p_end))
    # 遍历四条边求垂足
    for (p1, p2) in edges:
        line_seg = LineString([tuple(p1), tuple(p2)])
        pt_A = Point(center_A)
        # 求取线段上距离A最近点=垂足
        foot_pt = nearest_points(line_seg, pt_A)[0]
        foot_coords = np.array([foot_pt.x, foot_pt.y])
        foot_list.append((foot_coords, line_seg))
    return foot_list

def calculate_angle_between_vectors(vec1, vec2):
    """计算两个向量之间的夹角 弧度 [0, π]"""
    dot = np.dot(vec1, vec2)
    norm1 = np.linalg.norm(vec1)
    norm2 = np.linalg.norm(vec2)
    if norm1 < 1e-6 or norm2 < 1e-6:
        return 0.0
    cos_ang = np.clip(dot / (norm1 * norm2), -1.0, 1.0)
    return np.arccos(cos_ang)

def visualize_group_aligned(img_path):
    # 加载OBB检测模型 + 分割模型
    obb_model = YOLO(OBB_MODEL_PATH)
    seg_model = YOLO(SEG_MODEL_PATH)

    # 推理图片
    obb_res = obb_model(img_path)[0]
    seg_res = seg_model(img_path)[0]

    fig, ax = plt.subplots(figsize=(12, 9))
    img_bgr = obb_res.orig_img
    # OpenCV读取图片默认BGR通道，转换成RGB供matplotlib正确显示
    img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    orig_h, orig_w = img_rgb.shape[:2]
    ax.imshow(img_rgb)

    # 1. 解析所有OBB结果
    obb_result = obb_res.obb
    if obb_result is None:
        print("未检测到任何OBB目标")
        ax.axis("off")
        plt.tight_layout()
        plt.show()
        return

    boxes = obb_result.xyxyxyxy.cpu().numpy()
    cls_ids = obb_result.cls.cpu().numpy().astype(int)
    confs = obb_result.conf.cpu().numpy()

    # 划分：红色left(A点框)、黄色big/small(B点框)
    red_list = []   # [(obb8pts, conf, center_A)]
    yellow_list = []# [(obb8pts, cls, conf, center_B)]
    for box, cls, conf in zip(boxes, cls_ids, confs):
        ctr = get_obb_center(box)
        if cls == 0:
            red_list.append([box, conf, ctr])
        else:
            yellow_list.append([box, cls, conf, ctr])

    # 2. 解析分割所有实例掩码
    seg_masks = []
    if seg_res.masks is not None:
        seg_masks = seg_res.masks.data.cpu().numpy()  # [N, H_mask, W_mask]

    # 3. 按分割实例分组 + 新增B点筛选逻辑
    groups = []
    used_yellow_idx = set()
    used_red_idx = set()

    for mask in seg_masks:
        raw_group_yellow = []
        raw_group_red = []

        # 匹配本组所有黄色B框
        for y_idx, y_item in enumerate(yellow_list):
            if y_idx in used_yellow_idx:
                continue
            _, _, _, b_ctr = y_item
            if point_in_mask(b_ctr, mask, (orig_h, orig_w)):
                raw_group_yellow.append((y_idx, y_item))
                used_yellow_idx.add(y_idx)

        # 匹配本组所有红色A框
        for r_idx, r_item in enumerate(red_list):
            if r_idx in used_red_idx:
                continue
            _, _, a_ctr = r_item
            if point_in_mask(a_ctr, mask, (orig_h, orig_w)):
                raw_group_red.append(r_item)
                used_red_idx.add(r_idx)

        # ========== 分组过滤核心逻辑 ==========
        # 规则1：mask内无B点(黄色框)，直接丢弃该分组，不输出AC角度
        if len(raw_group_yellow) == 0:
            continue
        # 规则2：mask内无A点(红色框)，丢弃分组
        if len(raw_group_red) == 0:
            continue

        # 当前分组仅有1个A点
        point_A = raw_group_red[0][2]
        mask_obb_pts, center_O = get_mask_rotated_min_bbox(mask, orig_h, orig_w)

        candidate_B_info = []
        for y_idx, y_item in raw_group_yellow:
            b_box, b_cls, b_conf, point_B = y_item
            # 向量 OA, OB
            vec_OA = point_A - center_O
            vec_OB = point_B - center_O
            # 计算∠AOB 弧度夹角
            ang_AOB = calculate_angle_between_vectors(vec_OA, vec_OB)
            # 记录与90度差值，差值越小越接近直角
            diff_to_right = abs(ang_AOB - RIGHT_ANGLE_RAD)
            candidate_B_info.append((diff_to_right, y_item))

        # 挑选差值最小：∠AOB最接近90°的唯一B黄色框
        candidate_B_info.sort(key=lambda x: x[0])
        best_yellow_item = candidate_B_info[0][1]
        best_y_cls = best_yellow_item[1]
        # 获取最优黄色框长边角度作为对齐参考
        best_yellow_long_angle, _, _ = get_obb_main_angle(best_yellow_item[0])

        # 存入最终有效分组，附带黄色类别
        groups.append({
            "single_red": raw_group_red[0],    # 本组唯一A红色框
            "best_yellow": best_yellow_item,   # 筛选后最优B黄色框
            "y_cls": best_y_cls,
            "ref_angle": best_yellow_long_angle,
            "mask": mask
        })

    # ========== 分情况绘制 ==========
    # 情况1：存在经过筛选的有效分组
    if len(groups) > 0:
        for g_idx, g in enumerate(groups):
            ref_angle = g["ref_angle"]
            red_item = g["single_red"]
            yellow_item = g["best_yellow"]
            y_category = g["y_cls"]
            mask = g["mask"]

            # 绘制筛选保留的最优黄色OBB
            y_box, y_cls, _, _ = yellow_item
            color_y = COLOR_MAP[y_cls]
            draw_obb_box(ax, y_box, color_y, linewidth=2)

            # 获取掩码倾斜最小外接矩形 + 中心点O
            mask_obb_pts, center_O = get_mask_rotated_min_bbox(mask, orig_h, orig_w)
            if mask_obb_pts is not None and center_O is not None:
                # 蓝色实线绘制倾斜最小外接矩形
                draw_obb_box(ax, mask_obb_pts.reshape(-1), COLOR_MASK_BOX, linewidth=2, linestyle="-")
                # 绘制中心点O圆点
                ax.scatter(center_O[0], center_O[1], color=COLOR_O, s=60, zorder=10)

            # 红色框流程：参考最优黄色框角度对齐 → 长边拉长2倍
            r_box, r_conf, _ = red_item
            # 第一步：角度对齐
            pts4_raw = np.array(r_box).reshape(4, 2)
            aligned_pts4 = rotate_polygon_optimal(pts4_raw, ref_angle)
            aligned_8 = aligned_pts4.reshape(-1)

            # 第二步：长边延长2倍
            scaled_4pts = scale_obb_long_edge(aligned_8, LONG_EDGE_SCALE)
            scaled_8 = scaled_4pts.reshape(-1)
            scaled_center_A = get_obb_center(scaled_8)

            # 绘制缩放后红色OBB
            draw_obb_box(ax, scaled_8, COLOR_MAP[0], linewidth=2)

            # 标记中心点A圆点
            ax.scatter(scaled_center_A[0], scaled_center_A[1], color="red", s=60, zorder=10)

            if center_O is not None:
                # 绘制OA参考连线：方向 O -> A
                ax.plot([center_O[0], scaled_center_A[0]],
                        [center_O[1], scaled_center_A[1]],
                        color=COLOR_OA, lw=2, linestyle="-.")
                # 参考向量：O指向A
                vec_OA = scaled_center_A - center_O

                # 获取四边全部垂足
                foot_points = get_perpendicular_foot_points(scaled_4pts, scaled_center_A)
                candidate_list = []
                for idx, (foot_C, _) in enumerate(foot_points):
                    # AC向量：A指向C
                    vec_AC = foot_C - scaled_center_A
                    # 归一化向量，计算点积；点积越大，两向量方向越一致
                    norm_oa = vec_OA / np.linalg.norm(vec_OA)
                    norm_ac = vec_AC / np.linalg.norm(vec_AC)
                    dot_product = np.dot(norm_oa, norm_ac)
                    candidate_list.append((dot_product, foot_C, vec_AC))

                # 点积降序排序，取最大值：和OA方向最一致的射线
                candidate_list.sort(key=lambda x: x[0], reverse=True)
                best_dot, best_C, best_vec = candidate_list[0]

                # 计算AC射线弧度角度、角度制
                ac_rad = np.arctan2(best_vec[1], best_vec[0])
                ac_deg = np.rad2deg(ac_rad)

                # ---------------- 新增方位判断逻辑 ----------------
                # 左右判定：A.x > O.x → 右，否则左
                if scaled_center_A[0] > center_O[0]:
                    lr_dir = "右"
                else:
                    lr_dir = "左"
                # 大小判定：1=big大上，2=small小上
                if y_category == 1:
                    size_word = "大上"
                else:
                    size_word = "小上"
                pos_result = size_word + lr_dir
                # ----------------------------------------------------

                # 打印角度+方位结果
                print(f"分组{g_idx} AC射线角度：弧度 {ac_rad:.4f} rad，角度 {ac_deg:.2f} °  方位：{pos_result}")

                # 绘制垂足C圆点
                ax.scatter(best_C[0], best_C[1], color=COLOR_AC, s=50, zorder=10)

                # 向外延伸绘制射线AC + 箭头
                ray_end = scaled_center_A + best_vec / np.linalg.norm(best_vec) * RAY_EXTEND_LENGTH
                ax.annotate(
                    "",
                    xy=(ray_end[0], ray_end[1]),
                    xytext=(scaled_center_A[0], scaled_center_A[1]),
                    arrowprops=dict(arrowstyle="->", color=COLOR_AC, lw=3),
                    zorder=9
                )

    # 情况2：无有效分组，兜底渲染所有框（不计算AC角度）
    else:
        print("无满足条件的有效分组，仅渲染所有检测框")
        # 绘制所有黄色框
        for box, cls, conf, _ in yellow_list:
            color = COLOR_MAP[cls]
            draw_obb_box(ax, box, color, linewidth=2)

        # 红色框直接长边放大2倍
        for box, conf, _ in red_list:
            scaled_4pts = scale_obb_long_edge(box, LONG_EDGE_SCALE)
            scaled_8 = scaled_4pts.reshape(-1)
            scaled_center_A = get_obb_center(scaled_8)
            draw_obb_box(ax, scaled_8, COLOR_MAP[0], linewidth=2)
            ax.scatter(scaled_center_A[0], scaled_center_A[1], color="red", s=60)

    # 关闭图例、坐标轴
    ax.axis("off")
    plt.tight_layout()
    plt.show()

# ==================== 运行入口 ====================
if __name__ == "__main__":
    image_file = "2.jpg"
    visualize_group_aligned(image_file)