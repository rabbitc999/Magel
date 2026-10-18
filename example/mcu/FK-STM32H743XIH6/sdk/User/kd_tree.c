#include "kd_tree.h"
#include "fast_math.h"
#include "algorithm.h"
#include <string.h>

LidarPosition_T g_LidarPosition;

/**
 * @brief  获取下一个可用节点的索引
 * @retval 下一个可用节点的索引
 */
int32_t LidarPosition_GetNextAvailableNodeIndex(void)
{
    return g_LidarPosition.availableNodeIndex++;  // 分配后指针+1
}

/**
 * @brief  重置 KD-Tree 节点状态
 */
void LidarPosition_ResetNodes(void)
{
    Memory_Set(g_LidarPosition.NodeKDNodes, 0, sizeof(LidarNodeKD_T) * LIDAR_POSITION_MAX_NODES);
    Memory_Set(g_LidarPosition.LastNodeKDNodes, 0, sizeof(LidarNodeKD_T) * LIDAR_POSITION_MAX_NODES);
    g_LidarPosition.availableNodeIndex = 0;
}

/**
 * @brief  按x坐标升序排序的比较函数
 * @param  a 指向第一个点的指针
 * @param  b 指向第二个点的指针
 * @retval 两点的x坐标差值
 */
static int SortByX(const void *a, const void *b)
{
    return ((LidarPoint2D_T *)a)->x - ((LidarPoint2D_T *)b)->x;
}

/**
 * @brief  按y坐标升序排序的比较函数
 * @param  a 指向第一个点的指针
 * @param  b 指向第二个点的指针
 * @retval 两点的y坐标差值
 */
static int SortByY(const void *a, const void *b)
{
    return ((LidarPoint2D_T *)a)->y - ((LidarPoint2D_T *)b)->y;
}

/**
 * @brief  在点云数组中查找中位数点
 * @param  points 点云数组
 * @param  split 分割维度（0表示x坐标，1表示y坐标）
 * @param  start 起始索引
 * @param  end   结束索引
 * @retval 指向中位数点的指针
 */
LidarPoint2D_T *LidarPosition_FindMedian(LidarPoint2D_T points[], uint8_t split, int32_t start, int32_t end)
{
    int32_t n = end - start + 1;
    LidarPoint2D_T *address = 0;

    // 使用快速选择算法代替排序
    if (split == 0)
    {
        // 按x坐标找中位数
        qsort(&points[start], n, sizeof(LidarPoint2D_T), SortByX);
        address = &points[start + n / 2];
    }
    else
    {
        // 按y坐标找中位数
        qsort(&points[start], n, sizeof(LidarPoint2D_T), SortByY);
        address = &points[start + n / 2];
    }

    return address;
}

/**
 * @brief  递归构建 KD-Tree
 * @param  points 点云数组
 * @param  depth  当前深度
 * @param  start  起始索引
 * @param  end    结束索引
 * @retval 指向当前节点的指针
 */
