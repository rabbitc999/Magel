#include "gui.h"

int16_t GUI_MenuGetItemDistance(GUI_BaseItem_T *pItem1, GUI_BaseItem_T *pItem2);
uint16_t GUI_MenuGetStringIndexRight(GUI_Device_T *baseUI, uint16_t charCount);
void GUI_MenuShowUnitMode(GUI_Device_T *baseUI);
float GUI_MenuGetUnitValue(GUI_Device_T *baseUI);
void GUI_MenuItemBack(GUI_Device_T *baseUI, GUI_BaseItem_T *pThis);
void GUI_MenuItemJump(GUI_Device_T *baseUI, GUI_BaseItem_T *pThis, GUI_Menu_T *pMenu);

/**
 * @brief 项目详情函数 (文本菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针，通常包含行号信息
 * @return void 无返回值
 */
static void textItemDrawDetail(GUI_BaseItem_T *pThis, uint16_t currentLine)
{
    uint16_t x;

    /* 在指定位置显示文本菜单的详情信息 */
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;

    if (currentMenu->firstUpdate)
    {
        x = GUI_MenuGetStringIndexRight(baseUI, String_GetLength(baseUI->menuPainter.textItemString, baseUI->menuPainter.Width / baseUI->menuPainter.charWidth));

        uint16_t BlackColor = baseUI->menuPainter.textItemBlackColor;
        uint16_t fontColor = baseUI->menuPainter.textItemFontColor;

        GUI_MenuDrawRectangle(baseUI, currentLine, BlackColor);
        GUI_MenuShowString(baseUI, x, currentLine, baseUI->menuPainter.textItemString, fontColor, BlackColor);
        GUI_MenuShowString(baseUI, 0, currentLine, pThis->title, fontColor, BlackColor); /* 显示标题 */
    }
}

/**
 * @brief 项目详情函数 (跳转菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针，通常包含行号信息
 * @return void 无返回值
 */
static void jumpItemDrawDetail(GUI_BaseItem_T *pThis, uint16_t currentLine)
{
    uint16_t x;
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    int16_t selectLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    if (currentMenu->firstUpdate)
    {
        x = GUI_MenuGetStringIndexRight(baseUI, String_GetLength(baseUI->menuPainter.jumpItemString, baseUI->menuPainter.Width / baseUI->menuPainter.charWidth));

        uint16_t BlackColor = currentLine == selectLine ? baseUI->menuPainter.selectBlackColor : baseUI->menuPainter.notSelectBlackColor;
        uint16_t fontColor = currentLine == selectLine ? baseUI->menuPainter.selectFontColor : baseUI->menuPainter.notSelectFontColor;
        GUI_MenuShowString(baseUI, x, currentLine, baseUI->menuPainter.jumpItemString, fontColor, BlackColor);
        GUI_MenuShowString(baseUI, 0, currentLine, pThis->title, fontColor, BlackColor); /* 显示标题 */
    }
}

/**
 * @brief 进入函数 (跳转菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针
 * @return void 无返回值
 */
static void jumpItemEnter(GUI_BaseItem_T *pThis)
{
    /* 将当前菜单项指针转换为跳转菜单项指针 */
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    GUI_JumpItem_T *pItem = (GUI_JumpItem_T *)pThis;

    /* 获取要跳转的菜单指针并跳转 */
    GUI_MenuItemJump(baseUI, pThis, pItem->jump);
}

/**
 * @brief 项目详情函数 (函数菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针，通常包含行号信息
 * @return void 无返回值
 */
static void funcItemDrawDetail(GUI_BaseItem_T *pThis, uint16_t currentLine)
{
    uint16_t x;
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    GUI_FuncItem_T *pItem = (GUI_FuncItem_T *)pThis;
    int16_t selectLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    if (currentMenu->firstUpdate)
    {
        x = GUI_MenuGetStringIndexRight(baseUI, String_GetLength(baseUI->menuPainter.funcItemString, baseUI->menuPainter.Width / baseUI->menuPainter.charWidth));

        uint16_t BlackColor = currentLine == selectLine ? baseUI->menuPainter.selectBlackColor : baseUI->menuPainter.notSelectBlackColor;
        uint16_t fontColor = currentLine == selectLine ? baseUI->menuPainter.selectFontColor : baseUI->menuPainter.notSelectFontColor;

        /* 仅在有回调函数的情况下显示函数的功能文本 */
        if (pItem->callback.once != NULL)
        {
            if (pItem->funcType == GUI_MENU_FUNCTION_PAGE)
            {
                GUI_MenuShowString(baseUI, x, currentLine, baseUI->menuPainter.pageItemString, fontColor, BlackColor);
            }
            else
            {
                GUI_MenuShowString(baseUI, x, currentLine, baseUI->menuPainter.funcItemString, fontColor, BlackColor);
            }
        }

        GUI_MenuShowString(baseUI, 0, currentLine, pThis->title, fontColor, BlackColor); /* 显示标题 */
    }
}

/**
 * @brief 进入函数 (函数菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针
 * @return void 无返回值
 */
static void funcItemEnter(GUI_BaseItem_T *pThis)
{
    GUI_FuncItem_T *pItem = (GUI_FuncItem_T *)pThis;
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;

    /* 判断是否为循环运行菜单 */
    if (pItem->funcType == GUI_MENU_FUNCTION_PAGE)
    {
        /* 若函数类型为循环执行，且回调函数不为空 */
        if (pItem->callback.page != NULL)
        {
            baseUI->pageRunFlag = TRUE;
            baseUI->currentPage.page = pItem->callback.page;
            baseUI->currentPage.userState = 0;
            baseUI->currentPage.InitFlag = TRUE;
            baseUI->returnPage.page = NULL;
            GUI_MenuClearShow(baseUI);
        }
    }
    else
    {
        /* 若函数类型为单次执行，且回调函数不为空 */
        if (pItem->callback.once != NULL)
        {
            pItem->callback.once(pThis);
            pItem->parent.baseMenu->firstUpdate = TRUE;
        }
    }
}

/**
 * @brief 项目详情函数 (值修改菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针，通常包含行号信息
 * @return void 无返回值
 */
