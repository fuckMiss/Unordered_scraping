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
OBB_MODEL_PATH = r"D:\视觉项目\runs\obb\train5\weights\best.pt"
SEG_MODEL_PATH = "best_seg.pt"
PERP_SCALE = 2.0       # 垂直AC方向拉伸倍数
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

# ========== 新增：实例邻近过滤参数 ==========
NEAR_DIST_PX = 15               # 原图坐标系下，判定为“过近”的像素距离阈值
NEAR_AREA_RATIO_THRESH = 0.3    # 膨胀后交集面积占原mask面积的比例阈值，超过则判定为大面积邻近，过滤该实例
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
    原始逻辑：长边延长N倍，中心点、角度、短边宽度完全保持不变
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

def scale_obb_along_perp_AC(obb_4pts, center_A, vec_AC, scale_factor):
    """
    新拉伸逻辑：以A为中心，沿垂直于AC的方向双向拉伸OBB
    :param obb_4pts: 对齐角度后的原始四点 (4,2)
    :param center_A: A中心点，拉伸后保持为中心
    :param vec_AC: AC射线方向向量
    :param scale_factor: 拉伸倍数 PERP_SCALE
    :return: 拉伸后四点 (4,2)
    """
    # 1. 计算AC垂直方向单位向量
    ac_norm = vec_AC / np.linalg.norm(vec_AC)
    # 垂直AC方向：(-y, x)
    perp_vec = np.array([-ac_norm[1], ac_norm[0]])

    # 2. 所有点相对于A中心偏移
    pts_shift = obb_4pts - center_A

    # 3. 分解每个点到【垂直AC】与【平行AC】两个分量
    new_pts = []
    for pt in pts_shift:
        # 平行AC分量（不缩放）
        para_comp = np.dot(pt, ac_norm) * ac_norm
        # 垂直AC分量（缩放）
        perp_comp = np.dot(pt, perp_vec) * perp_vec * scale_factor
        # 合并偏移
        new_shift = para_comp + perp_comp
        new_pts.append(center_A + new_shift)
    return np.array(new_pts)

def rotate_polygon_optimal(polygon_pts, target_long_angle):
    """保留函数占位（主流程不再调用，兜底逻辑未用到，保留避免引用报错）"""
    return polygon_pts.copy()

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

    # ✅修复bug：遍历box_pts_mask，不是box_pts_origin
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

# ==================== 新增：实例邻近过滤核心函数 ====================
def filter_near_masks(mask_list, dist_thresh=15, ratio_thresh=0.3):
    """
    过滤掉距离过近且大面积相邻的分割实例
    原理：对每个掩码向外膨胀dist_thresh距离，若两掩码膨胀后交集面积占各自原面积比例均超过阈值，
    说明两者有大面积区域距离小于阈值，判定为堆叠/过近实例，全部过滤
    :param mask_list: list of np.array (H, W)，二值掩码（浮点0~1）
    :param dist_thresh: 掩码坐标系下的距离阈值（像素）
    :param ratio_thresh: 交集面积占原mask面积的比例阈值
    :return: keep_masks: 保留的掩码列表, filtered_idx: 被过滤的索引集合
    """
    n = len(mask_list)
    if n <= 1:
        return mask_list, set()

    # 构造圆形膨胀核，尺寸适配距离阈值
    kernel_size = max(3, int(dist_thresh * 2 + 1))
    # 确保核大小为奇数
    if kernel_size % 2 == 0:
        kernel_size += 1
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (kernel_size, kernel_size))

    # 预计算每个mask的膨胀结果、原面积
    dilated_masks = []
    mask_areas = []
    for mask in mask_list:
        # 浮点掩码转二值uint8
        mask_bin = (mask > 0.5).astype(np.uint8)
        dilated = cv2.dilate(mask_bin, kernel)
        dilated_masks.append(dilated)
        mask_areas.append(np.sum(mask_bin))

    filtered_idx = set()

    # 两两配对判定
    for i in range(n):
        if i in filtered_idx:
            continue
        for j in range(i + 1, n):
            if j in filtered_idx:
                continue
            # 计算膨胀后两个掩码的交集
            intersection = cv2.bitwise_and(dilated_masks[i], dilated_masks[j])
            inter_area = np.sum(intersection)
            if inter_area == 0:
                continue

            # 交集占各自原掩码面积的比例
            ratio_i = inter_area / mask_areas[i]
            ratio_j = inter_area / mask_areas[j]

            # 双方占比均超过阈值 → 大面积邻近，双双过滤
            if ratio_i > ratio_thresh and ratio_j > ratio_thresh:
                filtered_idx.add(i)
                filtered_idx.add(j)

    # 生成保留的掩码列表
    keep_masks = [mask_list[i] for i in range(n) if i not in filtered_idx]
    return keep_masks, filtered_idx
# =================================================================

