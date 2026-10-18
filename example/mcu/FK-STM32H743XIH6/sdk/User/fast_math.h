#ifndef __FF_MATH_H
#define __FF_MATH_H

#define FLOAT_CALC_EPS 1e-4f

#ifndef M_PI
#define M_PI 3.14159265358979f
#endif
#ifndef LN_2
#define LN_2 0.69314718055994f
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 普通计算函数
void Math_Init(void);
float Math_Sin(float x);
float Math_Cos(float x);
float Math_Tan(float x);
float Math_Atan2(float y, float x);
float Math_Exp(float x);
float Math_Log(float x);
float Math_InvSqrt(float x);
float Math_Sqrt(float x);
float Math_Asin(float x);
float Math_Acos(float x);

// 计算右尾概率
float Math_FDist(float F, float m, float n);
float Math_TDist(float t, float n);
float Math_ChiDist(float x, float n);
float Math_NormDist(float x);

// 欧拉积分
float Math_LnGamma(float x);
float Math_Gamma(float x);
float Math_Beta(float z, float w);

#ifdef __cplusplus
}
#endif

#endif /* __FF_MATH_H */