static void valueItemDrawDetail(GUI_BaseItem_T *pThis, uint16_t currentLine)
{
    /* 在指定位置显示文本菜单的详情信息 */
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)pThis;
    int16_t selectLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    uint16_t BlackColor = currentLine == selectLine ? baseUI->menuPainter.selectBlackColor : baseUI->menuPainter.notSelectBlackColor;
    uint16_t fontColor = currentLine == selectLine ? baseUI->menuPainter.selectFontColor : baseUI->menuPainter.notSelectFontColor;

    if (pThis->selectable == FALSE)
    {
        /* 文本菜单显示颜色 */
        BlackColor = baseUI->menuPainter.valueItemReadOnlyBlackColor;
        fontColor = baseUI->menuPainter.valueItemReadOnlyFontColor;
    }

    /* 菜单标题显示 */
    if (currentMenu->firstUpdate)
    {
        GUI_MenuDrawRectangle(baseUI, currentLine, BlackColor);
        GUI_MenuShowString(baseUI, 0, currentLine, pThis->title, fontColor, BlackColor); /* 显示标题 */
    }

    /* 若菜单为值模式(且选中)，则更换显示颜色(仅针对设置部分) */
    if (currentLine == selectLine && baseUI->valueFlag)
    {
        BlackColor = baseUI->menuPainter.valueItemEditBlackColor;
        fontColor = baseUI->menuPainter.valueItemEditFontColor;
    }

    if (pItem->disableShowNumberFlag) return;

    /* 数值显示 */
    uint16_t x = GUI_MenuGetStringIndexRight(baseUI, baseUI->menuPainter.intValueShowLength);
    uint16_t fx = GUI_MenuGetStringIndexRight(baseUI, baseUI->menuPainter.floatValueShowLength);

    switch (pItem->valueType)
    {
    case GUI_MENU_VALUE_TYPE_INT32:
        GUI_MenuShowInt(baseUI, x, currentLine, pItem->pValue->int32Value, fontColor, BlackColor);
        break;
    case GUI_MENU_VALUE_TYPE_INT16:
        GUI_MenuShowInt(baseUI, x, currentLine, pItem->pValue->int16Value, fontColor, BlackColor);
        break;
    case GUI_MENU_VALUE_TYPE_INT8:
        GUI_MenuShowInt(baseUI, x, currentLine, pItem->pValue->int8Value, fontColor, BlackColor);
        break;
    case GUI_MENU_VALUE_TYPE_UINT32:
        GUI_MenuShowInt(baseUI, x, currentLine, pItem->pValue->uint32Value, fontColor, BlackColor);
        break;
    case GUI_MENU_VALUE_TYPE_UINT16:
        GUI_MenuShowInt(baseUI, x, currentLine, pItem->pValue->uint16Value, fontColor, BlackColor);
        break;
    case GUI_MENU_VALUE_TYPE_UINT8:
        GUI_MenuShowInt(baseUI, x, currentLine, pItem->pValue->uint8Value, fontColor, BlackColor);
        break;
    case GUI_MENU_VALUE_TYPE_FLOAT:
        GUI_MenuShowFloat(baseUI, fx, currentLine, pItem->pValue->floatValue, fontColor, BlackColor);
        break;
    case GUI_MENU_VALUE_TYPE_DOUBLE:
        GUI_MenuShowFloat(baseUI, fx, currentLine, pItem->pValue->doubleValue, fontColor, BlackColor);
        break;
    default:
        break;
    }
}

/**
 * @brief 退出函数 (值修改菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针
 * @return void 无返回值
 */
static void valueItemExit(GUI_BaseItem_T *pThis)
{
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)pThis;

    if (baseUI->valueFlag)
    {
        baseUI->valueFlag = FALSE;
        GUI_MenuShowUnitMode(baseUI);
    }
    else
    {
        GUI_MenuItemBack(baseUI, pThis);
    }
}

/**
 * @brief 进入函数 (值修改菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针
 * @return void 无返回值
 */
static void valueItemEnter(GUI_BaseItem_T *pThis)
{
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)pThis;
    int16_t showLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    if (baseUI->valueFlag == FALSE)
    {
        /* 若修改类型为普通更新，开启修改标志并更新显示 */
        if (pItem->updateType == GUI_MENU_VALUE_UPDATE_CHANGE)
        {
            baseUI->valueFlag = TRUE;

            if (pItem->valueType == GUI_MENU_VALUE_TYPE_DOUBLE)
            {
                /* 浮点数类型差值 */
                double deltaValue = pItem->maxValue.doubleValue - pItem->minValue.doubleValue;

                /* 调节单位推测 */
                if (deltaValue > 100)
                {
                    baseUI->changeUnit = GUI_MENU_UNITA;
                }
                else if (deltaValue > 10)
                {
                    baseUI->changeUnit = GUI_MENU_UNITB;
                }
                else if (deltaValue > 1)
                {
                    baseUI->changeUnit = GUI_MENU_UNITC;
                }
                else
                {
                    baseUI->changeUnit = GUI_MENU_UNITD;
                }
            }
            if (pItem->valueType == GUI_MENU_VALUE_TYPE_FLOAT)
            {
                /* 浮点数类型差值 */
                float deltaValue = pItem->maxValue.floatValue - pItem->minValue.floatValue;

                /* 调节单位推测 */
                if (deltaValue > 100)
                {
                    baseUI->changeUnit = GUI_MENU_UNITA;
                }
                else if (deltaValue > 10)
                {
                    baseUI->changeUnit = GUI_MENU_UNITB;
                }
                else if (deltaValue > 1)
                {
                    baseUI->changeUnit = GUI_MENU_UNITC;
                }
                else
                {
                    baseUI->changeUnit = GUI_MENU_UNITD;
                }
            }
            else if (pItem->valueType == GUI_MENU_VALUE_TYPE_UINT32 || pItem->valueType == GUI_MENU_VALUE_TYPE_INT32)
            {
                /* 整数类型差值 */
                int32_t deltaValue = pItem->maxValue.int32Value - pItem->minValue.int32Value;

                /* 调节单位推测 */
                if (deltaValue > 100)
                {
                    baseUI->changeUnit = GUI_MENU_UNITA;
                }
                else
                {
                    baseUI->changeUnit = GUI_MENU_UNITB;
                }
            }
            else if (pItem->valueType == GUI_MENU_VALUE_TYPE_INT16 || pItem->valueType == GUI_MENU_VALUE_TYPE_UINT16)
            {
                /* 整数类型差值 */
                int16_t deltaValue = pItem->maxValue.int16Value - pItem->minValue.int16Value;

                /* 调节单位推测 */
                if (deltaValue > 100)
                {
                    baseUI->changeUnit = GUI_MENU_UNITA;
                }
                else
                {
                    baseUI->changeUnit = GUI_MENU_UNITB;
                }
            }
            else
            {
                /* 整数类型差值 */
                int8_t deltaValue = pItem->maxValue.int8Value - pItem->minValue.int8Value;

                /* 调节单位推测 */
                if (deltaValue > 100)
                {
                    baseUI->changeUnit = GUI_MENU_UNITA;
                }
                else
                {
                    baseUI->changeUnit = GUI_MENU_UNITB;
                }
            }

            GUI_MenuShowUnitMode(baseUI);
        }

        /* 若修改类型为清零更新，将值清零并保存 */
        if (pItem->updateType == GUI_MENU_VALUE_UPDATE_SET_VALUE)
        {
            /* 将值设置为默认值 */
            switch (pItem->valueType)
            {
            case GUI_MENU_VALUE_TYPE_DOUBLE:
                pItem->pValue->doubleValue = pItem->setDefaultValue.doubleValue;
                break;
            case GUI_MENU_VALUE_TYPE_FLOAT:
            case GUI_MENU_VALUE_TYPE_UINT32:
            case GUI_MENU_VALUE_TYPE_INT32:
                pItem->pValue->int32Value = pItem->setDefaultValue.int32Value;
                break;
            case GUI_MENU_VALUE_TYPE_UINT16:
            case GUI_MENU_VALUE_TYPE_INT16:
                pItem->pValue->int16Value = pItem->setDefaultValue.int16Value;
                break;
            case GUI_MENU_VALUE_TYPE_UINT8:
            case GUI_MENU_VALUE_TYPE_INT8:
                pItem->pValue->int8Value = pItem->setDefaultValue.int8Value;
                break;
            }

            /* 仅显示修改值 */
            valueItemDrawDetail(pThis, showLine);
        }
    }
    else
    {
        if (pItem->valueType == GUI_MENU_VALUE_TYPE_FLOAT)
        {
            baseUI->changeUnit = (baseUI->changeUnit + 1) % 4;
        }
        else
        {
            baseUI->changeUnit = (baseUI->changeUnit + 1) % 2;
        }

        /* 显示菜单的操作模式和单位 */
        GUI_MenuShowUnitMode(baseUI);
    }
}