LidarNodeKD_T *LidarPosition_BuildKDTree(LidarPoint2D_T points[], int32_t depth, int32_t start, int32_t end)
{
    if (start <= end)
    {
        int32_t nodeIndex = LidarPosition_GetNextAvailableNodeIndex();  // 获取下一个可用节点的索引
        LidarNodeKD_T *node = &g_LidarPosition.NodeKDNodes[nodeIndex];
        int32_t split = depth % 2;
        node->point = *LidarPosition_FindMedian(points, split, start, end);

        int32_t temp = end;
        int32_t n = end - start + 1;

        // 左子树.
        if (n % 2 != 0)
        {
            end = start + (n - 1) / 2 - 1;
        }
        else
        {
            end = start + n / 2 - 1;
        }
        node->left = LidarPosition_BuildKDTree(points, depth + 1, start, end);

        // 右子树.
        if (n % 2 != 0)
        {
            start = start + (n - 1) / 2 + 1;
        }
        else
        {
            start = start + n / 2 + 1;
        }
        end = temp;
        node->right = LidarPosition_BuildKDTree(points, depth + 1, start, end);

        return node;
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief  在 KD-Tree 中查找查询点到最近节点的最短距离
 * @param  node  当前节点
 * @param  point 查询点
 * @param  depth 当前深度
 * @param  d_min 输出：[0]最短距离，[1]x偏移，[2]y偏移
 */
void LidarPosition_FindMinDistance(LidarNodeKD_T *node, LidarPoint2D_T point, int32_t depth, float d_min[])
{
    while (node != NULL)
    {
        float d_x = point.x - node->point.x;
        float d_y = point.y - node->point.y;
        float d = Math_Sqrt(d_x * d_x + d_y + d_y);  // 计算欧式距离
        if (d <= d_min[0])
        {
            d_min[0] = d;
            d_min[1] = d_x;
            d_min[2] = d_y;
        }
        if (depth % 2 != 0)
        {
            if (point.x <= node->point.x)
            {
                node = node->left;
            }
            else
            {
                node = node->right;
            }
            depth += 1;
        }
        else
        {
            if (point.y <= node->point.y)
            {
                node = node->left;
            }
            else
            {
                node = node->right;
            }
            depth += 1;
        }
    }
}

/**
 * @brief  重置点云数据
 */
static void LidarPosition_PointsReset(void)
{
    LidarPosition_ResetNodes();
    g_LidarPosition.RootNode = NULL;
    Memory_Set(g_LidarPosition.NodePoints, 0, sizeof(LidarPoint2D_T) * LIDAR_POSITION_MAX_NODES);
    Memory_Set(g_LidarPosition.NodePointsCopy, 0, sizeof(LidarPoint2D_T) * LIDAR_POSITION_MAX_NODES);
    Memory_Set(g_LidarPosition.TempPoints, 0, sizeof(LidarPoint2D_T) * LIDAR_POSITION_MAX_NODES);
}

/**
 * @brief  从旋转矩阵提取旋转角度
 * @param  rotation 旋转矩阵
 * @retval 旋转角度（弧度）
 */
static float LidarPosition_ExtractRotationAngle(const Matrix2x2_T *rotation)
{
    return atan2(rotation->m21, rotation->m11);
}

/**
 * @brief  在指定角度范围内搜索最佳旋转匹配角度
 * @param  targetPoints 目标点云
 * @param  pointCount   点数
 * @param  startAngle   起始角度（度）
 * @param  endAngle     结束角度（度）
 * @param  stepSize     搜索步长（度）
 * @retval 最佳旋转角度
 */
float SearchForBestRotationInRange(LidarPoint2D_T *targetPoints, int32_t pointCount, float startAngle, float endAngle, float stepSize)
{
    float bestAngle = 0.0f;
    float bestError = 3e6;

    // 在搜索范围内遍历所有角度
    for (float angle = startAngle; angle <= endAngle; angle += stepSize)
    {
        float currentError = 0.0f;
        float cosA = Math_Cos(angle);
        float sinA = Math_Sin(angle);

        // 计算当前角度下的匹配误差，且进行快速搜索
        for (int i = 0; i < pointCount; i += 5)
        {
            // 旋转目标点云
            float rotatedX = targetPoints[i].x * cosA - targetPoints[i].y * sinA;
            float rotatedY = targetPoints[i].x * sinA + targetPoints[i].y * cosA;

            float d_min[3] = { 3e6, 0, 0 };                                                                    // 初始化
            LidarPosition_FindMinDistance(g_LidarPosition.RootNode, g_LidarPosition.NodePoints[i], 0, d_min);  // 每个模板点寻找最近距离
            currentError += d_min[0];
        }

        // 更新最佳角度
        if (currentError < bestError)
        {
            bestError = currentError;
            bestAngle = angle;
        }
    }

    return bestAngle;
}

/**
 * @brief  分层搜索点云最佳旋转角度，先粗搜再细搜
 * @param  targetPoints 目标点云
 * @param  pointCount   点数
 * @retval 最佳旋转角度
 */
float HierarchicalRotationSearch(LidarPoint2D_T *targetPoints, int32_t pointCount)
{
    float coarseAngle = SearchForBestRotationInRange(targetPoints, pointCount, -30.0f, 30.0f, 5.0f);
    float fineAngle = SearchForBestRotationInRange(targetPoints, pointCount, coarseAngle - 5.0f, coarseAngle + 5.0f, 0.5f);

    return fineAngle;
}

/**
 * @brief  基于KD-Tree的激光雷达点云配准定位
 * @param  PointList  激光雷达点云数据列表
 * @param  currentYaw 当前偏航角
 * @param  result     输出结果数组：[0]位置x, [1]位置y, [2]旋转z, [3]迭代次数
 * @param  startFlag  启动标志，0表示初始化复位
 */
void LidarPosition_Positioning(LidarDataList_T *PointList, float currentYaw, int32_t *result, uint8_t startFlag)
{
    static int32_t counter = 0;                   // 计数器
    float x_bias = 0;                             // x坐标系累计偏移
    float y_bias = 0;                             // y坐标系累计偏移
    float z_bias = 0;                             // z坐标系旋转
    float max_error = 10000;                      // 偏移最大值
    static float XError = 0;                      // x坐标系偏移
    static float YError = 0;                      // y坐标系偏移
    static float ZError = 0;                      // z坐标系旋转
    static int32_t positionX = 0, positionY = 0;  // 定位成功的坐标
    static float positionZ = 0;                   // 定位成功的坐标
    static float positionX1Mid = 0;               // 模板点云重心x
    static float positionY1Mid = 0;               // 模板点云重心y
    static float positionX2Mid = 0;               // 目标点云重心x
    static float positionY2Mid = 0;               // 目标点云重心y
    float ZErrorMax = 60, ZErrorMin = -60;        // z坐标系旋转区间
    int32_t iter = 0;                             // 迭代次数
    int32_t real_cnt = 0;                         // 实际点数
    LidarData_T *PointCloud = PointList->buffer;
    uint32_t length = PointList->count;

    // 未初始化
    if (startFlag == 0)
    {
        counter = 0;
        XError = YError = ZError = 0;
        positionX1Mid = positionY1Mid = positionX2Mid = positionY2Mid = 0;
        positionX = positionY = positionZ = 0;
        result[0] = 0;
        result[1] = 0;
        result[2] = 0;
        result[3] = 0;
        max_error = 10000;
        x_bias = y_bias = z_bias = 0;
        LidarPosition_PointsReset();
        return;
    }

    // 过滤距离太小(5cm)和太远(超过50m)的扫描点，认为是无效点
    for (int32_t i = 0; i < length; i++)
    {
        if (PointCloud[i].distance > 50)
        {
            float theta = PointCloud[i].angle - currentYaw;
            g_LidarPosition.NodePoints[real_cnt].x = PointCloud[i].distance * Math_Cos(theta);
            g_LidarPosition.NodePoints[real_cnt].y = PointCloud[i].distance * Math_Sin(theta);
            real_cnt += 1;
        }
    }

    // 为了确保数据的稳定性，前2帧作为初始化，第3帧为起始模板，第4帧开始匹配
    if (counter < 3)
    {
        positionX1Mid = 0;
        positionY1Mid = 0;
        for (int32_t j = 0; j < real_cnt; j++)
        {
            positionX1Mid += g_LidarPosition.NodePoints[j].x;
            positionY1Mid += g_LidarPosition.NodePoints[j].y;
        }
        positionX1Mid /= real_cnt;  // 模板点云重心x
        positionY1Mid /= real_cnt;  // 模板点云重心y

        // 去除重心偏移
        for (int32_t j = 0; j < real_cnt; j++)
        {
            g_LidarPosition.NodePoints[j].x -= positionX1Mid;
            g_LidarPosition.NodePoints[j].y -= positionY1Mid;
        }
        LidarPosition_ResetNodes();                                                                            // 重置内存
        g_LidarPosition.RootNode = LidarPosition_BuildKDTree(g_LidarPosition.NodePoints, 0, 0, real_cnt - 1);  // 构建KD-Tree形式存储
    }
    else
    {
        positionX2Mid = 0;
        positionY2Mid = 0;
        for (int32_t j = 0; j < real_cnt; j++)
        {
            positionX2Mid += g_LidarPosition.NodePoints[j].x;
            positionY2Mid += g_LidarPosition.NodePoints[j].y;
        }
        positionX2Mid /= real_cnt;  // 目标点云重心x
        positionY2Mid /= real_cnt;  // 目标点云重心y

        // 去除重心偏移
        for (int32_t j = 0; j < real_cnt; j++)
        {
            g_LidarPosition.NodePoints[j].x -= positionX2Mid;
            g_LidarPosition.NodePoints[j].y -= positionY2Mid;
        }

        // 分层搜索点云旋转量
        z_bias = HierarchicalRotationSearch(g_LidarPosition.NodePoints, real_cnt);

        // 应用旋转变换到当前点云
        float cosZ = Math_Cos(z_bias);
        float sinZ = Math_Sin(z_bias);

        for (int32_t j = 0; j < real_cnt; j++)
        {
            float newX = g_LidarPosition.NodePoints[j].x * cosZ - g_LidarPosition.NodePoints[j].y * sinZ;
            float newY = g_LidarPosition.NodePoints[j].x * sinZ + g_LidarPosition.NodePoints[j].y * cosZ;
            g_LidarPosition.NodePoints[j].x = newX;
            g_LidarPosition.NodePoints[j].y = newY;
        }

        // 备份目标点云
        for (int32_t j = 0; j < real_cnt; j++)
        {
            g_LidarPosition.NodePointsCopy[j].x = g_LidarPosition.NodePoints[j].x;
            g_LidarPosition.NodePointsCopy[j].y = g_LidarPosition.NodePoints[j].y;
        }

        // 首先使用中位数方法
        x_bias = positionX1Mid - positionX2Mid;  // 初始x坐标系累计偏移
        y_bias = positionY1Mid - positionY2Mid;  // 初始y坐标系累计偏移

        // 要求偏移小于阈值
        while (max_error > 0.1f)
        {
            iter += 1;   // 累计迭代次数
            XError = 0;  // x坐标系偏移清零
            YError = 0;  // y坐标系偏移清零
            for (int32_t i = 0; i < real_cnt; i++)
            {
                float d_min[3] = { 3e6, 0, 0 };                                                                    // 初始化
                LidarPosition_FindMinDistance(g_LidarPosition.RootNode, g_LidarPosition.NodePoints[i], 0, d_min);  // 每个模板点寻找最近距离
                g_LidarPosition.TempPoints[i].x = d_min[1];                                                        // 存储x偏移
                g_LidarPosition.TempPoints[i].y = d_min[2];                                                        // 存储y偏移
            }

            // 使用快速选择算法获取中位数（改进方法）
            qsort(g_LidarPosition.TempPoints, real_cnt, sizeof(LidarPoint2D_T), SortByX);
            LidarPoint2D_T *x_median = &g_LidarPosition.TempPoints[real_cnt / 2];
            qsort(g_LidarPosition.TempPoints, real_cnt, sizeof(LidarPoint2D_T), SortByY);
            LidarPoint2D_T *y_median = &g_LidarPosition.TempPoints[real_cnt / 2];

            if (x_median != NULL)
                XError = x_median->x;
            if (y_median != NULL)
                YError = y_median->y;

            max_error = fabs(XError) + fabs(YError);  // 累计偏移误差为偏移的绝对值之和
            for (int32_t j = 0; j < real_cnt; j++)
            {
                g_LidarPosition.NodePoints[j].x -= XError;
                g_LidarPosition.NodePoints[j].y -= YError;
            }
            x_bias -= XError;
            y_bias -= YError;

            // 设置最大迭代为50次，防止循环无法收敛
            if (iter > 50)
            {
                x_bias = y_bias = z_bias = 0;
                break;
            }
        }
    }

    if (fabs(x_bias) > 50 || fabs(y_bias) > 50 || fabs(z_bias) > 20)  // 当坐标系累计偏移达到阈值时，记录当前位置，重置模板
    {
        positionX1Mid = positionX2Mid;
        positionY1Mid = positionY2Mid;
        positionX += (int32_t)x_bias;  // 当前位置为新的原点
        positionY += (int32_t)y_bias;  // 当前位置为新的原点
        positionZ += z_bias;           // 当前位置为新的原点
        x_bias = 0;
        y_bias = 0;
        z_bias = 0;
        LidarPosition_ResetNodes();                                                                                // 初始化状态内存
        g_LidarPosition.RootNode = LidarPosition_BuildKDTree(g_LidarPosition.NodePointsCopy, 0, 0, real_cnt - 1);  // 重新构建
    }

    if (counter < 5) counter++;

    // 输出结果
    result[0] = positionX + x_bias;   // 位置坐标x
    result[1] = positionY + y_bias;   // 位置坐标y
    result[2] = currentYaw + z_bias;  // 旋转坐标z
    result[3] = iter;                 // 迭代次数
}

/**
 * @brief  先序遍历 KD-Tree 并输出点云信息到缓冲区
 * @param  outputBuffer 输出缓冲区
 * @param  length       缓冲区长度
 */
void LidarPosition_PreorderKDTree(char *outputBuffer, uint32_t length)
{
    if (outputBuffer == NULL || length == 0) return;

    LidarNodeKD_T *stack[LIDAR_POSITION_MAX_NODES];
    int32_t top = 0;
    uint32_t offset = 0, pointIndex = 0;
    LidarNodeKD_T *node = g_LidarPosition.RootNode;

    if (offset < length)
    {
        offset += snprintf(outputBuffer + offset, length - offset, "先序遍历KD-Tree树：\n");
    }

    if (node != NULL)
    {
        stack[top++] = node;
    }

    while (top > 0)
    {
        LidarNodeKD_T *cur = stack[--top];

        if (offset < length && pointIndex % 10 == 0)
        {
            offset += snprintf(outputBuffer + offset, length - offset, "点云坐标[%d]: (%.2f, %.2f)\n", pointIndex, cur->point.x, cur->point.y);
        }

        // 先将右子节点压栈，再压左子节点，这样出栈时先访问左子节点，实现先序：根、左、右
        if (cur->right != NULL && top < (int32_t)LIDAR_POSITION_MAX_NODES)
        {
            stack[top++] = cur->right;
        }
        if (cur->left != NULL && top < (int32_t)LIDAR_POSITION_MAX_NODES)
        {
            stack[top++] = cur->left;
        }

        pointIndex++;
    }

    if (offset >= length)
    {
        outputBuffer[length - 1] = '\0';
    }
    else
    {
        outputBuffer[offset] = '\0';
    }
}
