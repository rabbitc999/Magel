#include "gui.h"

uint8_t GUI_MenuNeedScrollBar(GUI_Device_T *baseUI);
uint16_t GUI_MenuGetScrollBarWidth(GUI_Device_T *baseUI);

/**
 * @brief 获取菜单最大行数
 * @param baseUI 菜单设备指针，包含绘制器配置信息
 * @return uint16_t 最大行数，如果baseUI为NULL则返回0
 * @note 根据菜单区域总高度和行间距计算可显示的最大行数
 * @note 计算公式：总高度 / 行间距
 * @warning 调用前需确保baseUI指针有效
 */
static uint16_t GUI_MenuGetMaxCharLine(GUI_Device_T *baseUI)
{
    if (baseUI == NULL)
    {
        return 0;
    }
    return baseUI->menuPainter.Height / baseUI->menuPainter.lineInterval;
}

/**
 * @brief 计算字符对应的像素宽度（从左边开始）
 * @param baseUI 菜单设备指针，包含字符宽度配置
 * @param charCount 字符数量
 * @return uint16_t 像素宽度，如果baseUI为NULL则返回0
 * @note 用于计算字符串在屏幕上占用的水平像素空间
 * @note 计算公式：字符数量 × 字符宽度
 * @warning 需确保charCount值合理，避免溢出
 */
static uint16_t GUI_MenuGetStringIndexLeft(GUI_Device_T *baseUI, uint16_t charCount)
{
    if (baseUI == NULL)
    {
        return 0;
    }
    return charCount * baseUI->menuPainter.charWidth;
}

/**
 * @brief 计算字符对应的像素宽度（从右边开始）
 * @param baseUI 菜单设备指针，包含屏幕和滚动条配置
 * @param charCount 字符数量
 * @return uint16_t 从右边界开始的像素位置，如果baseUI为NULL则返回0
 * @note 用于计算字符串右对齐时的起始X坐标
 * @note 计算公式：屏幕宽度 - 字符宽度 - 滚动条宽度
 * @note 自动考虑滚动条占用的空间
 * @warning 返回值可能为负数，调用时需进行边界检查
 */
uint16_t GUI_MenuGetStringIndexRight(GUI_Device_T *baseUI, uint16_t charCount)
{
    if (baseUI == NULL)
    {
        return 0;
    }
    return baseUI->menuPainter.Width - GUI_MenuGetStringIndexLeft(baseUI, charCount) - GUI_MenuGetScrollBarWidth(baseUI);
}

/**
 * @brief 检查菜单项的绘制详情函数是否有效
 * @param item 菜单项指针，包含绘制函数指针
 * @return uint8_t 1=有效, 0=无效
 */