/**
 * @brief 项目修改函数 (值修改菜单类)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针，通常包含修改的差值
 * @return void 无返回值
 */
static void valueItemChange(GUI_BaseItem_T *pThis, int16_t change)
{
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)pThis;
    int16_t showLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    /* 若修改类型不是普通更新，调用进入函数 */
    if (pItem->updateType != GUI_MENU_VALUE_UPDATE_CHANGE)
    {
        valueItemEnter(pThis);
        return;
    }

    /* 若修改标志未开启，开启修改标志并更新显示 */
    if (baseUI->valueFlag == FALSE)
    {
        baseUI->valueFlag = TRUE;
        GUI_MenuShowUnitMode(baseUI);
        return;
    }

    /* 根据值的类型，更新值并限制在最小值和最大值之间 */
    if (pItem->valueType == GUI_MENU_VALUE_TYPE_INT32)
    {
        pItem->pValue->int32Value = GUI_MENU_LIMIT(pItem->pValue->int32Value + GUI_FloatRound(GUI_MenuGetUnitValue(baseUI)) * change, pItem->minValue.int32Value, pItem->maxValue.int32Value);
    }
    if (pItem->valueType == GUI_MENU_VALUE_TYPE_INT16)
    {
        pItem->pValue->int16Value = GUI_MENU_LIMIT(pItem->pValue->int16Value + GUI_FloatRound(GUI_MenuGetUnitValue(baseUI)) * change, pItem->minValue.int16Value, pItem->maxValue.int16Value);
    }
    if (pItem->valueType == GUI_MENU_VALUE_TYPE_INT8)
    {
        pItem->pValue->int8Value = GUI_MENU_LIMIT(pItem->pValue->int8Value + GUI_FloatRound(GUI_MenuGetUnitValue(baseUI)) * change, pItem->minValue.int8Value, pItem->maxValue.int8Value);
    }
    if (pItem->valueType == GUI_MENU_VALUE_TYPE_UINT32)
    {
        pItem->pValue->uint32Value = GUI_MENU_LIMIT(pItem->pValue->uint32Value + GUI_FloatRound(GUI_MenuGetUnitValue(baseUI)) * change, pItem->minValue.uint32Value, pItem->maxValue.uint32Value);
    }
    if (pItem->valueType == GUI_MENU_VALUE_TYPE_UINT16)
    {
        pItem->pValue->uint16Value = GUI_MENU_LIMIT(pItem->pValue->uint16Value + GUI_FloatRound(GUI_MenuGetUnitValue(baseUI)) * change, pItem->minValue.uint16Value, pItem->maxValue.uint16Value);
    }
    if (pItem->valueType == GUI_MENU_VALUE_TYPE_UINT8)
    {
        pItem->pValue->uint8Value = GUI_MENU_LIMIT(pItem->pValue->uint8Value + GUI_FloatRound(GUI_MenuGetUnitValue(baseUI)) * change, pItem->minValue.uint8Value, pItem->maxValue.uint8Value);
    }
    if (pItem->valueType == GUI_MENU_VALUE_TYPE_FLOAT)
    {
        pItem->pValue->floatValue = GUI_MENU_LIMIT(pItem->pValue->floatValue + GUI_MenuGetUnitValue(baseUI) * change, pItem->minValue.floatValue, pItem->maxValue.floatValue);
        pItem->pValue->floatValue = GUI_FloatRound(pItem->pValue->floatValue * 100.0f) / 100.0f;
    }
    if (pItem->valueType == GUI_MENU_VALUE_TYPE_DOUBLE)
    {
        pItem->pValue->doubleValue = GUI_MENU_LIMIT(pItem->pValue->doubleValue + GUI_MenuGetUnitValue(baseUI) * change, pItem->minValue.doubleValue, pItem->maxValue.doubleValue);
        pItem->pValue->doubleValue = GUI_FloatRound(pItem->pValue->doubleValue * 100.0) / 100.0;
    }

    /* 显示修改后的值 */
    valueItemDrawDetail(pThis, showLine);
}

/**
 * @brief 项目详情函数 (多选菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针，通常包含行号信息
 * @return void 无返回值
 */
static void checkItemDrawDetail(GUI_BaseItem_T *pThis, uint16_t currentLine)
{
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    /* 将当前菜单项指针转换为多选菜单项指针 */
    GUI_CheckItem_T *pItem = (GUI_CheckItem_T *)pThis;
    int16_t selectLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;
    uint16_t BlackColor = currentLine == selectLine ? baseUI->menuPainter.selectBlackColor : baseUI->menuPainter.notSelectBlackColor;
    uint16_t fontColor = currentLine == selectLine ? baseUI->menuPainter.selectFontColor : baseUI->menuPainter.notSelectFontColor;
    uint16_t x = 0;

    if (pItem->selectShowString == NULL || pItem->unSelectShowString == NULL)
    {
        pItem->selectShowString = baseUI->menuPainter.checkItemSelectString;
        pItem->unSelectShowString = baseUI->menuPainter.checkItemNotSelectString;
    }

    if (currentMenu->firstUpdate)
    {
        GUI_MenuShowString(baseUI, 0, currentLine, pThis->title, fontColor, BlackColor); /* 显示标题 */
    }

    /* 判断是否选中 */
    if (GUI_MENU_IS_CHECKED(*pItem->pValue, pItem->equivalent))
    {
        x = GUI_MenuGetStringIndexRight(baseUI, String_GetLength(pItem->selectShowString, baseUI->menuPainter.Width / baseUI->menuPainter.charWidth));
        GUI_MenuShowString(baseUI, x, currentLine, pItem->selectShowString, fontColor, BlackColor);
    }
    else
    {
        x = GUI_MenuGetStringIndexRight(baseUI, String_GetLength(pItem->unSelectShowString, baseUI->menuPainter.Width / baseUI->menuPainter.charWidth));
        GUI_MenuShowString(baseUI, x, currentLine, pItem->unSelectShowString, fontColor, BlackColor);
    }
}

/**
 * @brief 进入函数 (多选菜单)
 * @param pThis 指向当前菜单项的指针
 * @param object 指向操作对象的指针
 * @return void 无返回值
 */
static void checkItemEnter(GUI_BaseItem_T *pThis)
{
    GUI_CheckItem_T *pItem = (GUI_CheckItem_T *)pThis;

    /* 根据当前选中状态，切换选项 */
    *pItem->pValue ^= GUI_MENU_CHECK_BIT(pItem->equivalent);
}

/**
 * @brief 模式菜单项绘制函数
 * @param pThis 菜单项指针
 * @param currentLine 当前行号
 */
