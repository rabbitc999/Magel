#include "bsp_system.h"
#include "algorithm.h"
#include <string.h>
#include <stdlib.h>

/* 内部辅助函数声明 */
static void HeapSort_SiftDown(void *base, size_t start, size_t end, size_t size, int (*compare)(const void *, const void *));
static void HeapSort_Swap(void *a, void *b, void *temp, size_t size);

/* 静态全局变量 */
static int s_HeapSortSwapCount = 0; /* 交换次数计数器 */

/**
 * @brief  交换两个内存块的内容
 * @param  a 第一个内存块的指针
 * @param  b 第二个内存块的指针
 * @param  temp 临时缓冲区指针
 * @param  size 每个内存块的大小
 */
static void HeapSort_Swap(void *a, void *b, void *temp, size_t size)
{
    Memory_Copy(temp, a, size);
    Memory_Copy(a, b, size);
    Memory_Copy(b, temp, size);
    s_HeapSortSwapCount++;
}

/**
 * @brief  堆排序的下沉操作（调整堆）
 * @param  base 数组基地址
 * @param  start 开始下沉的位置
 * @param  end 结束位置
 * @param  size 元素大小
 * @param  compare 比较函数
 */
static void HeapSort_SiftDown(void *base, size_t start, size_t end, size_t size, int (*compare)(const void *, const void *))
{
    size_t root = start;

    while ((root * 2 + 1) <= end)
    {
        size_t child = root * 2 + 1; /* 左子节点 */
        size_t swapIdx = root;       /* 需要交换的节点索引 */

        char *basePtr = (char *)base;
        void *rootElem = basePtr + root * size;
        void *childElem = basePtr + child * size;

        /* 比较根节点和左子节点 */
        if (compare(childElem, rootElem) > 0)
        {
            swapIdx = child;
        }

        /* 如果有右子节点，比较右子节点 */
        if (child + 1 <= end)
        {
            void *rightChildElem = basePtr + (child + 1) * size;
            void *swapElem = basePtr + swapIdx * size;

            if (compare(rightChildElem, swapElem) > 0)
            {
                swapIdx = child + 1;
            }
        }

        /* 如果根节点已经是最大的，结束 */
        if (swapIdx == root)
        {
            return;
        }
        else
        {
            /* 交换根节点和较大的子节点 */
            void *rootAddr = basePtr + root * size;
            void *swapAddr = basePtr + swapIdx * size;
            char tempBuffer[size];

            HeapSort_Swap(rootAddr, swapAddr, tempBuffer, size);
            root = swapIdx; /* 继续向下调整 */
        }
    }
}

/**
 * @brief  堆排序主函数
 * @param  base 待排序数组的起始地址
 * @param  nmemb 数组中元素的数量
 * @param  size 每个元素的大小（字节）
 * @param  compare 比较函数指针
 */
void HeapSort(void *base, size_t nmemb, size_t size, int (*compare)(const void *, const void *))
{
    if (nmemb <= 1 || base == NULL || compare == NULL)
    {
        return;
    }

    s_HeapSortSwapCount = 0; /* 重置交换计数器 */
    char *basePtr = (char *)base;

    /* 构建最大堆：从最后一个非叶子节点开始向上调整 */
    for (ptrdiff_t start = (nmemb / 2) - 1; start >= 0; start--)
    {
        HeapSort_SiftDown(base, (size_t)start, nmemb - 1, size, compare);
    }

    /* 排序：将堆顶元素（最大值）与末尾元素交换，然后调整堆 */
    for (size_t end = nmemb - 1; end > 0; end--)
    {
        /* 交换堆顶和当前末尾元素 */
        void *firstElem = basePtr;
        void *lastElem = basePtr + end * size;
        char tempBuffer[size];

        HeapSort_Swap(firstElem, lastElem, tempBuffer, size);

        /* 调整堆，排除已排序的末尾元素 */
        HeapSort_SiftDown(base, 0, end - 1, size, compare);
    }
}

/**
 * @brief  交换两个内存块的内容
 * @param  a 指向第一个内存块的起始地址
 * @param  b 指向第二个内存块的起始地址
 * @param  size 内存块的字节大小
 * @note   要求a和b指向的内存区域不重叠且有效
 */