static uint8_t GUI_MenuAssertDrawDetailFunction(GUI_BaseItem_T *item)
{
    if (item == NULL || item->drawDetail == NULL)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 检查菜单项的进入函数是否有效
 * @param item 菜单项指针，包含进入函数指针
 * @return uint8_t 1=有效, 0=无效
 */
static uint8_t GUI_MenuAssertEnterFunction(GUI_BaseItem_T *item)
{
    if (item == NULL || item->enter == NULL)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 检查菜单项的退出函数是否有效
 * @param item 菜单项指针，包含退出函数指针
 * @return uint8_t 1=有效, 0=无效
 */
static uint8_t GUI_MenuAssertExitFunction(GUI_BaseItem_T *item)
{
    if (item == NULL || item->exit == NULL)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 检查菜单项的变更函数是否有效
 * @param item 菜单项指针，包含变更函数指针
 * @return uint8_t 1=有效, 0=无效
 */
static uint8_t GUI_MenuAssertChangeFunction(GUI_BaseItem_T *item)
{
    if (item == NULL || item->change == NULL)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 获取滚动条宽度
 * @param baseUI 菜单设备指针，包含滚动条配置信息
 * @return uint16_t 滚动条宽度，如果baseUI为NULL或滚动条未启用则返回0
 * @note 只有当showScrollBar为TRUE时才返回实际滚动条宽度
 * @note 滚动条用于指示当前菜单项在整个菜单中的位置
 * @warning 返回值可能为0，调用时需注意处理
 */
uint16_t GUI_MenuGetScrollBarWidth(GUI_Device_T *baseUI)
{
    if (baseUI == NULL)
    {
        return 0;
    }
    return baseUI->menuPainter.showScrollBar && GUI_MenuNeedScrollBar(baseUI) ? baseUI->menuPainter.scrollBarWidth : 0;
}

/**
 * @brief 获取字符显示偏移量
 * @param baseUI 菜单设备指针，包含行高和字符高度配置
 * @return uint16_t 字符垂直偏移量，如果baseUI为NULL则返回0
 * @note 用于计算字符在行中的垂直居中位置
 * @note 计算公式：(行间距 - 字符高度) / 2
 * @note 确保字符在行中垂直居中显示
 * @warning 返回值可能为0，表示无需偏移
 */
uint16_t GUI_MenuGetCharOffset(GUI_Device_T *baseUI)
{
    if (baseUI == NULL)
    {
        return 0;
    }
    return (baseUI->menuPainter.lineInterval - baseUI->menuPainter.charHeight) / 2;
}

/**
 * @brief 计算行对应的像素位置（从上边开始）
 * @param baseUI 菜单设备指针，包含行间距配置
 * @param line 行号（从0开始）
 * @return uint16_t Y坐标，如果baseUI为NULL则返回0
 * @note 用于计算指定行在屏幕上的垂直像素位置
 * @note 计算公式：行号 × 行间距
 * @note 行号0对应第一行，通常用于显示标题
 * @warning 需确保line值在有效范围内，避免超出屏幕边界
 */
uint16_t GUI_MenuGetStringLineUp(GUI_Device_T *baseUI, uint16_t line)
{
    if (baseUI == NULL)
    {
        return 0;
    }
    return line * baseUI->menuPainter.lineInterval;
}

/**
 * @brief 获取当前活动的菜单
 * @param baseUI 指向菜单设备的指针，该设备包含菜单栈和栈顶信息
 * @return GUI_Menu_T* 返回当前活动的菜单的指针
 */
GUI_Menu_T *GUI_GetCurrentMenu(GUI_Device_T *baseUI)
{
    if (baseUI->menuStackTop != 0)
    {
        return baseUI->menuStack[baseUI->menuStackTop - 1];
    }
    return NULL;
}

/**
 * @brief 获取当前活动的页面
 * @param baseUI 指向菜单设备的指针，包含当前页面信息
 * @return GUI_Page_T* 返回当前活动的页面的指针
 */
GUI_Page_T *GUI_GetCurrentPage(GUI_Device_T *baseUI)
{
    return &baseUI->currentPage;
}

/**
 * @brief 获取当前选中的菜单项
 * @param baseUI 指向菜单设备的指针，该设备包含菜单栈和栈顶信息
 * @return GUI_BaseItem_T* 返回当前选中的菜单项的指针
 */
GUI_BaseItem_T *GUI_GetSelectItem(GUI_Device_T *baseUI)
{
    GUI_Menu_T *currentMenu = GUI_GetCurrentMenu(baseUI);

    if (currentMenu != NULL)
    {
        return currentMenu->select;
    }
    return NULL;
}

/**
 * @brief 获取两个菜单项之间的距离
 * @param pItem1 第一个菜单项的指针
 * @param pItem2 第二个菜单项的指针
 * @return uint16_t 返回两个菜单项之间的距离，如果未找到则返回0xFFFF
 */
int16_t GUI_MenuGetItemDistance(GUI_BaseItem_T *pItem1, GUI_BaseItem_T *pItem2)
{
    GUI_BaseItem_T *pItem;
    int16_t distance = 0;

    if (pItem1 == pItem2)
        return 0;

    pItem = pItem1;
    while (pItem != NULL)
    {
        if (pItem == pItem2)
            return distance;
        pItem = pItem->next;
        distance--;
    }

    pItem = pItem1;
    distance = 0;
    while (pItem != NULL)
    {
        if (pItem == pItem2)
            return distance;
        pItem = pItem->prev;
        distance++;
    }

    return 0xFFFF; /* 未找到，返回一个不可能的距离值 */
}

/**
 * @brief 显示菜单的操作模式和单位
 * @param baseUI 指向菜单设备的指针，包含菜单的相关信息
 * @return void 无返回值
 */
void GUI_MenuShowUnitMode(GUI_Device_T *baseUI)
{
    /* 根据是否处于编辑模式，显示相应的模式字符串 */
    GUI_MenuShowString(baseUI, 0, 0, baseUI->valueFlag ? baseUI->menuPainter.editModeString : baseUI->menuPainter.itemModeString,
                       baseUI->menuPainter.headerFontColor, baseUI->menuPainter.headerBlackColor);

    /* 根据当前的单位选择，显示对应的单位字符串 */
    uint16_t x = GUI_MenuGetStringIndexRight(baseUI, String_GetLength(baseUI->menuPainter.unitString[baseUI->changeUnit], baseUI->menuPainter.Width / baseUI->menuPainter.charWidth));
    x += GUI_MenuGetScrollBarWidth(baseUI);
    GUI_MenuShowString(baseUI, x, 0, baseUI->menuPainter.unitString[baseUI->changeUnit], baseUI->menuPainter.headerFontColor, baseUI->menuPainter.headerBlackColor);
}

/**
 * @brief 获取当前菜单设备的单位值
 * @param baseUI 指向菜单设备的指针，包含单位选择标志
 * @return float 返回当前选择的单位值，如果没有匹配的单位选择则返回0
 */
float GUI_MenuGetUnitValue(GUI_Device_T *baseUI)
{
    /* 根据菜单设备的单位选择标志，选择对应的单位值 */
    return baseUI->menuPainter.unitValue[baseUI->changeUnit];
}

/**
 * @brief 显示菜单标题
 * @param currentMenu 指向当前菜单的指针
 * @return void 无返回值
 */
void GUI_MenuShowMenuTitle(GUI_Device_T *baseUI, GUI_Menu_T *currentMenu)
{
    uint16_t maxWidth = baseUI->menuPainter.Width / baseUI->menuPainter.charWidth;
    uint16_t modeStringSize = GUI_MenuGetStringIndexLeft(baseUI, String_GetLength(baseUI->menuPainter.unitString[baseUI->changeUnit], maxWidth));                                          /* 计算模式字符串的起始位置 */
    uint16_t unitStringSize = GUI_MenuGetStringIndexLeft(baseUI, String_GetLength(baseUI->valueFlag ? baseUI->menuPainter.editModeString : baseUI->menuPainter.itemModeString, maxWidth)); /* 计算单位字符串的起始位置 */
    uint16_t titleStringWidth = GUI_MenuGetStringIndexLeft(baseUI, String_GetLength(currentMenu->title, maxWidth));                                                                        /* 计算菜单标题字符串的宽度 */
    uint16_t showIndex = modeStringSize + GUI_MAX(baseUI->menuPainter.Width - modeStringSize - unitStringSize - titleStringWidth, 0) / 2;                                                  /* 计算菜单标题的显示位置 */

    GUI_MenuDrawRectangle(baseUI, 0, baseUI->menuPainter.headerBlackColor);                                                                  /* 填充标题行背景颜色 */
    GUI_MenuShowString(baseUI, showIndex, 0, currentMenu->title, baseUI->menuPainter.headerFontColor, baseUI->menuPainter.headerBlackColor); /* 在指定位置显示菜单标题 */
    GUI_MenuShowUnitMode(baseUI);                                                                                                            /* 显示菜单的操作模式和单位 */
}

/**
 * @brief 退回前级菜单
 * @param pThis 指向当前菜单项的指针
 * @return void 无返回值
 */
void GUI_MenuItemBack(GUI_Device_T *baseUI, GUI_BaseItem_T *pThis)
{
    /* 获取当前菜单项所属的菜单设备 */
    GUI_ASSERT(baseUI->menuStackTop >= 1);

    /* 若菜单栈顶指针大于1，将栈顶指针减1，并设置全局更新标志 */
    if (baseUI->menuStackTop > 1)
    {
        baseUI->menuStackTop--;
        baseUI->updateAllFlag = TRUE;

        /* 跳转菜单需要执行退出操作 */
        GUI_BaseItem_T *selectJumpItem = GUI_GetSelectItem(baseUI);

        /* 如果当前选中的菜单项是跳转项且自定义退出函数不为空，则调用自定义退出函数 */
        if (selectJumpItem->itemType == GUI_MENU_ITEM_TYPE_JUMP && selectJumpItem->customExit != NULL)
        {
            selectJumpItem->customExit(selectJumpItem);
        }
    }
    else
    {
        /* 清空菜单界面 */
        GUI_MenuClearShow(baseUI);

        /* 设置为主函数运行 */
        baseUI->menuStackTop = 0;
        baseUI->updateAllFlag = TRUE;
        baseUI->pageRunFlag = TRUE;
        baseUI->currentPage.page = baseUI->mainPage;
        baseUI->currentPage.userState = 0;
        baseUI->currentPage.InitFlag = TRUE;
        baseUI->returnPage.page = NULL;
    }
}

/**
 * @brief 跳转下级菜单
 * @param pThis 指向当前菜单项的指针
 * @param pMenu 指向要跳转的菜单的指针
 * @return void 无返回值
 */
void GUI_MenuItemJump(GUI_Device_T *baseUI, GUI_BaseItem_T *pThis, GUI_Menu_T *pMenu)
{
    /* 获取当前菜单项所属的菜单设备 */
    GUI_ASSERT(baseUI->menuStackTop < GUI_MENU_DEPTH);

    /* 设置要跳转的菜单的起始索引和选中索引为0 */
    pMenu->start = pMenu->first;
    pMenu->select = pMenu->first;

    /* 将要跳转的菜单压入菜单栈 */
    baseUI->menuStack[baseUI->menuStackTop++] = pMenu;

    /* 设置全局更新标志 */
    baseUI->updateAllFlag = TRUE;
}

/**
 * @brief 设置运行函数
 * @param baseUI 指向菜单设备的指针
 * @param runPage 要设置的运行函数指针
 * @return void 无返回值
 */
void GUI_ChangeRunPage(GUI_Device_T *baseUI, GUI_MenuPageCallback_T runPage)
{
    GUI_ASSERT(runPage != NULL);

    GUI_Page_T *pagePtr = GUI_GetCurrentPage(baseUI);
    pagePtr->page = runPage;
    pagePtr->userState = 0;
    pagePtr->InitFlag = TRUE;
    baseUI->pageRunFlag = TRUE;
    baseUI->pageChangeFlag = TRUE;
}

/**
 * @brief 进入页面函数
 * @param baseUI 指向菜单设备的指针
 * @param runPage 要设置的运行函数指针
 * @return 0:进入失败 1:进入成功
 */
uint8_t GUI_EnterPage(GUI_Device_T *baseUI, GUI_MenuPageCallback_T runPage)
{
    /* 保存当前页面为返回页面 */
    baseUI->returnPage = baseUI->currentPage;

    baseUI->pageRunFlag = TRUE;
    baseUI->pageChangeFlag = TRUE;

    GUI_Page_T *pagePtr = GUI_GetCurrentPage(baseUI);
    pagePtr->page = runPage;
    pagePtr->userState = 0;
    pagePtr->InitFlag = TRUE;
    return 1;
}

/**
 * @brief 设置根菜单
 * @param baseUI 指向菜单设备的指针
 * @param rootMenu 根菜单指针
 */
void GUI_SetRootMenu(GUI_Device_T *baseUI, GUI_Menu_T *rootMenu)
{
    if (baseUI == NULL || rootMenu == NULL)
    {
        return;
    }

    baseUI->menuStack[0] = rootMenu;
    baseUI->menuStackTop = 0;
}

/**
 * @brief 检查是否有根菜单
 * @param baseUI 指向菜单设备的指针
 * @return TRUE:有根菜单 FALSE:无根菜单
 */
uint8_t GUI_HasRootMenu(GUI_Device_T *baseUI)
{
    return (baseUI != NULL && baseUI->menuStackTop == 0 && baseUI->menuStack[0] != NULL);
}

/**
 * @brief 退出页面
 * @param baseUI 指向菜单设备的指针
 * @return 0:退出失败 1:退出到页面 2:退出到菜单
 */
uint8_t GUI_ExitPage(GUI_Device_T *baseUI)
{
    if (baseUI == NULL)
    {
        return 0;
    }

    /* 情况1：有返回页面，退出到上一页面 */
    if (baseUI->returnPage.page != NULL)
    {
        baseUI->currentPage = baseUI->returnPage;
        baseUI->currentPage.userState = 0;
        baseUI->currentPage.InitFlag = TRUE;
        baseUI->returnPage.page = NULL;
        return 1;
    }

    /* 情况2：在菜单栈中，退出到菜单 */
    if (baseUI->menuStackTop > 0)
    {
        baseUI->pageRunFlag = FALSE;
        return 2;
    }

    /* 情况3：单页面情况，检查是否在主页面 */
    if (baseUI->currentPage.page != baseUI->mainPage)
    {
        baseUI->currentPage.page = baseUI->mainPage;
        baseUI->currentPage.userState = 0;
        baseUI->currentPage.InitFlag = TRUE;
        baseUI->pageChangeFlag = TRUE;
        return 1;
    }

    /* 情况4：在主页面，检查是否有根菜单 */
    if (baseUI->menuStackTop == 0 && baseUI->menuStack[0] != NULL)
    {
        /* 有根菜单，退出到菜单 */
        baseUI->pageRunFlag = FALSE;
        return 2;
    }

    /* 情况5：在主页面且无根菜单，无法退出 */
    return 0;
}

/**
 * @brief 进入菜单操作
 * @param baseUI 指向菜单设备的指针
 * @param rootMenu 根菜单指针
 * @return 0:退出失败 1:退出到页面 2:退出到菜单
 */
void GUI_EnterMenu(GUI_Device_T *baseUI, GUI_Menu_T *rootMenu)
{
    baseUI->menuStackTop = 0;
    baseUI->menuStack[0] = rootMenu;
    baseUI->pageRunFlag = FALSE;

    /* 执行自定义进入回调 */
    GUI_BaseItem_T *currentItem = rootMenu->select;
    if (currentItem != NULL && currentItem->customEnter != NULL)
    {
        currentItem->customEnter(currentItem);
    }
}

/**
 * @brief 退出当前菜单
 * @param baseUI 指向菜单设备的指针
 * @return 0:退出失败(已在根菜单) 1:成功退出到上级菜单
 */
uint8_t GUI_ExitMenu(GUI_Device_T *baseUI)
{
    if (baseUI == NULL)
    {
        return 0;
    }

    /* 检查是否在根菜单层 */
    if (baseUI->menuStackTop <= 0)
    {
        return 0; /* 已经在根菜单，无法退出 */
    }

    /* 退出当前菜单 */
    baseUI->menuStackTop--;

    /* 获取上级菜单 */
    GUI_Menu_T *parentMenu = baseUI->menuStack[baseUI->menuStackTop - 1];

    /* 重置菜单状态 */
    parentMenu->start = parentMenu->first;
    parentMenu->select = parentMenu->first;

    /* 设置全局更新标志 */
    baseUI->updateAllFlag = TRUE;

    /* 退出页面运行状态（如果有） */
    baseUI->pageRunFlag = FALSE;

    /* 执行自定义退出回调 */
    GUI_BaseItem_T *currentItem = parentMenu->select;
    if (currentItem != NULL && currentItem->customExit != NULL)
    {
        currentItem->customExit(currentItem);
    }

    return 1;
}

/**
 * @brief 判断是否需要显示滚动条
 * @param baseUI 菜单设备指针
 * @param totalItems 菜单项总数
 * @return uint8_t 1=需要显示, 0=不需要显示
 * @note 根据菜单项总数和可显示行数判断是否需要滚动条
 * @note 考虑了标题行占用的空间
 * @warning 如果baseUI为NULL，返回0（不需要显示）
 */
uint8_t GUI_MenuNeedScrollBar(GUI_Device_T *baseUI)
{
    GUI_Menu_T *currentMenu = GUI_GetCurrentMenu(baseUI);

    if (currentMenu == NULL)
    {
        return 0;
    }

    uint16_t totalItems = 0;
    GUI_BaseItem_T *tempItem = currentMenu->first;

    while (tempItem != NULL)
    {
        totalItems++;
        tempItem = tempItem->next;
    }

    /* 计算最大可见菜单项数量（减1是因为标题行） */
    uint16_t maxVisibleItems = GUI_MenuGetMaxCharLine(baseUI) - 1;

    /* 如果菜单项数量大于可见行数，则需要显示滚动条 */
    return (totalItems > maxVisibleItems) ? 1 : 0;
}

/**
 * @brief 退出菜单运行函数
 * @param sender 指向发送者菜单项的指针
 * @param object 指向操作对象的指针
 * @return void 无返回值
 */
static void exitItemFunction(GUI_BaseItem_T *pThis)
{
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    GUI_MenuItemBack(baseUI, pThis);
}

/**
 * @brief 显示一个项目
 * @param currentItem 指向当前菜单项的指针
 * @param itemLine 菜单项所在的行号
 * @param currentIndex 当前菜单项的索引
 * @return void 无返回值
 */
static void GUI_MenuShowItem(GUI_Device_T *baseUI, GUI_Menu_T *currentMenu, GUI_BaseItem_T *currentItem, uint32_t itemLine)
{
    /* 第一次调用需要更新文本值 */
    if (currentMenu->firstUpdate)
    {
        if (currentItem != currentMenu->select)
        {
            GUI_MenuDrawRectangle(baseUI, itemLine, baseUI->menuPainter.notSelectBlackColor);
        }
        else
        {
            GUI_MenuDrawRectangle(baseUI, itemLine, baseUI->menuPainter.selectBlackColor);
        }
    }

    /* 调用菜单项的绘制详细信息函数 */
    GUI_ASSERT(GUI_MenuAssertDrawDetailFunction(currentItem));
    currentItem->drawDetail(currentItem, itemLine);
}

/**
 * @brief 绘制菜单移动进度条（修复溢出版本）
 * @param baseUI 菜单设备指针
 * @param currentMenu 当前菜单指针
 */
static void GUI_MenuDrawScrollBar(GUI_Device_T *baseUI, GUI_Menu_T *currentMenu)
{
    if (baseUI == NULL || currentMenu == NULL || !baseUI->menuPainter.showScrollBar)
    {
        return;
    }

    /* 计算菜单项总数 */
    uint16_t totalItems = 0;
    GUI_BaseItem_T *tempItem = currentMenu->first;
    while (tempItem != NULL)
    {
        totalItems++;
        tempItem = tempItem->next;
    }

    /* 如果菜单项数量少于等于显示行数，不显示滚动条 */
    uint16_t maxVisibleItems = GUI_MenuGetMaxCharLine(baseUI) - 1; /* 减1是因为标题行 */
    if (totalItems <= maxVisibleItems)
    {
        return;
    }

    /* 计算当前选中项的索引 */
    uint16_t currentIndex = 0;
    tempItem = currentMenu->first;
    while (tempItem != NULL && tempItem != currentMenu->select)
    {
        currentIndex++;
        tempItem = tempItem->next;
    }

    /* 确保当前索引在有效范围内 */
    if (currentIndex >= totalItems)
    {
        currentIndex = totalItems - 1;
    }

    /* 计算滚动条区域 */
    uint16_t scrollBarMinY = baseUI->menuPainter.lineInterval;
    uint16_t scrollBarMaxY = baseUI->menuPainter.Height;

    /* 确保滚动条区域有效 */
    if (scrollBarMaxY <= scrollBarMinY)
    {
        return;
    }

    uint16_t scrollBarHeight = scrollBarMaxY - scrollBarMinY;
    uint16_t scrollBarWidth = baseUI->menuPainter.scrollBarWidth;
    uint16_t scrollBarX = baseUI->menuPainter.Width - baseUI->menuPainter.scrollBarWidth;

    /* 安全计算滑块高度 */
    uint16_t sliderHeight;
    if (totalItems > 0)
    {
        /* 使用更安全的计算方法，避免溢出 */
        sliderHeight = (uint16_t)((uint32_t)scrollBarHeight * maxVisibleItems / totalItems);

        /* 确保滑块最小高度 */
        if (sliderHeight < 3)
        {
            sliderHeight = 3;
        }

        /* 确保滑块不超过滚动条高度 */
        if (sliderHeight > scrollBarHeight)
        {
            sliderHeight = scrollBarHeight;
        }
    }
    else
    {
        sliderHeight = scrollBarHeight;
    }

    /* 安全计算滑块位置 */
    uint16_t sliderPosition;
    if (totalItems > 1)
    {
        /* 使用更安全的计算方法，避免溢出 */
        uint32_t availableHeight = scrollBarHeight - sliderHeight;
        sliderPosition = scrollBarMinY + (uint16_t)((uint32_t)availableHeight * currentIndex / (totalItems - 1));

        /* 确保滑块位置在有效范围内 */
        if (sliderPosition < scrollBarMinY)
        {
            sliderPosition = scrollBarMinY;
        }
        else if (sliderPosition + sliderHeight > scrollBarMaxY)
        {
            sliderPosition = scrollBarMaxY - sliderHeight;
        }
    }
    else
    {
        /* 只有一个项目时，滑块在顶部 */
        sliderPosition = scrollBarMinY;
    }

    /* 绘制滚动条背景 */
    GUI_MenuDrawRectangleEx(baseUI, baseUI->menuPainter.StartX + scrollBarX, baseUI->menuPainter.StartY + scrollBarMinY, scrollBarWidth, scrollBarHeight,
                            baseUI->menuPainter.scrollBarBackgroundColor);

    /* 绘制滚动条滑块 */
    GUI_MenuDrawRectangleEx(baseUI, baseUI->menuPainter.StartX + scrollBarX, baseUI->menuPainter.StartY + sliderPosition, scrollBarWidth, sliderHeight,
                            baseUI->menuPainter.scrollBarForegroundColor);
}

/**
 * @brief 显示菜单项目
 * @param baseUI 指向菜单设备的指针
 * @return void 无返回值
 */
static void GUI_MenuShowItemList(GUI_Device_T *baseUI)
{
    uint32_t itemLine = 1;
    GUI_Menu_T *currentMenu = GUI_GetCurrentMenu(baseUI);

    /* 获取当前菜单项的指针 */
    GUI_BaseItem_T *currentItem = currentMenu->start;

    /* 若全局更新标志有效，清空菜单界面并显示菜单标题 */
    if (baseUI->updateAllFlag)
    {
        currentMenu->firstUpdate = TRUE;

        GUI_MenuClearShow(baseUI);
        GUI_MenuShowMenuTitle(baseUI, currentMenu);
        GUI_MenuDrawScrollBar(baseUI, currentMenu);
    }

    /* 循环显示菜单项，直到达到最大行数或菜单项数量 */
    for (; itemLine < GUI_MenuGetMaxCharLine(baseUI) && currentItem != NULL; itemLine++)
    {
        GUI_MenuShowItem(baseUI, currentMenu, currentItem, itemLine);

        /* 移动到下一个菜单项 */
        currentItem = currentItem->next;
    }

    /* 显示剩余空白 */
    for (; itemLine < GUI_MenuGetMaxCharLine(baseUI); itemLine++)
    {
        GUI_MenuDrawRectangle(baseUI, itemLine, baseUI->menuPainter.notSelectBlackColor);
    }

    /* 清空全局更新标志和首次更新标志 */
    baseUI->updateAllFlag = FALSE;
    currentMenu->firstUpdate = FALSE;
}

/**
 * @brief 菜单调用函数
 */
void GUI_MenuRunFunction(GUI_Device_T *baseUI, uint8_t userKey)
{
    GUI_Page_T *pagePtr = GUI_GetCurrentPage(baseUI);

    /* 设置上一次函数运行标志 */
    baseUI->pageLastRunFlag = TRUE;

    /* 设置上一次函数运行标志 */
    GUI_FuncItem_T *selectItem = (GUI_FuncItem_T *)GUI_GetSelectItem(baseUI);

    GUI_MenuParam_T param = { 0 };
    param.baseMenu = GUI_GetCurrentMenu(baseUI);
    param.baseUI = baseUI;
    param.key = userKey;
    param.userState = pagePtr->userState;
    param.InitFlag = pagePtr->InitFlag;

    /* 若菜单栈为空，调用主函数并传入无按键参数 */
    uint16_t userState = pagePtr->page(selectItem, &param);

    /* 查看用户界面是否被覆盖 */
    if (!baseUI->pageChangeFlag)
    {
        pagePtr->InitFlag = FALSE;
        pagePtr->userState = userState;
    }

    /* 清理更新标志 */
    baseUI->pageChangeFlag = FALSE;
}

/**
 * @brief UI运行函数
 * @param baseUI 指向菜单设备的指针
 * @return void 无返回值
 */
void GUI_MenuLoop(GUI_Device_T *baseUI)
{
    GUI_Menu_T *currentMenu = NULL;

    /* 检查函数运行标志是否为真 */
    if (baseUI->pageRunFlag)
    {
        GUI_MenuRunFunction(baseUI, GUI_MENU_KEY_NONE);
    }
    else if (baseUI->pageLastRunFlag)
    {
        /* 检查菜单栈是否为空 */
        if (baseUI->menuStackTop == 0)
        {
            /* 若菜单栈为空，将栈顶指针加1 (对应的是主界面的退出操作) */
            baseUI->menuStackTop++;

            GUI_Menu_T *rootMenu = GUI_GetCurrentMenu(baseUI);
            rootMenu->start = rootMenu->first;
            rootMenu->select = rootMenu->first;
        }

        /* 设置全局更新标志 */
        baseUI->updateAllFlag = TRUE;
        baseUI->pageLastRunFlag = FALSE;

        /* 获取当前选中的菜单项 */
        currentMenu = GUI_GetCurrentMenu(baseUI);

        /* 设置首次更新标志 */
        currentMenu->firstUpdate = TRUE;
    }
    else
    {
        currentMenu = GUI_GetCurrentMenu(baseUI);
        GUI_MenuShowItemList(baseUI);
    }

    /* 进行按键事件 */
    GUI_MenuKeyBasicOperator(baseUI);
}

/**
 * @brief UI项目进入操作
 * @param baseUI 指向菜单设备的指针
 * @return void 无返回值
 */
void GUI_MenuEnterEvent(GUI_Device_T *baseUI)
{
    /* 检查函数运行标志是否为真 */
    if (baseUI->pageRunFlag)
    {
        GUI_MenuRunFunction(baseUI, GUI_MENU_KEY_ENTER);
    }
    else
    {
        /* 将当前选中的菜单项指针转换为基础菜单项指针 */
        GUI_BaseItem_T *selectItem = GUI_GetSelectItem(baseUI);

        /* 调用菜单项的进入函数 */
        if (GUI_MenuAssertEnterFunction(selectItem))
        {
            selectItem->enter(selectItem);
        }

        /* 判断项目是否有自定义进入函数 */
        if (selectItem->customEnter != NULL)
        {
            selectItem->customEnter(selectItem);
        }
    }
}

/**
 * @brief UI项目退出操作
 * @param baseUI 指向菜单设备的指针
 * @return void 无返回值
 */
void GUI_MenuExitEvent(GUI_Device_T *baseUI)
{
    /* 检查函数运行标志是否为真 */
    if (baseUI->pageRunFlag)
    {
        GUI_MenuRunFunction(baseUI, GUI_MENU_KEY_EXIT);
    }
    else
    {
        /* 将当前选中的菜单项指针转换为基础菜单项指针 */
        GUI_BaseItem_T *selectItem = GUI_GetSelectItem(baseUI);

        /* 调用菜单项的退出函数 */
        if (GUI_MenuAssertExitFunction(selectItem))
        {
            selectItem->exit(selectItem);

            /* 判断项目是否有自定义退出函数 */
            if (selectItem->customExit != NULL)
            {
                selectItem->customExit(selectItem);
            }
        }
        else
        {
            GUI_MenuItemBack(baseUI, selectItem);
        }
    }
}

/**
 * @brief UI项目修改操作
 * @param baseUI 指向菜单设备的指针
 * @param deltaValue 修改的差值
 * @return void 无返回值
 */
void GUI_MenuItemChangeEvent(GUI_Device_T *baseUI, int32_t deltaValue)
{
    /* 获取当前菜单 */
    GUI_Menu_T *currentMenu = GUI_GetCurrentMenu(baseUI);

    /* 判断是否需要更新菜单 */
    if (baseUI->pageRunFlag)
    {
        /* 根据修改的差值生成按键值 */
        int32_t keyPress = deltaValue > 0 ? (GUI_MENU_KEY_ITEM_UP | (deltaValue & 0x1F)) : (GUI_MENU_KEY_ITEM_DOWN | ((-deltaValue) & 0x1F));

        GUI_MenuRunFunction(baseUI, keyPress);
    }
    else
    {
        GUI_BaseItem_T *lastSelect = currentMenu->select;
        GUI_BaseItem_T *currentSelect = currentMenu->select;
        GUI_BaseItem_T *currentStart = currentMenu->start;
        int32_t step = (deltaValue > 0) ? 1 : -1;
        int32_t count = abs(deltaValue);

        /* 查找下一个可选中项 */
        while (count > 0 && currentSelect != NULL)
        {
            GUI_BaseItem_T *nextItem = currentSelect;

            /* 寻找下一个可选中的项目 */
            do
            {
                if (step > 0)
                {
                    nextItem = nextItem->next;
                    if (nextItem == NULL)
                    {
                        nextItem = currentSelect;
                        break;
                    }
                }
                else
                {
                    nextItem = nextItem->prev;
                    if (nextItem == NULL)
                    {
                        nextItem = currentSelect;
                        break;
                    }
                }

                /* 如果回到起点，说明没有可选项目 */
                if (nextItem == currentSelect)
                {
                    break;
                }
            } while (nextItem != NULL && nextItem->selectable == FALSE);

            /* 如果没有找到可选项目，保持原样 */
            if (nextItem == NULL || nextItem->selectable == FALSE)
            {
                break;
            }

            currentSelect = nextItem;
            count--;
        }

        /* 更新选中的菜单项 */
        currentMenu->select = currentSelect;

        /* 计算当前选中项在显示列表中的位置 */
        uint16_t lastSelectLine = GUI_MenuGetItemDistance(lastSelect, currentStart) + 1;
        uint16_t selectLine = GUI_MenuGetItemDistance(currentSelect, currentStart) + 1;

        /* 判断是否需要更新起始显示项 */
        if (selectLine >= GUI_MenuGetMaxCharLine(baseUI) || selectLine < 1)
        {
            /* 需要调整起始显示项 */
            if (step > 0)
            {
                GUI_BaseItem_T *tempSelect = currentMenu->start;
                selectLine = selectLine - GUI_MenuGetMaxCharLine(baseUI);
                while (selectLine >= 1 && tempSelect->next != NULL)
                {
                    tempSelect = tempSelect->next;
                    selectLine--;
                }
                if (tempSelect->next != NULL)
                {
                    currentMenu->start = tempSelect->next;
                }
            }
            else
            {
                currentMenu->start = currentMenu->select;
            }

            currentMenu->firstUpdate = TRUE;
            GUI_MenuShowItemList(baseUI);
            GUI_MenuDrawScrollBar(baseUI, currentMenu);
            currentMenu->firstUpdate = FALSE;
        }
        else
        {
            /* 只需更新两个菜单项 */
            currentMenu->firstUpdate = TRUE;
            GUI_MenuShowItem(baseUI, currentMenu, lastSelect, lastSelectLine);
            GUI_MenuShowItem(baseUI, currentMenu, currentSelect, selectLine);
            GUI_MenuDrawScrollBar(baseUI, currentMenu);
            currentMenu->firstUpdate = FALSE;
        }
    }
}

/**
 * @brief UI项目修改操作
 * @param baseUI 指向菜单设备的指针
 * @param deltaValue 修改的差值
 * @return void 无返回值
 */
void GUI_MenuValueChangeEvent(GUI_Device_T *baseUI, int32_t deltaValue)
{
    /* 获取当前菜单 */
    GUI_Menu_T *currentMenu = GUI_GetCurrentMenu(baseUI);

    /* 检查函数运行标志是否为真 */
    if (baseUI->pageRunFlag)
    {
        /* 根据修改的差值生成按键值 */
        int32_t keyPress = deltaValue > 0 ? (GUI_MENU_KEY_VALUE_UP | (deltaValue & 0x1F)) : (GUI_MENU_KEY_VALUE_DOWN | ((-deltaValue) & 0x1F));

        GUI_MenuRunFunction(baseUI, keyPress);
    }
    else
    {
        /* 获取当前活动的菜单项目 */
        GUI_BaseItem_T *selectItem = currentMenu->select;

        /* 调用菜单项的修改函数 */
        if (GUI_MenuAssertChangeFunction(selectItem))
        {
            selectItem->change(selectItem, deltaValue);
        }
        else if (selectItem->enter != NULL)
        {
            /* 执行进入函数 */
            selectItem->enter(selectItem);

            /* 判断项目是否有自定义进入函数 */
            if (selectItem->customEnter != NULL)
            {
                /* 执行项目进入函数 */
                selectItem->customEnter(selectItem);
            }
        }
    }
}

/**
 * @brief 设置滚动条显示状态
 * @param baseUI 菜单设备指针
 * @param show 是否显示滚动条
 */
void GUI_MenuSetScrollBarVisible(GUI_Device_T *baseUI, uint8_t show)
{
    if (baseUI != NULL)
    {
        baseUI->menuPainter.showScrollBar = show;
        baseUI->updateAllFlag = TRUE;
    }
}

/**
 * @brief 结束初始化UI界面
 * @param mainPage 主函数指针
 * @return void 无返回值
 */
void GUI_MenuDeviceInit(GUI_Device_T *baseUI, GUI_MenuPageCallback_T mainPage)
{
    /* 断言 */
    GUI_ASSERT(mainPage != NULL);

    /* 设置菜单设备的主函数 */
    baseUI->mainPage = mainPage;
    baseUI->pageRunFlag = TRUE;
    baseUI->pageLastRunFlag = FALSE;
    baseUI->updateAllFlag = FALSE;
    baseUI->valueFlag = FALSE;
    baseUI->pageChangeFlag = FALSE;
    baseUI->changeUnit = 0;

    /* 设置当前页面为主页面 */
    baseUI->currentPage.page = mainPage;
    baseUI->currentPage.userState = 0;
    baseUI->currentPage.InitFlag = TRUE;
    baseUI->returnPage.page = NULL;
    baseUI->returnPage.userState = 0;
    baseUI->returnPage.InitFlag = FALSE;

    /* 菜单栈保持未初始化状态 */
    baseUI->menuStackTop = 0;
    baseUI->menuStack[0] = NULL;

    /* 绘图器初始化 */
    baseUI->menuPainter.textItemFontColor = GUI_WHITE;
    baseUI->menuPainter.textItemBlackColor = GUI_RED;
    baseUI->menuPainter.valueItemEditFontColor = GUI_YELLOW;
    baseUI->menuPainter.valueItemEditBlackColor = GUI_BLACK;
    baseUI->menuPainter.valueItemReadOnlyFontColor = GUI_DARK_BLUE;
    baseUI->menuPainter.valueItemReadOnlyBlackColor = GUI_WHITE;

    baseUI->menuPainter.headerBlackColor = GUI_BLUE;
    baseUI->menuPainter.headerFontColor = GUI_WHITE;
    baseUI->menuPainter.selectBlackColor = GUI_GRAY;
    baseUI->menuPainter.selectFontColor = GUI_WHITE;
    baseUI->menuPainter.notSelectBlackColor = GUI_BLACK;
    baseUI->menuPainter.notSelectFontColor = GUI_WHITE;

    baseUI->menuPainter.textItemString = "";
    baseUI->menuPainter.jumpItemString = "[跳转]";
    baseUI->menuPainter.funcItemString = "[功能]";
    baseUI->menuPainter.pageItemString = "[页面]";
    baseUI->menuPainter.checkItemSelectString = "[开启]";
    baseUI->menuPainter.checkItemNotSelectString = "[关闭]";
    baseUI->menuPainter.itemModeString = "[项目]";
    baseUI->menuPainter.editModeString = "[数值]";
    baseUI->menuPainter.unitString[0] = "[10.00]";
    baseUI->menuPainter.unitString[1] = "[01.00]";
    baseUI->menuPainter.unitString[2] = "[00.10]";
    baseUI->menuPainter.unitString[3] = "[00.01]";

    baseUI->menuPainter.unitValue[0] = 10.00f;
    baseUI->menuPainter.unitValue[1] = 01.00f;
    baseUI->menuPainter.unitValue[2] = 00.10f;
    baseUI->menuPainter.unitValue[3] = 00.01f;

    baseUI->menuPainter.intValueShowLength = 8;
    baseUI->menuPainter.floatValueShowLength = 8;

    /* 初始化滚动条配置 */
    baseUI->menuPainter.scrollBarWidth = 4;
    baseUI->menuPainter.scrollBarBackgroundColor = GUI_GRAY;
    baseUI->menuPainter.scrollBarForegroundColor = GUI_WHITE;
    baseUI->menuPainter.showScrollBar = 1; /* 默认显示 */
}

/**
 * @brief 菜单添加函数
 * @param baseUI 指向菜单设备的指针
 * @param title 菜单标题字符串指针
 * @return GUI_Menu_T* 返回初始化后的菜单指针
 */
GUI_Menu_T *GUI_CreateMenu(GUI_Device_T *baseUI, const char *title)
{
    /* 断言菜单标题指针不为空 */
    GUI_ASSERT_WITH_RETURN(title != NULL, NULL);

    /* 获取当前要初始化的菜单指针 */
    GUI_Menu_T *pMenu = GUI_MenuMalloc(sizeof(GUI_Menu_T));
    GUI_ASSERT_WITH_RETURN(pMenu != NULL, NULL);
    /* Reused pool storage must not retain old list pointers or flags. */
    memset(pMenu, 0, sizeof(*pMenu));

    /* 设置菜单的标题 */
    pMenu->title = title;
    pMenu->baseUI = baseUI;
    pMenu->select = NULL;
    pMenu->start = NULL;

    /* 添加退出菜单项 */
    GUI_CreateFuncItem(baseUI, pMenu, GUI_MENU_EXIT_ITEM_TITLE_STRING, exitItemFunction);

    return pMenu;
}

/**
 * @brief 从菜单中移除指定的菜单项
 * @param baseUI 指向菜单设备的指针
 * @param pItem 要移除的菜单项指针
 * @return void 无返回值
 */
void GUI_DeleteItem(GUI_Device_T *baseUI, GUI_BaseItem_T *pItem)
{
    GUI_ASSERT(baseUI != NULL);
    GUI_ASSERT(pItem != NULL);

    GUI_Menu_T *pMenu = pItem->baseMenu;

    /* 从链表中移除菜单项 */
    if (pItem->prev != NULL)
    {
        pItem->prev->next = pItem->next;
    }
    else
    {
        /* 如果是第一个项目，更新菜单的First指针 */
        pMenu->first = pItem->next;
    }

    if (pItem->next != NULL)
    {
        pItem->next->prev = pItem->prev;
    }
    else
    {
        /* 如果是最后一个项目，更新菜单的Last指针 */
        pMenu->last = pItem->prev;
    }

    /* 如果移除的是当前选中的项目，需要更新选中状态 */
    if (pMenu->select == pItem)
    {
        pMenu->select = (pItem->next != NULL) ? pItem->next : pItem->prev;
    }

    /* 如果移除的是起始显示的项目，需要更新起始显示状态 */
    if (pMenu->start == pItem)
    {
        pMenu->start = (pItem->next != NULL) ? pItem->next : pItem->prev;
    }

    /* 释放菜单项内存 */
    GUI_MenuFree(pItem);
}

/**
 * @brief 移除整个菜单及其所有菜单项
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 要移除的菜单指针
 * @return void 无返回值
 */
void GUI_DeleteMenu(GUI_Device_T *baseUI, GUI_Menu_T *pMenu)
{
    GUI_ASSERT(baseUI != NULL);
    GUI_ASSERT(pMenu != NULL);

    /* 首先移除所有菜单项 */
    GUI_BaseItem_T *pItem = pMenu->first;
    while (pItem != NULL)
    {
        GUI_BaseItem_T *pNext = pItem->next;
        GUI_DeleteItem(baseUI, pItem);
        pItem = pNext;
    }

    /* 从菜单栈中移除该菜单(如果存在) */
    for (int i = 0; i < baseUI->menuStackTop; i++)
    {
        if (baseUI->menuStack[i] == pMenu)
        {
            /* 将后面的菜单向前移动 */
            for (int j = i; j < baseUI->menuStackTop - 1; j++)
            {
                baseUI->menuStack[j] = baseUI->menuStack[j + 1];
            }
            baseUI->menuStackTop--;
            break;
        }
    }

    /* 释放菜单内存 */
    GUI_MenuFree(pMenu);
}