static void modeItemDrawDetail(GUI_BaseItem_T *pThis, uint16_t currentLine)
{
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    GUI_ModeItem_T *pItem = (GUI_ModeItem_T *)pThis;
    int16_t selectLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    uint16_t BlackColor = currentLine == selectLine ?
                              baseUI->menuPainter.selectBlackColor :
                              baseUI->menuPainter.notSelectBlackColor;
    uint16_t fontColor = currentLine == selectLine ?
                             baseUI->menuPainter.selectFontColor :
                             baseUI->menuPainter.notSelectFontColor;

    if (currentMenu->firstUpdate)
    {
        /* 绘制背景 */
        GUI_MenuDrawRectangle(baseUI, currentLine, BlackColor);

        /* 显示标题 */
        GUI_MenuShowString(baseUI, 0, currentLine, pThis->title, fontColor, BlackColor);
    }

    /* 若菜单为值模式(且选中)，则更换模式菜单显示颜色(仅针对设置部分) */
    if (currentLine == selectLine && baseUI->valueFlag)
    {
        BlackColor = baseUI->menuPainter.valueItemEditBlackColor;
        fontColor = baseUI->menuPainter.valueItemEditFontColor;
    }

    /* 查找当前数值 */
    uint8_t hasValue = TRUE;
    if (pItem->isShortValue)
    {
        if (pItem->elements[pItem->modeIndex].value != *(uint8_t *)pItem->valuePtr)
        {
            for (pItem->modeIndex = 0; pItem->modeIndex < pItem->modeCount; pItem->modeIndex++)
            {
                if (pItem->elements[pItem->modeIndex].value == *(uint8_t *)pItem->valuePtr)
                {
                    break;
                }
            }
            if (pItem->modeIndex >= pItem->modeCount)
            {
                pItem->modeIndex = 0;
                hasValue = FALSE;
            }
        }
    }
    else
    {
        if (pItem->elements[pItem->modeIndex].value != *pItem->valuePtr)
        {
            for (pItem->modeIndex = 0; pItem->modeIndex < pItem->modeCount; pItem->modeIndex++)
            {
                if (pItem->elements[pItem->modeIndex].value == *pItem->valuePtr)
                {
                    break;
                }
            }
            if (pItem->modeIndex >= pItem->modeCount)
            {
                pItem->modeIndex = 0;
                hasValue = FALSE;
            }
        }
    }

    /* 显示当前模式名称 */
    if (hasValue)
    {
        const char *modeName = pItem->elements[pItem->modeIndex].Name;
        uint16_t x = GUI_MenuGetStringIndexRight(baseUI, String_GetLength(modeName, baseUI->menuPainter.Width / baseUI->menuPainter.charWidth));
        GUI_MenuShowString(baseUI, x, currentLine, modeName, fontColor, BlackColor);
    }
    else
    {
        const char *modeName = "未知模式";
        uint16_t x = GUI_MenuGetStringIndexRight(baseUI, String_GetLength(modeName, baseUI->menuPainter.Width / baseUI->menuPainter.charWidth));
        GUI_MenuShowString(baseUI, x, currentLine, modeName, fontColor, BlackColor);
    }
}

// 设置模式菜单的数值
static void modeItemSetValue(GUI_ModeItem_T *pItem)
{
    if (pItem->isShortValue)
    {
        *((uint8_t *)pItem->valuePtr) = pItem->elements[pItem->modeIndex].value;
    }
    else
    {
        *pItem->valuePtr = pItem->elements[pItem->modeIndex].value;
    }
}

/**
 * @brief 模式菜单项进入函数
 * @param pThis 菜单项指针
 */
static void modeItemEnter(GUI_BaseItem_T *pThis)
{
    GUI_ModeItem_T *pItem = (GUI_ModeItem_T *)pThis;
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    int16_t showLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    /* 只读模式不允许操作 */
    if (pItem->readOnly) { return; }

    /* Enter键：进入状态设置 */
    if (!baseUI->valueFlag)
    {
        baseUI->valueFlag = TRUE;

        /* 重新绘制当前项 */
        currentMenu->firstUpdate = TRUE;
        modeItemDrawDetail(pThis, showLine);
        currentMenu->firstUpdate = FALSE;
    }
    else
    {
        pItem->modeIndex = (pItem->modeIndex + pItem->modeCount - 1) % pItem->modeCount;
        modeItemSetValue(pItem);

        /* 重新绘制当前项 */
        currentMenu->firstUpdate = TRUE;
        modeItemDrawDetail(pThis, showLine);
        currentMenu->firstUpdate = FALSE;
    }
}

/**
 * @brief 模式菜单项退出函数
 * @param pThis 菜单项指针
 */
static void modeItemExit(GUI_BaseItem_T *pThis)
{
    GUI_ModeItem_T *pItem = (GUI_ModeItem_T *)pThis;
    GUI_Menu_T *currentMenu = pThis->baseMenu;
    GUI_Device_T *baseUI = currentMenu->baseUI;
    int16_t showLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    if (baseUI->valueFlag)
    {
        baseUI->valueFlag = FALSE;

        /* 重新绘制当前项 */
        currentMenu->firstUpdate = TRUE;
        modeItemDrawDetail(pThis, showLine);
        currentMenu->firstUpdate = FALSE;
    }
    else
    {
        GUI_MenuItemBack(baseUI, pThis);
    }
}

/**
 * @brief 模式菜单项变更函数
 * @param pThis 菜单项指针
 * @param change 变更量（+1向上，-1向下）
 */
static void modeItemChange(GUI_BaseItem_T *pThis, int16_t change)
{
    GUI_ModeItem_T *pItem = (GUI_ModeItem_T *)pThis;

    /* 只读模式不允许修改 */
    if (pItem->readOnly)
    {
        return;
    }

    GUI_Menu_T *currentMenu = pThis->baseMenu;
    int16_t showLine = GUI_MenuGetItemDistance(currentMenu->select, currentMenu->start) + 1;

    /* 上下键切换模式 */
    if (change > 0)
    {
        /* 向上键：切换到上一个模式 */
        pItem->modeIndex = (pItem->modeIndex + pItem->modeCount - 1) % pItem->modeCount;
        modeItemSetValue(pItem);
    }
    else
    {
        /* 向下键：切换到下一个模式 */
        pItem->modeIndex = (pItem->modeIndex + 1) % pItem->modeCount;
        modeItemSetValue(pItem);
    }

    /* 重新绘制当前项 */
    currentMenu->firstUpdate = TRUE;
    modeItemDrawDetail(pThis, showLine);
    currentMenu->firstUpdate = FALSE;
}

/**
 * @brief 添加一个基础的项目
 * @param pMenu 基于的菜单
 * @param title 菜单标题
 * @return 基础的菜单项目指针
 */
GUI_BaseItem_T *GUI_CreateBaseItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, char *title, uint32_t size)
{
    /* 获取当前要添加的菜单项指针 */
    GUI_BaseItem_T *pItem = GUI_MenuMalloc(size);
    GUI_ASSERT_WITH_RETURN(pItem != NULL, NULL);
    memset(pItem, 0, size);

    /* 设置菜单项 */
    pItem->baseMenu = pMenu;
    pItem->title = title;
    pItem->exit = NULL;
    pItem->enter = NULL;
    pItem->change = NULL;
    pItem->selectable = TRUE;

    /* 默认自定义进去和退出函数为NULL */
    pItem->customEnter = NULL;
    pItem->customExit = NULL;

    /* 添加到链表 */
    if (pMenu->first == NULL)
    {
        pMenu->first = pItem;
        pMenu->last = pItem;
        pItem->next = NULL;
        pItem->prev = NULL;
    }
    else
    {
        pMenu->last->next = pItem;
        pItem->prev = pMenu->last;
        pMenu->last = pItem;
        pItem->next = NULL;
    }

    /* 返回菜单项指针 */
    return (GUI_BaseItem_T *)pItem;
}