static void swapElements(char *a, char *b, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

/**
 * @brief  对子数组[start, end)执行三路分区（Dijkstra三分法），基于extract提取的数值
 * @param  arr 指向整个数组的字节指针（char*类型便于偏移计算）
 * @param  start 分区起始索引（包含）
 * @param  end 分区结束索引（不包含）
 * @param  size 每个元素的字节大小
 * @param  extract 用户提供的数值提取回调函数
 * @param  out_lowEnd 输出参数，等于pivot区间的起始索引
 * @param  out_highStart 输出参数，等于pivot区间的结束索引
 * @note   分区后数组结构为：[start, lowEnd)小于pivot、[lowEnd, highStart)等于pivot、
 *         [highStart, end)大于pivot；pivot采用三数取中法选取
 */
static void partition3WayByExtract(char *arr, size_t start, size_t end, size_t size, float (*extract)(const void *), size_t *out_lowEnd, size_t *out_highStart)
{
    // 三数取中法：选择首、中、尾三个元素的中位数
    size_t mid = start + (end - start) / 2;

    // 获取三个位置的值
    float val1 = extract(&arr[start * size]);
    float val2 = extract(&arr[mid * size]);
    float val3 = extract(&arr[(end - 1) * size]);

    size_t pivotIndex = start;

    // 找出中位数的索引
    if ((val1 <= val2 && val2 <= val3) || (val3 <= val2 && val2 <= val1))
    {
        pivotIndex = mid;
    }
    else if ((val2 <= val1 && val1 <= val3) || (val3 <= val1 && val1 <= val2))
    {
        pivotIndex = start;  // 已经是start
    }
    else
    {
        pivotIndex = end - 1;
    }

    // 将pivot换到start位置
    if (pivotIndex != start)
    {
        swapElements(&arr[start * size], &arr[pivotIndex * size], size);
    }

    float pivotVal = extract(&arr[start * size]);
    size_t lt = start;
    size_t gt = end;
    size_t i = start + 1;

    while (i < gt)
    {
        float val = extract(&arr[i * size]);
        if (val < pivotVal)
        {
            swapElements(&arr[i * size], &arr[lt * size], size);
            ++lt;
            ++i;
        }
        else if (val > pivotVal)
        {
            --gt;
            swapElements(&arr[i * size], &arr[gt * size], size);
        }
        else
        {
            ++i;
        }
    }

    *out_lowEnd = lt;
    *out_highStart = gt;
}

/**
 * @brief  递归实现快速选择第k小元素（k为绝对索引）
 * @param  arr 数组字节指针
 * @param  start 当前处理区间的起始索引（包含）
 * @param  end 当前处理区间的结束索引（不包含）
 * @param  size 元素字节大小
 * @param  extract 数值提取回调
 * @param  k 目标序号（全局索引，0 <= k < 总元素数）
 * @retval 提取后的float值
 * @note   该函数假设输入合法，由上层做参数校验
 */
static float quickSelectKthRecursive(char *arr, size_t start, size_t end, size_t size, float (*extract)(const void *), size_t k)
{
    if (start + 1 == end)
    {
        return extract(&arr[start * size]);
    }

    size_t lowEnd, highStart;
    partition3WayByExtract(arr, start, end, size, extract, &lowEnd, &highStart);

    if (k < lowEnd)
    {
        return quickSelectKthRecursive(arr, start, lowEnd, size, extract, k);
    }
    else if (k < highStart)
    {
        return extract(&arr[lowEnd * size]);
    }
    else
    {
        return quickSelectKthRecursive(arr, highStart, end, size, extract, k);
    }
}

/**
 * @brief  使用extractor回调，原地查找数组中第k小的元素（k从0开始），平均O(n)时间
 * @param  base 指向数组起始地址（非NULL）
 * @param  count 元素总数（必须大于0）
 * @param  size 每个元素的字节大小（如sizeof(struct MyStruct)）
 * @param  extract 回调函数，用于从const void*提取可比较的float值
 * @param  k 目标序号，0 <= k < count
 * @retval 第k小元素对应的提取值；参数非法时返回0.0
 * @note   此函数会修改原数组的元素顺序（in-place）；extractor必须对所有元素
 *         返回语义一致的数值（如始终返回.score成员）
 */
float QuickSelect_SelectKthByExtractor(void *base, size_t count, size_t size, float (*extract)(const void *), size_t k)
{
    if (base == NULL || extract == NULL || count == 0 || size == 0 || k >= count)
    {
        return 0.0;
    }

    char *arr = (char *)base;
    return quickSelectKthRecursive(arr, 0, count, size, extract, k);
}

/**
 * @brief  通过extractor回调计算数组中位数（奇数取中间值，偶数取中间两元素的平均值）
 * @param  base 指向数组起始地址
 * @param  count 元素总数（大于0）
 * @param  size 每个元素的字节大小
 * @param  extract 数值提取回调函数
 * @retval 中位数值；参数非法时返回0.0
 * @note   内部调用QuickSelect_SelectKthByExtractor一次或两次，同样会修改原数组顺序，
 *         适用于int、float等可转为float的数值类型
 */
float QuickSelect_GetMedianByExtractor(void *base, size_t count, size_t size, float (*extract)(const void *))
{
    if (base == NULL || extract == NULL || count == 0 || size == 0)
    {
        return 0.0;
    }

    if (count % 2 == 1)
    {
        return QuickSelect_SelectKthByExtractor(base, count, size, extract, count / 2);
    }
    else
    {
        float left = QuickSelect_SelectKthByExtractor(base, count, size, extract, count / 2 - 1);
        float right = QuickSelect_SelectKthByExtractor(base, count, size, extract, count / 2);
        return (left + right) / 2.0;
    }
}

/**
 * @brief  使用extractor回调，原地选择第k小的元素，并返回其地址
 * @param  base 数组起始地址
 * @param  count 元素个数（大于0）
 * @param  size 每个元素字节大小
 * @param  extract 提取用于比较的float值的回调函数
 * @param  k 目标序号（0 <= k < count）
 * @retval 指向第k小元素的指针；参数非法时返回NULL
 * @note   数组会被部分重排，使第k小的元素位于base[k]，不分配额外内存
 */
void *QuickSelect_GetKthElementByExtractor(void *base, size_t count, size_t size, float (*extract)(const void *), size_t k)
{
    if (base == NULL || extract == NULL || count == 0 || size == 0 || k >= count)
    {
        return NULL;
    }

    char *arr = (char *)base;
    /* 执行快速选择，原地重排 */
    quickSelectKthRecursive(arr, 0, count, size, extract, k);
    /* 现在 arr[k] 就是第 k 小的元素 */
    return &arr[k * size];
}

/**
 * @brief  计算两个整数的最大公约数（欧几里得算法）
 * @param  a 第一个整数
 * @param  b 第二个整数
 * @retval 两个数的GCD
 */
uint16_t GCD(uint16_t a, uint16_t b)
{
    // 辗转相除法
    while (b != 0)
    {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

/**
 * @brief  计算数组中所有元素的最大公约数
 * @param  arr 输入的整数数组
 * @param  arrLength 数组长度
 * @retval 数组的GCD；若数组长度为0，返回1
 */
uint16_t GCD_Array(uint16_t arr[], uint16_t arrLength)
{
    // 若长度为0，则返回1作为GCD数值
    if (arrLength == 0 || arr == NULL) { return 1; }

    // 初始化当前GCD为数组第一个元素
    int currentGcd = arr[0];

    // 迭代计算数组所有元素的GCD
    for (int i = 1; i < arrLength; i++)
    {
        currentGcd = GCD(currentGcd, arr[i]);

        // 优化：GCD为1时可直接返回（1是最小正整数公约数）
        if (currentGcd == 1)
        {
            return 1;
        }
    }

    return currentGcd;
}

/**
 * @brief  计算矩阵的Frobenius范数
 * @param  matrix 输入矩阵指针
 * @retval 矩阵的Frobenius范数
 * @note   用于收敛性判断
 */
static float _CalculateFrobeniusNorm(const Matrix2x2_T *matrix)
{
    if (matrix == NULL)
    {
        return 0.0;
    }

    float normSquared = matrix->m11 * matrix->m11 + matrix->m12 * matrix->m12 + matrix->m21 * matrix->m21 + matrix->m22 * matrix->m22;

    return sqrt(normSquared);
}

/**
 * @brief  计算2x2矩阵的Jacobi旋转参数
 * @param  matrix 输入矩阵指针
 * @param  cosAngle 余弦值输出指针
 * @param  sinAngle 正弦值输出指针
 * @param  pivot 旋转轴 (0: (1,2), 1: (2,1))
 * @note   用于计算Givens旋转参数
 */
static void _JacobiRotation2x2(Matrix2x2_T *matrix, float *cosAngle, float *sinAngle, uint8_t pivot)
{
    if ((matrix == NULL) || (cosAngle == NULL) || (sinAngle == NULL))
    {
        return;
    }

    float a = 0.0;
    float b = 0.0;

    if (pivot == 0)
    {
        /* 针对(1,2)位置的旋转 */
        a = matrix->m11 - matrix->m22;
        b = matrix->m12 + matrix->m21;
    }
    else
    {
        /* 针对(2,1)位置的旋转 */
        a = matrix->m22 - matrix->m11;
        b = matrix->m12 + matrix->m21;
    }

    float denominator = sqrt(a * a + b * b);

    if (denominator < MATRIX_SVD_EPSILON)
    {
        *cosAngle = 1.0;
        *sinAngle = 0.0;
    }
    else
    {
        *cosAngle = a / denominator;
        *sinAngle = b / denominator;
    }
}

/**
 * @brief  应用Givens旋转到矩阵
 * @param  matrix 要旋转的矩阵指针
 * @param  cosAngle 旋转余弦值
 * @param  sinAngle 旋转正弦值
 * @param  pivot 旋转轴
 * @param  applyLeft 应用左旋转(1)还是右旋转(0)
 * @note   左旋转: U^T * A, 右旋转: A * V
 */
static void _ApplyRotation(Matrix2x2_T *matrix, float cosAngle, float sinAngle, uint8_t pivot, uint8_t applyLeft)
{
    if (matrix == NULL)
    {
        return;
    }

    Matrix2x2_T tempMatrix = *matrix;

    if (applyLeft != 0)
    {
        /* 左旋转: U^T * A */
        if (pivot == 0)
        {
            matrix->m11 = cosAngle * tempMatrix.m11 + sinAngle * tempMatrix.m21;
            matrix->m12 = cosAngle * tempMatrix.m12 + sinAngle * tempMatrix.m22;
            matrix->m21 = -sinAngle * tempMatrix.m11 + cosAngle * tempMatrix.m21;
            matrix->m22 = -sinAngle * tempMatrix.m12 + cosAngle * tempMatrix.m22;
        }
        else
        {
            matrix->m11 = cosAngle * tempMatrix.m11 + sinAngle * tempMatrix.m12;
            matrix->m21 = cosAngle * tempMatrix.m21 + sinAngle * tempMatrix.m22;
            matrix->m12 = -sinAngle * tempMatrix.m11 + cosAngle * tempMatrix.m12;
            matrix->m22 = -sinAngle * tempMatrix.m21 + cosAngle * tempMatrix.m22;
        }
    }
    else
    {
        /* 右旋转: A * V */
        if (pivot == 0)
        {
            matrix->m11 = tempMatrix.m11 * cosAngle + tempMatrix.m12 * sinAngle;
            matrix->m12 = -tempMatrix.m11 * sinAngle + tempMatrix.m12 * cosAngle;
            matrix->m21 = tempMatrix.m21 * cosAngle + tempMatrix.m22 * sinAngle;
            matrix->m22 = -tempMatrix.m21 * sinAngle + tempMatrix.m22 * cosAngle;
        }
        else
        {
            matrix->m11 = tempMatrix.m11 * cosAngle + tempMatrix.m12 * (-sinAngle);
            matrix->m12 = tempMatrix.m11 * sinAngle + tempMatrix.m12 * cosAngle;
            matrix->m21 = tempMatrix.m21 * cosAngle + tempMatrix.m22 * (-sinAngle);
            matrix->m22 = tempMatrix.m21 * sinAngle + tempMatrix.m22 * cosAngle;
        }
    }
}

/**
 * @brief  检查SVD迭代是否收敛
 * @param  matrix 当前迭代矩阵
 * @param  tolerance 收敛容差
 * @retval 1-收敛, 0-未收敛
 * @note   检查非对角线元素是否足够小
 */
static int8_t _CheckConvergence(const Matrix2x2_T *matrix, float tolerance)
{
    if (matrix == NULL)
    {
        return 1;
    }

    float offDiagonal = fabs(matrix->m12) + fabs(matrix->m21);
    return (offDiagonal < tolerance) ? 1 : 0;
}

/**
 * @brief  对2x2矩阵进行奇异值分解
 * @param  inputMatrix 输入矩阵指针
 * @param  result SVD结果输出指针
 * @retval 0-成功, -1-参数错误, -2-迭代不收敛
 * @note   使用Jacobi迭代法进行SVD分解
 */
int8_t Matrix_SVD_Decompose(const Matrix2x2_T *inputMatrix, SVDResult_T *result)
{
    /* 参数检查 */
    if ((inputMatrix == NULL) || (result == NULL))
    {
        return -1;
    }

    Matrix2x2_T workingMatrix = *inputMatrix;
    Matrix2x2_T uMatrix = { 1.0, 0.0, 0.0, 1.0 };
    Matrix2x2_T vMatrix = { 1.0, 0.0, 0.0, 1.0 };

    uint16_t iterationCount = 0;
    uint8_t convergenceFlag = 0;

    /* Jacobi迭代主循环 */
    for (iterationCount = 0; iterationCount < MATRIX_SVD_MAX_ITERATIONS; iterationCount++)
    {
        float cosAngle = 0.0;
        float sinAngle = 0.0;

        /* 对(1,2)位置进行旋转 */
        _JacobiRotation2x2(&workingMatrix, &cosAngle, &sinAngle, 0);
        _ApplyRotation(&workingMatrix, cosAngle, sinAngle, 0, 1);
        _ApplyRotation(&uMatrix, cosAngle, sinAngle, 0, 1);

        /* 对(2,1)位置进行旋转 */
        _JacobiRotation2x2(&workingMatrix, &cosAngle, &sinAngle, 1);
        _ApplyRotation(&workingMatrix, cosAngle, sinAngle, 1, 0);
        _ApplyRotation(&vMatrix, cosAngle, sinAngle, 1, 0);

        /* 检查收敛性 */
        if (_CheckConvergence(&workingMatrix, MATRIX_SVD_EPSILON) != 0)
        {
            convergenceFlag = 1;
            break;
        }
    }

    /* 确保奇异值为正数 */
    if (workingMatrix.m11 < 0)
    {
        workingMatrix.m11 = -workingMatrix.m11;
        uMatrix.m11 = -uMatrix.m11;
        uMatrix.m12 = -uMatrix.m12;
    }

    if (workingMatrix.m22 < 0)
    {
        workingMatrix.m22 = -workingMatrix.m22;
        uMatrix.m21 = -uMatrix.m21;
        uMatrix.m22 = -uMatrix.m22;
    }

    /* 按奇异值大小排序 */
    if (workingMatrix.m11 < workingMatrix.m22)
    {
        float tempSingular = workingMatrix.m11;
        workingMatrix.m11 = workingMatrix.m22;
        workingMatrix.m22 = tempSingular;

        /* 交换U和V的列 */
        float tempU1 = uMatrix.m11;
        float tempU2 = uMatrix.m21;
        uMatrix.m11 = uMatrix.m12;
        uMatrix.m21 = uMatrix.m22;
        uMatrix.m12 = tempU1;
        uMatrix.m22 = tempU2;

        float tempV1 = vMatrix.m11;
        float tempV2 = vMatrix.m21;
        vMatrix.m11 = vMatrix.m12;
        vMatrix.m21 = vMatrix.m22;
        vMatrix.m12 = tempV1;
        vMatrix.m22 = tempV2;
    }

    /* 填充结果 */
    result->uMatrix = uMatrix;
    result->vMatrix = vMatrix;
    result->singularValue1 = workingMatrix.m11;
    result->singularValue2 = workingMatrix.m22;
    result->convergenceFlag = convergenceFlag;
    result->iterationCount = iterationCount;

    return (convergenceFlag != 0) ? 0 : -2;
}

/**
 * @brief  计算矩阵的条件数
 * @param  result SVD结果指针
 * @retval 矩阵的条件数
 * @note   条件数 = 最大奇异值 / 最小奇异值
 */
float Matrix_SVD_CalculateConditionNumber(const SVDResult_T *result)
{
    if ((result == NULL) || (fabs(result->singularValue2) < MATRIX_SVD_EPSILON))
    {
        return 1e36;
    }

    return result->singularValue1 / result->singularValue2;
}

/**
 * @brief  从SVD结果重构原始矩阵
 * @param  result SVD分解结果指针
 * @param  outputMatrix 重构后的矩阵输出指针
 * @retval 0-成功, -1-参数错误
 * @note   验证分解的正确性: A = U * Sigma * V^T
 */
int8_t Matrix_SVD_ReconstructMatrix(const SVDResult_T *result, Matrix2x2_T *outputMatrix)
{
    if ((result == NULL) || (outputMatrix == NULL))
    {
        return -1;
    }

    /* 计算 U * Sigma */
    Matrix2x2_T uSigma = {
        result->uMatrix.m11 * result->singularValue1,
        result->uMatrix.m12 * result->singularValue2,
        result->uMatrix.m21 * result->singularValue1,
        result->uMatrix.m22 * result->singularValue2
    };

    /* 计算 (U * Sigma) * V^T */
    outputMatrix->m11 = uSigma.m11 * result->vMatrix.m11 + uSigma.m12 * result->vMatrix.m21;
    outputMatrix->m12 = uSigma.m11 * result->vMatrix.m12 + uSigma.m12 * result->vMatrix.m22;
    outputMatrix->m21 = uSigma.m21 * result->vMatrix.m11 + uSigma.m22 * result->vMatrix.m21;
    outputMatrix->m22 = uSigma.m21 * result->vMatrix.m12 + uSigma.m22 * result->vMatrix.m22;

    return 0;
}

/**
 * @brief  初始化平均值滤波器
 * @param  filter 平均值滤波器结构体指针
 * @param  size 滤波器大小
 */
void AverageFilt_Init(AverageFilt_T *filter, uint16_t size)
{
    uint8_t *buffer = NULL;

    /* 判断指针是否非法 */
    if (filter == NULL) { return; }

    /* 如果已经初始化且大小相同，则清空滤波器 */
    if (filter->InitFlag)
    {
        AverageFilt_Clear(filter);
        return;
    }

    /* 判断分配大小是否合法 */
    if (size > MID_FILT_ALLOC_SIZE || size == 0)
    {
        /* 处理错误 */
        return;
    }

    /* 分配内存 */
    buffer = Memory_Malloc(MP_ROM_IN, sizeof(float) * filter->size);
    if (buffer != NULL)
    {
        /* 初始化结构体成员 */
        filter->size = size;
        filter->index = 0;
        filter->sum = 0;

        filter->data = (float *)buffer;
        Memory_Set(filter->data, 0, sizeof(float) * filter->size);
    }
    else
    {
        filter->size = 0;
        filter->index = 0;
        filter->sum = 0;
    }

    filter->InitFlag = 1;
}

/**
 * @brief  平均值滤波器
 * @param  filter 平均值滤波器结构体指针
 * @param  newData 新数据点
 * @retval 滤波后的平均值
 */
float AverageFilt_Filter(AverageFilt_T *filter, float newData)
{
    /* 判断指针是否非法 */
    if (filter == NULL) { return 0.0; }

    /* 如果未初始化，直接返回新数据 */
    if (!filter->InitFlag || filter->size == 0)
    {
        return newData;
    }

    if (filter->index >= filter->size) { filter->index = 0; }

    /* 更新当前和 */
    filter->sum += newData - filter->data[filter->index];
    /* 更新数据数组 */
    filter->data[filter->index] = newData;
    /* 更新索引 */
    filter->index = (filter->index + 1) % filter->size;

    /* 返回平均值 */
    return filter->sum / filter->size;
}

/**
 * @brief  清空平均值滤波器内部的数值
 * @param  filter 平均值滤波器结构体指针
 */
void AverageFilt_Clear(AverageFilt_T *filter)
{
    /* 判断指针是否非法 */
    if (filter == NULL) { return; }

    /* 如果未初始化，直接返回新数据 */
    if (!filter->InitFlag || filter->size == 0)
    {
        return;
    }

    Memory_Set(filter->data, 0, sizeof(float) * filter->size);
    filter->index = filter->sum = 0;
}

/**
 * @brief  初始化插入排序中值滤波器
 * @param  filter 滤波器结构体指针
 * @param  size 缓冲区大小
 */
void MedianFilt_Init(MedianFilt_T *filter, uint16_t size)
{
    uint8_t *buffer = NULL;

    /* 判断指针是否非法 */
    if (filter == NULL) { return; }

    /* 如果已经初始化且大小相同，则清空滤波器 */
    if (filter->InitFlag)
    {
        MedianFilt_Clear(filter);
        return;
    }

    if (filter == NULL || size == 0)
    {
        return;
    }

    /* 分配内存 */
    buffer = Memory_Malloc(MP_ROM_IN, sizeof(float) * (2 * size));
    if (buffer != NULL)
    {
        /* 初始化结构体成员 */
        filter->bufferSize = size;
        filter->currentIndex = 0;
        filter->dataCount = 0;
        filter->isBufferFilled = 0;
        filter->InitFlag = 1;
        filter->buffer = (float *)(&buffer[0]);
        filter->sortedBuffer = (float *)(&buffer[size * sizeof(float)]);

        /* 清空缓冲区 */
        Memory_Set(filter->buffer, 0, size * sizeof(float));
        Memory_Set(filter->sortedBuffer, 0, size * sizeof(float));
    }
    else
    {
        filter->bufferSize = 0;
        filter->currentIndex = 0;
        filter->buffer = NULL;
        filter->sortedBuffer = NULL;
    }
}

/**
 * @brief  使用插入排序更新中值滤波器
 * @param  filter 滤波器结构体指针
 * @param  newValue 新输入值
 * @retval 中位数滤波结果
 */
float MedianFilt_Update(MedianFilt_T *filter, float newValue)
{
    if (filter == NULL || filter->buffer == NULL || filter->sortedBuffer == NULL)
    {
        return newValue;
    }

    /* 1. 更新循环缓冲区 */
    filter->buffer[filter->currentIndex] = newValue;

    /* 2. 更新数据计数和填充状态 */
    if (filter->dataCount < filter->bufferSize)
    {
        filter->dataCount++;
    }
    else
    {
        filter->isBufferFilled = 1;
    }

    /* 3. 使用插入排序维护有序数组 */
    if (filter->dataCount == 1)
    {
        /* 第一个元素直接放入 */
        filter->sortedBuffer[0] = newValue;
    }
    else
    {
        /* 找到新元素在有序数组中的插入位置 */
        int16_t insertPos = filter->dataCount - 2; /* 从倒数第二个开始比较 */

        /* 如果缓冲区已满，需要移除最旧的元素 */
        if (filter->isBufferFilled)
        {
            /* 找到要移除的元素在有序数组中的位置 */
            float oldestValue = filter->buffer[(filter->currentIndex + 1) % filter->bufferSize];
            int16_t removePos = -1;

            /* 在有序数组中查找要移除的元素 */
            for (int16_t i = 0; i < filter->dataCount - 1; i++)
            {
                if (filter->sortedBuffer[i] == oldestValue)
                {
                    removePos = i;
                    break;
                }
            }

            if (removePos >= 0)
            {
                /* 移除元素：将后面的元素前移 */
                for (int16_t i = removePos; i < filter->dataCount - 1; i++)
                {
                    filter->sortedBuffer[i] = filter->sortedBuffer[i + 1];
                }
            }

            insertPos = filter->dataCount - 2; /* 更新插入位置 */
        }
        else
        {
            /* 缓冲区未满，在末尾为新元素腾出位置 */
            for (int16_t i = filter->dataCount - 1; i > insertPos + 1; i--)
            {
                filter->sortedBuffer[i] = filter->sortedBuffer[i - 1];
            }
        }

        /* 找到新元素的正确插入位置 */
        while (insertPos >= 0 && filter->sortedBuffer[insertPos] > newValue)
        {
            filter->sortedBuffer[insertPos + 1] = filter->sortedBuffer[insertPos];
            insertPos--;
        }

        /* 插入新元素 */
        filter->sortedBuffer[insertPos + 1] = newValue;
    }

    /* 4. 更新索引 */
    filter->currentIndex = (filter->currentIndex + 1) % filter->bufferSize;

    /* 5. 计算并返回中位数 */
    if (filter->dataCount == 0)
    {
        return newValue;
    }

    if (filter->dataCount % 2 == 1)
    {
        /* 奇数个元素，取中间值 */
        return filter->sortedBuffer[filter->dataCount / 2];
    }
    else
    {
        /* 偶数个元素，取中间两个的平均值 */
        uint16_t mid = filter->dataCount / 2;
        return (filter->sortedBuffer[mid - 1] + filter->sortedBuffer[mid]) / 2.0f;
    }
}

/**
 * @brief  清空中值滤波器
 * @param  filter 滤波器结构体指针
 */
void MedianFilt_Clear(MedianFilt_T *filter)
{
    if (filter == NULL || filter->buffer == NULL || filter->sortedBuffer == NULL)
    {
        return;
    }

    filter->currentIndex = 0;
    filter->dataCount = 0;
    filter->isBufferFilled = 0;

    Memory_Set(filter->buffer, 0, filter->bufferSize * sizeof(float));
    Memory_Set(filter->sortedBuffer, 0, filter->bufferSize * sizeof(float));
}

/**
 * @brief  获取当前有效数据个数
 * @param  filter 滤波器结构体指针
 * @retval 有效数据个数
 */
uint16_t MedianFilt_GetDataCount(const MedianFilt_T *filter)
{
    if (filter == NULL)
    {
        return 0;
    }
    return filter->dataCount;
}

/**
 * @brief  检查缓冲区是否已填满
 * @param  filter 滤波器结构体指针
 * @retval 1-已填满, 0-未填满
 */
uint8_t MedianFilt_IsBufferFilled(const MedianFilt_T *filter)
{
    if (filter == NULL)
    {
        return 0;
    }
    return filter->isBufferFilled;
}

/**
 * @brief  初始化卡尔曼滤波器
 * @param  kalmanFilter 卡尔曼滤波器结构体指针
 * @param  initX 初始估计值
 * @param  initP 初始估计误差协方差
 * @param  Q 过程噪声协方差
 * @param  R 测量噪声协方差
 * @note   初始化时需根据实际场景设置合理的噪声参数
 */
void Kalman_Init(KalmanFilter_T *kalmanFilter, float initX, float initP, float Q, float R)
{
    if (kalmanFilter == NULL)
    {
        return;
    }

    kalmanFilter->X_Est = initX;
    kalmanFilter->P_Est = initP;
    kalmanFilter->Q = Q;
    kalmanFilter->R = R;
    kalmanFilter->X_Pred = 0.0f;
    kalmanFilter->P_Pred = 0.0f;
    kalmanFilter->K = 0.0f;

    // 记录初始化时的initX和initP
    kalmanFilter->initX = initX;
    kalmanFilter->initP = initP;
}

/**
 * @brief  卡尔曼滤波更新步骤
 * @param  kalmanFilter 卡尔曼滤波器结构体指针
 * @param  measurement 测量值
 * @retval 滤波后的估计值
 * @note   根据测量值修正预测结果，更新估计值和误差协方差
 */
float Kalman_Update(KalmanFilter_T *kalmanFilter, float measurement)
{
    if (kalmanFilter == NULL)
    {
        return 0.0f;
    }

    /* 预测状态 */
    kalmanFilter->X_Pred = kalmanFilter->X_Est;

    /* 预测误差协方差 */
    kalmanFilter->P_Pred = kalmanFilter->P_Est + kalmanFilter->Q;

    /* 计算卡尔曼增益 */
    kalmanFilter->K = kalmanFilter->P_Pred / (kalmanFilter->P_Pred + kalmanFilter->R);

    /* 更新估计值 */
    kalmanFilter->X_Est = kalmanFilter->X_Pred + kalmanFilter->K * (measurement - kalmanFilter->X_Pred);

    /* 更新估计误差协方差 */
    kalmanFilter->P_Est = (1.0f - kalmanFilter->K) * kalmanFilter->P_Pred;

    return kalmanFilter->X_Est;
}

/**
 * @brief  重置卡尔曼滤波器状态
 * @param  kalmanFilter 滤波器结构体指针
 * @note   用于系统重启、状态突变等场景，保留过程噪声和测量噪声配置
 */
void Kalman_Clear(KalmanFilter_T *kalmanFilter)
{
    if (kalmanFilter == NULL) return;

    kalmanFilter->X_Est = kalmanFilter->initX; /* 用初始值重置状态估计 */
    kalmanFilter->P_Est = kalmanFilter->initP; /* 用初始值重置协方差 */
    kalmanFilter->X_Pred = 0.0f;               /* 重置预测值 */
    kalmanFilter->P_Pred = 0.0f;               /* 重置预测协方差 */
    kalmanFilter->K = 0.0f;                    /* 重置卡尔曼增益 */
}

/**
 * @brief  设置过程噪声协方差
 * @param  kf 卡尔曼滤波器结构体指针
 * @param  Q 过程噪声协方差
 */
void Kalman_SetProcessNoise(KalmanFilter_T *kf, float Q)
{
    if (kf == NULL) return;
    kf->Q = Q;
}

/**
 * @brief  设置测量噪声协方差
 * @param  kf 卡尔曼滤波器结构体指针
 * @param  R 测量噪声协方差
 */
void Kalman_SetMeasurementNoise(KalmanFilter_T *kf, float R)
{
    if (kf == NULL) return;
    kf->R = R;
}

/**
 * @brief  清除PID控制器的所有状态
 * @param  pid PID控制器结构体指针
 */
void PID_Clear(PID_T *pid)
{
    if (pid == NULL) return;

    pid->Target = 0;
    pid->Actual = 0;
    pid->Output = 0;
    pid->Error = 0;
    pid->ErrorInt = 0;
    pid->LastError = 0;
    pid->LastOutput = 0;
}

/**
 * @brief  执行带积分分离的PID控制算法
 * @param  pid PID控制器结构体指针
 * @param  deltaTime 控制周期时间增量
 * @retval PID输出值（经积分限幅、输出限幅和一阶低通滤波后）
 */
float PID_Control(PID_T *pid, float deltaTime)
{
    if (pid == NULL) return 0.0f;

    /* 计算当前偏差 */
    pid->Error = pid->Target - pid->Actual;

    /* 过零处理，范围(-pid->ZeroLimit, pid->ZeroLimit] */
    if (pid->ZeroLimit != 0)
    {
        pid->Error = fmodf(pid->Error, 2 * pid->ZeroLimit);

        if (pid->Error > pid->ZeroLimit)
        {
            pid->Error -= 2 * pid->ZeroLimit;
        }
        if (pid->Error <= -pid->ZeroLimit)
        {
            pid->Error += 2 * pid->ZeroLimit;
        }
    }

    /* 积分限幅 */
    if (pid->ErrorInt > pid->IntegralLimit)
    {
        pid->ErrorInt = pid->IntegralLimit;
    }
    else if (pid->ErrorInt < -pid->IntegralLimit)
    {
        pid->ErrorInt = -pid->IntegralLimit;
    }

    /* PID输出计算 */
    pid->Output = pid->Kp * pid->Error + pid->ErrorInt + pid->Kd * (pid->Error - pid->LastError);

    /* 输出限幅 */
    if (pid->Output > pid->OutputLimit)
    {
        pid->Output = pid->OutputLimit;
    }
    else if (pid->Output < -pid->OutputLimit)
    {
        pid->Output = -pid->OutputLimit;
    }

    /* 输出滤波 */
    pid->Output = 0.75 * pid->LastOutput + 0.25 * pid->Output;

    /* 更新历史值 */
    pid->LastOutput = pid->Output;
    pid->LastError = pid->Error;

    return pid->Output;
}

/**
 * @brief  复制PID参数（不包括运行状态）
 * @param  dst 目标PID控制器结构体指针
 * @param  src 源PID控制器结构体指针
 */
void PID_Copy(PID_T *dst, PID_T *src)
{
    if (src == NULL || dst == NULL) return;

    dst->Kp = src->Kp;
    dst->Ki = src->Ki;
    dst->Kd = src->Kd;
    dst->OutputLimit = src->OutputLimit;
    dst->IntegralLimit = src->IntegralLimit;
}

/**
 * @brief  从参数结构体设置PID参数（不包括运行状态）
 * @param  dst 目标PID控制器结构体指针
 * @param  src 源PID参数结构体指针
 */
void PID_SetParam(PID_T *dst, PidParam_T *src)
{
    if (src == NULL || dst == NULL) return;

    dst->Kp = src->Kp;
    dst->Ki = src->Ki;
    dst->Kd = src->Kd;
    dst->OutputLimit = src->OutputLimit;
    dst->IntegralLimit = src->IntegralLimit;
}
