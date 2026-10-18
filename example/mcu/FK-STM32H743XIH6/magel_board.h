#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 从 SDRAM 申请 VM 内存并进入 LCD 程序选择循环。
 * @retval 非零表示初始化失败；初始化成功后持续处理程序选择，不返回。
 * @note 单次程序退出或执行错误后释放该次运行资源并返回选择页面。
 */
int Magel_BoardRun(void);
#ifdef __cplusplus
}
#endif