/**
 * @brief 添加文本菜单项
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param titleStr 格式化字符串，但不包含任何参数
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_TextItem_T *GUI_CreateTextItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *titleStr)
{
    /* 基础菜单项 */
    GUI_BaseItem_T *newItem = NULL;

    /* 添加菜单项 */
    newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)titleStr, sizeof(GUI_TextItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);

    /* 将基础菜单项转换为文本菜单项指针 */
    GUI_TextItem_T *pItem = (GUI_TextItem_T *)newItem;

    /* 设置菜单项的详细信息绘制函数为文本菜单详情绘制函数 */
    pItem->parent.drawDetail = textItemDrawDetail;

    /* 设置菜单项的类型为文本菜单 */
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_TEXT;
    pItem->parent.selectable = FALSE;

    return (GUI_TextItem_T *)newItem;
}

/**
 * @brief 添加跳转菜单项
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pJumpMenu 指向要跳转的菜单
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_JumpItem_T *GUI_CreateJumpItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, GUI_Menu_T *pJumpMenu)
{
    /* 添加菜单项 */
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_JumpItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_JumpItem_T *pItem = (GUI_JumpItem_T *)newItem;

    pItem->parent.drawDetail = jumpItemDrawDetail;
    pItem->parent.enter = jumpItemEnter;
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_JUMP;
    pItem->jump = pJumpMenu;
    return (GUI_JumpItem_T *)newItem;
}

/**
 * @brief 添加浮点数值菜单项(无存储功能)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向浮点数值变量的指针
 * @param Min 最小值
 * @param Max 最大值
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateFloatItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue, float Min, float Max)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ValueItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    /* 将基础菜单项转换为值修改菜单项指针 */
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    /* 处理浮点数类型 */
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_VALUE;
    pItem->valueType = GUI_MENU_VALUE_TYPE_FLOAT;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_CHANGE;
    pItem->parent.drawDetail = valueItemDrawDetail;
    pItem->parent.enter = valueItemEnter;
    pItem->parent.exit = valueItemExit;
    pItem->parent.change = valueItemChange;
    pItem->pValue = (GUI_StoreValue_T *)pValue; /* 获取整数指针 */
    pItem->minValue.floatValue = Min;
    pItem->maxValue.floatValue = Max;
    pItem->disableShowNumberFlag = FALSE;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加函数菜单项(单次执行)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pFunc 指向函数的指针
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_FuncItem_T *GUI_CreateFuncItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, GUI_MenuOnceCallback_T pFunc)
{
    /* 添加菜单项 */
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_PageItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_PageItem_T *pItem = (GUI_PageItem_T *)newItem;
    pItem->callback.once = pFunc;
    pItem->parent.drawDetail = funcItemDrawDetail;
    pItem->parent.enter = funcItemEnter;

    /* 设置菜单项的类型为函数菜单，单次执行 */
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_FUNC;
    pItem->funcType = GUI_MENU_FUNCTION_ONCE;

    return (GUI_PageItem_T *)newItem;
}

/**
 * @brief 添加函数菜单项(循环执行)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pFunc 指向函数的指针
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_PageItem_T *GUI_CreatePageItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, GUI_MenuPageCallback_T pFunc)
{
    /* 添加菜单项 */
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateFuncItem(baseUI, pMenu, title, (GUI_MenuDefaultEventCallback_T)pFunc);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_FuncItem_T *pItem = (GUI_FuncItem_T *)newItem;
    pItem->funcType = GUI_MENU_FUNCTION_PAGE;
    pItem->callback.page = pFunc;

    return (GUI_PageItem_T *)newItem;
}

/**
 * @brief 添加跳转菜单项
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pJumpMenu 指向要跳转的菜单
 * @param enter 自定义进入函数
 * @param exit 自定义退出函数
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_JumpItem_T *GUI_CreateJumpItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, GUI_Menu_T *pJumpMenu, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    /* 添加菜单项 */
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateJumpItem(baseUI, pMenu, title, pJumpMenu);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_JumpItem_T *)newItem;
}

/**
 * @brief 添加多选菜单项
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pVal 指向整数值变量的指针
 * @param eq 等效位(当等效位置1时该菜单项将被选中，范围0-7)
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_CheckItem_T *GUI_CreateCheckItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pVal, uint8_t eq)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_CheckItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_CheckItem_T *pItem = (GUI_CheckItem_T *)newItem; /* 将基础菜单项转换为多选菜单项指针 */

    pItem->parent.drawDetail = checkItemDrawDetail;
    pItem->parent.enter = checkItemEnter;
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_CHECK;
    pItem->pValue = pVal;
    pItem->equivalent = eq;
    pItem->selectShowString = baseUI->menuPainter.checkItemSelectString;
    pItem->unSelectShowString = baseUI->menuPainter.checkItemNotSelectString;

    return (GUI_CheckItem_T *)newItem;
}

/**
 * @brief 添加多选菜单项(带回调函数)
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pVal 指向整数值变量的指针
 * @param eq 等效位(当等效位置1时该菜单项将被选中，范围0-7)
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_CheckItem_T *GUI_CreateCheckItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pVal, uint8_t eq, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_CheckItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_CheckItem_T *pItem = (GUI_CheckItem_T *)newItem; /* 将基础菜单项转换为多选菜单项指针 */

    pItem->parent.drawDetail = checkItemDrawDetail;
    pItem->parent.enter = checkItemEnter;
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_CHECK;
    pItem->pValue = pVal;
    pItem->equivalent = eq;
    pItem->selectShowString = baseUI->menuPainter.checkItemSelectString;
    pItem->unSelectShowString = baseUI->menuPainter.checkItemNotSelectString;

    newItem->customEnter = pFunc;

    return (GUI_CheckItem_T *)newItem;
}

/**
 * @brief 添加浮点值菜单项(带存储功能，回调函数)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @param Min 最小值
 * @param Max 最大值
 * @param enter 自定义进入函数
 * @param exit 自定义退出函数
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateFloatItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue, float Min, float Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateFloatItem(baseUI, pMenu, title, pValue, Min, Max);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加浮点数值菜单项(只读)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向浮点数值变量的指针
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateFloatItemR(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateFloatItem(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_READONLY;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加浮点数值菜单项(清零)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向浮点数值变量的指针
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateFloatItemZ(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue, float setDefaultValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateFloatItem(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_SET_VALUE;
    pItem->setDefaultValue.floatValue = setDefaultValue;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加浮点数值菜单项(清零带回调函数)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @param pFunc 指向函数的指针
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateFloatItemZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue, float setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc)
{
    /* 添加菜单项 */
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateFloatItemZ(baseUI, pMenu, title, pValue, setDefaultValue);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);

    /* 设置菜单项的类型循环执行 */
    newItem->customEnter = pFunc;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加双精度浮点值菜单项(无存储功能)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向 double 值的指针
 * @param Min 最小值
 * @param Max 最大值
 * @return GUI_BaseItem_T* 新创建的菜单项指针，失败返回 NULL
 */