def visualize_group_aligned(img_path):
    # 先加载分割模型并推理，优先完成实例邻近过滤
    seg_model = YOLO(SEG_MODEL_PATH)
    seg_res = seg_model(img_path)[0]

    fig, ax = plt.subplots(figsize=(12, 9))
    # 用分割结果的原图做显示，保证尺寸一致
    img_bgr = seg_res.orig_img
    img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    orig_h, orig_w = img_rgb.shape[:2]
    ax.imshow(img_rgb)

    # 1. 解析所有分割实例掩码
    seg_masks = []
    if seg_res.masks is not None:
        seg_masks = seg_res.masks.data.cpu().numpy()  # [N, H_mask, W_mask]

    # ========== 执行实例邻近过滤 ==========
    keep_seg_masks = seg_masks
    filtered_idx = set()
    if len(seg_masks) > 1:
        mask_h, mask_w = seg_masks[0].shape
        # 将原图距离阈值转换到掩码坐标系（掩码分辨率与原图不同，需等比缩放）
        scale = mask_w / orig_w
        mask_dist_thresh = NEAR_DIST_PX * scale
        # 执行邻近过滤
        keep_seg_masks, filtered_idx = filter_near_masks(
            seg_masks,
            dist_thresh=mask_dist_thresh,
            ratio_thresh=NEAR_AREA_RATIO_THRESH
        )
        print(f"分割实例总数：{len(seg_masks)}，过滤邻近实例数：{len(filtered_idx)}，保留有效实例数：{len(keep_seg_masks)}")
    # ======================================

    # 无保留实例，直接结束，不执行OBB推理与后续所有逻辑
    if len(keep_seg_masks) == 0:
        print("所有实例均因邻近被过滤，终止后续处理")
        ax.axis("off")
        plt.tight_layout()
        plt.show()
        return

    # 保留实例存在，再执行OBB模型推理
    obb_model = YOLO(OBB_MODEL_PATH)
    obb_res = obb_model(img_path)[0]

    # 2. 解析所有OBB结果
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

    # 3. 按保留的分割实例分组 + B点筛选逻辑
    groups = []
    used_yellow_idx = set()
    used_red_idx = set()

    for mask in keep_seg_masks:
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

        # 存入最终有效分组
        groups.append({
            "single_red": raw_group_red[0],    # 本组唯一A红色框
            "best_yellow": best_yellow_item,   # 筛选后最优B黄色框
            "y_cls": best_y_cls,
            "mask": mask
        })

    # ========== 分情况绘制 ==========
    # 情况1：存在经过筛选的有效分组
    if len(groups) > 0:
        for g_idx, g in enumerate(groups):
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

            # 红色框流程：直接使用原始检测坐标，取消以黄框角度旋转对齐步骤
            r_box_8, r_conf, _ = red_item
            raw_pts4 = np.array(r_box_8).reshape(4, 2)
            raw_center_A = get_obb_center(r_box_8)

            # 沿用原有垂足求解、OA筛选最优AC完整逻辑
            foot_points = get_perpendicular_foot_points(raw_pts4, raw_center_A)
            vec_OA = raw_center_A - center_O
            candidate_list = []
            for idx, (foot_C, _) in enumerate(foot_points):
                vec_AC = foot_C - raw_center_A
                norm_oa = vec_OA / np.linalg.norm(vec_OA)
                norm_ac = vec_AC / np.linalg.norm(vec_AC)
                dot_product = np.dot(norm_oa, norm_ac)
                candidate_list.append((dot_product, foot_C, vec_AC))

            # 点积降序，取和OA方向最一致的AC向量
            candidate_list.sort(key=lambda x: x[0], reverse=True)
            best_dot, best_C, best_vec_AC = candidate_list[0]

            # 沿垂直AC方向拉伸红色OBB，A保持中心不变
            scaled_4pts = scale_obb_along_perp_AC(raw_pts4, raw_center_A, best_vec_AC, PERP_SCALE)
            scaled_8 = scaled_4pts.reshape(-1)
            scaled_center_A = raw_center_A

            # 绘制拉伸后红色OBB
            draw_obb_box(ax, scaled_8, COLOR_MAP[0], linewidth=2)
            # 标记中心点A圆点
            ax.scatter(scaled_center_A[0], scaled_center_A[1], color="red", s=60, zorder=10)

            if center_O is not None:
                # 绘制OA参考连线：方向 O -> A
                ax.plot([center_O[0], scaled_center_A[0]],
                        [center_O[1], scaled_center_A[1]],
                        color=COLOR_OA, lw=2, linestyle="-.")

                # 计算AC射线弧度角度、角度制
                ac_rad = np.arctan2(best_vec_AC[1], best_vec_AC[0])
                ac_deg = np.rad2deg(ac_rad)

                # 方位判断逻辑
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

                # 打印角度+方位结果
                print(f"分组{g_idx} AC射线角度：弧度 {ac_rad:.4f} rad，角度 {ac_deg:.2f} °  方位：{pos_result}")

                # 绘制垂足C圆点
                ax.scatter(best_C[0], best_C[1], color=COLOR_AC, s=50, zorder=10)

                # 向外延伸绘制射线AC + 箭头
                ray_end = scaled_center_A + best_vec_AC / np.linalg.norm(best_vec_AC) * RAY_EXTEND_LENGTH
                ax.annotate(
                    "",
                    xy=(ray_end[0], ray_end[1]),
                    xytext=(scaled_center_A[0], scaled_center_A[1]),
                    arrowprops=dict(arrowstyle="->", color=COLOR_AC, lw=3),
                    zorder=9
                )

    # 情况2：有保留实例但无有效分组，兜底渲染保留实例内的检测框
    else:
        print("无满足条件的有效分组，仅渲染保留实例内的检测框")
        # 绘制匹配到保留掩码的黄色框
        for y_idx, y_item in enumerate(yellow_list):
            if y_idx in used_yellow_idx:
                box, cls, conf, _ = y_item
                color = COLOR_MAP[cls]
                draw_obb_box(ax, box, color, linewidth=2)

        # 绘制匹配到保留掩码的红色框，执行长边放大
        for r_idx, r_item in enumerate(red_list):
            if r_idx in used_red_idx:
                box, conf, _ = r_item
                scaled_4pts = scale_obb_long_edge(box, PERP_SCALE)
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
    image_file = "248.png"
    visualize_group_aligned(image_file)