GUI_ValueItem_T *GUI_CreateDoubleItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue, double Min, double Max)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ValueItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;

    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_VALUE;
    pItem->valueType = GUI_MENU_VALUE_TYPE_DOUBLE;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_CHANGE;
    pItem->parent.drawDetail = valueItemDrawDetail;
    pItem->parent.enter = valueItemEnter;
    pItem->parent.exit = valueItemExit;
    pItem->parent.change = valueItemChange;
    pItem->pValue = (GUI_StoreValue_T *)pValue;
    pItem->minValue.doubleValue = Min;
    pItem->maxValue.doubleValue = Max;
    pItem->disableShowNumberFlag = FALSE;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加双精度浮点值菜单项(带回调)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @param Min 最小值
 * @param Max 最大值
 * @param enter 自定义进入函数
 * @param exit 自定义退出函数
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateDoubleItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue, double Min, double Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateDoubleItem(baseUI, pMenu, title, pValue, Min, Max);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加双精度浮点只读菜单项
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向 double 值的指针
 * @return GUI_BaseItem_T* 新创建的菜单项指针，失败返回 NULL
 */
GUI_ValueItem_T *GUI_CreateDoubleItemR(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateDoubleItem(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_READONLY;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加双精度浮点“清零”行为菜单项
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向 double 值的指针
 * @return GUI_BaseItem_T* 新创建的菜单项指针，失败返回 NULL
 */
GUI_ValueItem_T *GUI_CreateDoubleItemZ(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue, double setDefaultValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateDoubleItem(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_SET_VALUE;
    pItem->setDefaultValue.doubleValue = setDefaultValue;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加双精度浮点“清零”行为并指定进入回调
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向 double 值的指针
 * @param pFunc 自定义进入函数
 * @return GUI_BaseItem_T* 新创建的菜单项指针，失败返回 NULL
 */
GUI_ValueItem_T *GUI_CreateDoubleItemZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue, double setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateDoubleItemZ(baseUI, pMenu, title, pValue, setDefaultValue);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = pFunc;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加无符号32位整数值菜单项(无存储功能)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @param Min 最小值
 * @param Max 最大值
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateUintItem32(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue, uint32_t Min, uint32_t Max)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ValueItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    /* 将基础菜单项转换为值修改菜单项指针 */
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    /* 处理浮点数类型 */
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_VALUE;
    pItem->valueType = GUI_MENU_VALUE_TYPE_UINT32;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_CHANGE;
    pItem->parent.drawDetail = valueItemDrawDetail;
    pItem->parent.enter = valueItemEnter;
    pItem->parent.exit = valueItemExit;
    pItem->parent.change = valueItemChange;
    pItem->pValue = (GUI_StoreValue_T *)pValue; /* 获取整数指针 */
    pItem->minValue.uint32Value = Min;
    pItem->maxValue.uint32Value = Max;
    pItem->disableShowNumberFlag = FALSE;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加无符号32位整数值菜单项(无存储功能)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @param Min 最小值
 * @param Max 最大值
 * @param enter 自定义进入函数
 * @param exit 自定义退出函数
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateUintItem32F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue, uint32_t Min, uint32_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem32(baseUI, pMenu, title, pValue, Min, Max);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加无符号32位整数值菜单项(只读)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateUintItem32R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem32(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_READONLY;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加无符号32位整数值菜单项(清零)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateUintItem32Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue, uint32_t setDefaultValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem32(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_SET_VALUE;
    pItem->setDefaultValue.uint32Value = setDefaultValue;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加无符号32位整数值菜单项(清零带回调函数)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @param pFunc 指向函数的指针
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateUintItem32ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue, uint32_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc)
{
    /* 添加菜单项 */
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem32Z(baseUI, pMenu, title, pValue, setDefaultValue);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);

    /* 设置菜单项的类型循环执行 */
    newItem->customEnter = pFunc;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加无符号16位整数值菜单项(无存储功能)
 * @param baseUI 指向菜单设备的指针
 * @param pMenu 目标菜单指针
 * @param title 菜单标题
 * @param pValue 指向整数值变量的指针
 * @param Min 最小值
 * @param Max 最大值
 * @return GUI_BaseItem_T* 返回新创建的菜单项指针
 */
GUI_ValueItem_T *GUI_CreateUintItem16(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue, uint16_t Min, uint16_t Max)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ValueItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    /* 将基础菜单项转换为值修改菜单项指针 */
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    /* 处理浮点数类型 */
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_VALUE;
    pItem->valueType = GUI_MENU_VALUE_TYPE_UINT16;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_CHANGE;
    pItem->parent.drawDetail = valueItemDrawDetail;
    pItem->parent.enter = valueItemEnter;
    pItem->parent.exit = valueItemExit;
    pItem->parent.change = valueItemChange;
    pItem->pValue = (GUI_StoreValue_T *)pValue; /* 获取整数指针 */
    pItem->minValue.uint16Value = Min;
    pItem->maxValue.uint16Value = Max;
    pItem->disableShowNumberFlag = FALSE;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 为无符号 16 位值项添加带自定义回调的简易包装（F）。
 *
 * 基于 GUI_CreateUintItem16 创建项并设置 customEnter 与 customExit 回调。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 uint16_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @param enter 进入事件的自定义回调（可为 NULL）。
 * @param exit 退出事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateUintItem16F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue, uint16_t Min, uint16_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem16(baseUI, pMenu, title, pValue, Min, Max);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 为无符号 16 位值项添加只读简易包装（R）。
 *
 * 基于 GUI_CreateUintItem16 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_READONLY。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示的 uint16_t 值的指针（只读）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateUintItem16R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem16(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_READONLY;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 为无符号 16 位值项添加“零”行为的简易包装（Z）。
 *
 * 基于 GUI_CreateUintItem16 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_ZERO。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 uint16_t 值的指针。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateUintItem16Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue, uint16_t setDefaultValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem16(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_SET_VALUE;
    pItem->setDefaultValue.uint16Value = setDefaultValue;

    return (GUI_ValueItem_T *)newItem;
}
/**
 * @brief 在 GUI_CreateUintItem16Z 的基础上设置 customEnter（pFunc）。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 uint16_t 值的指针。
 * @param pFunc 进入事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateUintItem16ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue, uint16_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem16Z(baseUI, pMenu, title, pValue, setDefaultValue);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = pFunc;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位无符号整型值项到指定菜单（基础实现）。
 *
 * 创建并初始化一个 GUI_ValueItem_T，设置 valueType 为 GUI_MENU_VALUE_TYPE_UINT8，
 * 默认更新模式为 GUI_MENU_VALUE_UPDATE_CHANGE，并绑定默认绘制、进入、退出、改变回调。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题（以 null 结尾的字符串）。
 * @param pValue 指向要显示/编辑的 uint8_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateUintItem8(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue, uint8_t Min, uint8_t Max)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ValueItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;

    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_VALUE;
    pItem->valueType = GUI_MENU_VALUE_TYPE_UINT8;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_CHANGE;
    pItem->parent.drawDetail = valueItemDrawDetail;
    pItem->parent.enter = valueItemEnter;
    pItem->parent.exit = valueItemExit;
    pItem->parent.change = valueItemChange;
    pItem->pValue = (GUI_StoreValue_T *)pValue;
    pItem->minValue.uint8Value = Min;
    pItem->maxValue.uint8Value = Max;
    pItem->disableShowNumberFlag = FALSE;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位无符号整型值项并指定自定义进入/退出回调。
 *
 * 在 GUI_CreateUintItem8 的基础上设置 customEnter 和 customExit 回调。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 uint8_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @param enter 进入事件的自定义回调（可为 NULL）。
 * @param exit 退出事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */

GUI_ValueItem_T *GUI_CreateUintItem8F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue, uint8_t Min, uint8_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem8(baseUI, pMenu, title, pValue, Min, Max);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位无符号整型只读值项。
 *
 * 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_READONLY，禁止 UI 修改。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示的 uint8_t 值的指针（只读）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateUintItem8R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem8(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_READONLY;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位无符号整型“零”行为值项。
 *
 * 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_ZERO，用于特殊的零值更新策略。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 uint8_t 值的指针。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateUintItem8Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue, uint8_t setDefaultValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem8(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_SET_VALUE;
    pItem->setDefaultValue.uint8Value = setDefaultValue;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位无符号整型“零”行为值项并指定进入回调。
 *
 * 在 GUI_CreateUintItem8Z 的基础上设置 customEnter（pFunc）。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 uint8_t 值的指针。
 * @param pFunc 进入事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateUintItem8ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue, uint8_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateUintItem8Z(baseUI, pMenu, title, pValue, setDefaultValue);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = pFunc;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 32 位有符号整型值项到指定菜单（基础实现）。
 *
 * 创建并初始化一个 GUI_ValueItem_T 并将其作为值项添加到菜单中。
 * 设置项类型为 GUI_MENU_ITEM_TYPE_VALUE，值类型为 GUI_MENU_VALUE_TYPE_INT32，
 * 默认更新模式为 GUI_MENU_VALUE_UPDATE_CHANGE，并绑定默认绘制、进入、退出、改变回调。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题（以 null 结尾的字符串）。
 * @param pValue 指向要显示/编辑的 int32_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem32(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue, int32_t Min, int32_t Max)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ValueItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;

    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_VALUE;
    pItem->valueType = GUI_MENU_VALUE_TYPE_INT32;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_CHANGE;
    pItem->parent.drawDetail = valueItemDrawDetail;
    pItem->parent.enter = valueItemEnter;
    pItem->parent.exit = valueItemExit;
    pItem->parent.change = valueItemChange;
    pItem->pValue = (GUI_StoreValue_T *)pValue;
    pItem->minValue.int32Value = Min;
    pItem->maxValue.int32Value = Max;
    pItem->disableShowNumberFlag = FALSE;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 32 位有符号整型值项并指定自定义进入/退出回调。
 *
 * 在 GUI_CreateIntItem32 的基础上设置 customEnter 和 customExit 回调。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int32_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @param enter 进入事件的自定义回调（可为 NULL）。
 * @param exit 退出事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem32F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue, int32_t Min, int32_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem32(baseUI, pMenu, title, pValue, Min, Max);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 32 位有符号整型只读值项。
 *
 * 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_READONLY，禁止在 UI 中修改该值。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示的 int32_t 值的指针（只读）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */

GUI_ValueItem_T *GUI_CreateIntItem32R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem32(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_READONLY;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 32 位有符号整型“零”行为值项。
 *
 * 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_ZERO，表示特殊的“零值”更新策略（实现依赖内部逻辑）。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int32_t 值的指针。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem32Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue, int32_t setDefaultValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem32(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_SET_VALUE;
    pItem->setDefaultValue.int32Value = setDefaultValue;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 32 位有符号整型“零”行为值项并指定进入回调。
 *
 * 在 GUI_CreateIntItem32Z 的基础上设置 customEnter（pFunc）。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int32_t 值的指针。
 * @param pFunc 进入事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem32ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue, int32_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem32Z(baseUI, pMenu, title, pValue, setDefaultValue);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = pFunc;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 16 位有符号整型值项到指定菜单（基础实现）。
 *
 * 与 GUI_CreateIntItem32 类似，但针对 int16_t 类型，并将 valueType 设置为 GUI_MENU_VALUE_TYPE_INT16。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int16_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */

GUI_ValueItem_T *GUI_CreateIntItem16(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue, int16_t Min, int16_t Max)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ValueItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;

    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_VALUE;
    pItem->valueType = GUI_MENU_VALUE_TYPE_INT16;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_CHANGE;
    pItem->parent.drawDetail = valueItemDrawDetail;
    pItem->parent.enter = valueItemEnter;
    pItem->parent.exit = valueItemExit;
    pItem->parent.change = valueItemChange;
    pItem->pValue = (GUI_StoreValue_T *)pValue;
    pItem->minValue.int16Value = Min;
    pItem->maxValue.int16Value = Max;
    pItem->disableShowNumberFlag = FALSE;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 16 位有符号整型值项并指定自定义进入/退出回调。
 *
 * 在 GUI_CreateIntItem16 的基础上设置 customEnter 和 customExit 回调。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int16_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @param enter 进入事件的自定义回调（可为 NULL）。
 * @param exit 退出事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem16F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue, int16_t Min, int16_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem16(baseUI, pMenu, title, pValue, Min, Max);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 16 位有符号整型只读值项。
 *
 * 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_READONLY，禁止 UI 修改。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示的 int16_t 值的指针（只读）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem16R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem16(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_READONLY;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 16 位有符号整型“零”行为值项。
 *
 * 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_ZERO，用于特殊的零值更新策略。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int16_t 值的指针。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */

GUI_ValueItem_T *GUI_CreateIntItem16Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue, int16_t setDefaultValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem16(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_SET_VALUE;
    pItem->setDefaultValue.int16Value = setDefaultValue;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 16 位有符号整型“零”行为值项并指定进入回调。
 *
 * 在 GUI_CreateIntItem16Z 的基础上设置 customEnter（pFunc）。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int16_t 值的指针。
 * @param pFunc 进入事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem16ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue, int16_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem16Z(baseUI, pMenu, title, pValue, setDefaultValue);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = pFunc;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位有符号整型值项到指定菜单（基础实现）。
 *
 * 与更高位数的版本类似，但针对 int8_t，valueType 设置为 GUI_MENU_VALUE_TYPE_INT8。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int8_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem8(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue, int8_t Min, int8_t Max)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ValueItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;

    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_VALUE;
    pItem->valueType = GUI_MENU_VALUE_TYPE_INT8;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_CHANGE;
    pItem->parent.drawDetail = valueItemDrawDetail;
    pItem->parent.enter = valueItemEnter;
    pItem->parent.exit = valueItemExit;
    pItem->parent.change = valueItemChange;
    pItem->pValue = (GUI_StoreValue_T *)pValue;
    pItem->minValue.int8Value = Min;
    pItem->maxValue.int8Value = Max;
    pItem->disableShowNumberFlag = FALSE;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位有符号整型值项并指定自定义进入/退出回调。
 *
 * 在 GUI_CreateIntItem8 的基础上设置 customEnter 和 customExit 回调。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int8_t 值的指针。
 * @param Min 可接受的最小值（包含）。
 * @param Max 可接受的最大值（包含）。
 * @param enter 进入事件的自定义回调（可为 NULL）。
 * @param exit 退出事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem8F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue, int8_t Min, int8_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem8(baseUI, pMenu, title, pValue, Min, Max);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = enter;
    newItem->customExit = exit;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位有符号整型只读值项。
 *
 * 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_READONLY，禁止 UI 修改。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示的 int8_t 值的指针（只读）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem8R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem8(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_READONLY;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位有符号整型“零”行为值项。
 *
 * 创建项并将 updateType 设置为 GUI_MENU_VALUE_UPDATE_ZERO，用于特殊的零值更新策略。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int8_t 值的指针。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem8Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue, int8_t setDefaultValue)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem8(baseUI, pMenu, title, pValue, 0, 0);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    GUI_ValueItem_T *pItem = (GUI_ValueItem_T *)newItem;
    pItem->updateType = GUI_MENU_VALUE_UPDATE_SET_VALUE;
    pItem->setDefaultValue.int8Value = setDefaultValue;

    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 添加一个 8 位有符号整型“零”行为值项并指定进入回调。
 *
 * 在 GUI_CreateIntItem8Z 的基础上设置 customEnter（pFunc）。
 *
 * @param baseUI 指向 GUI 设备/上下文的指针。
 * @param pMenu 指向要添加项的菜单结构指针。
 * @param title 项显示标题。
 * @param pValue 指向要显示/编辑的 int8_t 值的指针。
 * @param pFunc 进入事件的自定义回调（可为 NULL）。
 * @return 成功返回新创建的 GUI_BaseItem_T*，失败返回 NULL。
 */
GUI_ValueItem_T *GUI_CreateIntItem8ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue, int8_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateIntItem8Z(baseUI, pMenu, title, pValue, setDefaultValue);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);
    newItem->customEnter = pFunc;
    return (GUI_ValueItem_T *)newItem;
}

/**
 * @brief 创建模式菜单项
 * @param baseUI GUI设备指针
 * @param pMenu 目标菜单
 * @param title 菜单标题
 * @param elements 模式元素数组
 * @param modeCount 模式数量
 * @param currentMode 当前模式指针
 * @return 菜单项指针
 */
GUI_ModeItem_T *GUI_CreateModeItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint32_t *value)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ModeItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);

    GUI_ModeItem_T *pItem = (GUI_ModeItem_T *)newItem;

    /* 设置基本属性 */
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_MODE;
    pItem->parent.drawDetail = modeItemDrawDetail;
    pItem->parent.enter = modeItemEnter;
    pItem->parent.exit = modeItemExit;
    pItem->parent.change = modeItemChange;
    pItem->parent.selectable = TRUE;

    /* 设置模式相关属性 */
    pItem->elements = elements;
    pItem->modeIndex = 0;
    pItem->valuePtr = value;
    pItem->readOnly = FALSE;
    pItem->isShortValue = FALSE;

    /* 计算项目个数 */
    pItem->modeCount = 0;
    while (elements[pItem->modeCount].Name != NULL)
    {
        pItem->modeCount++;
    }

    return (GUI_ModeItem_T *)newItem;
}

/**
 * @brief 创建带自定义回调的模式菜单项
 * @param baseUI GUI设备指针
 * @param pMenu 目标菜单
 * @param title 菜单标题
 * @param elements 模式元素数组
 * @param modeCount 模式数量
 * @param value 当前模式指针
 * @param pFunc 自定义进入回调函数
 * @return 菜单项指针
 */
GUI_ModeItem_T *GUI_CreateModeItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint32_t *value, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateModeItem(baseUI, pMenu, title, elements, value);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);

    /* 设置自定义回调函数 */
    newItem->customEnter = pFunc;

    return (GUI_ModeItem_T *)newItem;
}

/**
 * @brief 创建带自定义回调的模式菜单项(只读)
 * @param baseUI GUI设备指针
 * @param pMenu 目标菜单
 * @param title 菜单标题
 * @param elements 模式元素数组
 * @param modeCount 模式数量
 * @param value 当前模式指针
 * @param pFunc 自定义进入回调函数
 * @return 菜单项指针
 */
GUI_ModeItem_T *GUI_CreateModeItemR(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint32_t *value)
{
    GUI_ModeItem_T *newItem = GUI_CreateModeItem(baseUI, pMenu, title, elements, value);
    newItem->readOnly = TRUE;
    return newItem;
}

/**
 * @brief 创建模式菜单项
 * @param baseUI GUI设备指针
 * @param pMenu 目标菜单
 * @param title 菜单标题
 * @param elements 模式元素数组
 * @param modeCount 模式数量
 * @param currentMode 当前模式指针
 * @return 菜单项指针
 */
GUI_ModeItem_T *GUI_CreateModeItem8(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint8_t *value)
{
    GUI_BaseItem_T *newItem = GUI_CreateBaseItem(baseUI, pMenu, (char *)title, sizeof(GUI_ModeItem_T));
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);

    GUI_ModeItem_T *pItem = (GUI_ModeItem_T *)newItem;

    /* 设置基本属性 */
    pItem->parent.itemType = GUI_MENU_ITEM_TYPE_MODE;
    pItem->parent.drawDetail = modeItemDrawDetail;
    pItem->parent.enter = modeItemEnter;
    pItem->parent.exit = modeItemExit;
    pItem->parent.change = modeItemChange;
    pItem->parent.selectable = TRUE;

    /* 设置模式相关属性 */
    pItem->elements = elements;
    pItem->modeIndex = 0;
    pItem->valuePtr = (uint32_t *)value;
    pItem->readOnly = FALSE;
    pItem->isShortValue = TRUE;

    /* 计算项目个数 */
    pItem->modeCount = 0;
    while (elements[pItem->modeCount].Name != NULL)
    {
        pItem->modeCount++;
    }

    return (GUI_ModeItem_T *)newItem;
}

/**
 * @brief 创建带自定义回调的模式菜单项
 * @param baseUI GUI设备指针
 * @param pMenu 目标菜单
 * @param title 菜单标题
 * @param elements 模式元素数组
 * @param modeCount 模式数量
 * @param value 当前模式指针
 * @param pFunc 自定义进入回调函数
 * @return 菜单项指针
 */
GUI_ModeItem_T *GUI_CreateModeItem8F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint8_t *value, GUI_MenuDefaultEventCallback_T pFunc)
{
    GUI_BaseItem_T *newItem = (GUI_BaseItem_T *)GUI_CreateModeItem8(baseUI, pMenu, title, elements, value);
    GUI_ASSERT_WITH_RETURN(newItem != NULL, NULL);

    /* 设置自定义回调函数 */
    newItem->customEnter = pFunc;

    return (GUI_ModeItem_T *)newItem;
}

/**
 * @brief 创建带自定义回调的模式菜单项(只读)
 * @param baseUI GUI设备指针
 * @param pMenu 目标菜单
 * @param title 菜单标题
 * @param elements 模式元素数组
 * @param modeCount 模式数量
 * @param value 当前模式指针
 * @param pFunc 自定义进入回调函数
 * @return 菜单项指针
 */
GUI_ModeItem_T *GUI_CreateModeItem8R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint8_t *value)
{
    GUI_ModeItem_T *newItem = GUI_CreateModeItem8(baseUI, pMenu, title, elements, value);
    newItem->readOnly = TRUE;
    return newItem;
}
