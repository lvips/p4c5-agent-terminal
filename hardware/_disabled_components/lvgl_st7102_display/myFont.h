/**
 * @file myFont.h
 * @brief 自定义中文字体声明头文件
 *
 * 声明由 Lvgl Font Tool 生成的中文字体变量，
 * 供 LVGL 界面模块引用。
 *
 * @copyright Copyright (c) 2024 酷世DIY
 */

#ifndef MYFONT_H
#define MYFONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#if LVGL_VERSION_MAJOR >= 8
extern const lv_font_t myFont_24;
#else
extern lv_font_t myFont_24;
#endif

/** 与 myFont.c 中 `myFont_24` 同义，便于界面代码写 `&myFont` */
#define myFont myFont_24
/*
宏展开为，须传指针（与一致）典型换算：无电池充中已满放量芯片完成有是否源通道读取状态设定压关陀螺仪轴显示数据卡尔曼滤波三姿角触摸共用。初始化…度°加速未检测到磁仅←采样融合器形感就绪矫正窗宽约系统信息页面实现包括号和修订版本频率核心可堆内存最小大编译日期时间备扫描结果酷世志标签声明自义文字体返回按钮点击事件调对象构建收集、等格式输出缓冲区获节嵌外部创界紫色题栏滚动的顶容销毁例音键综硬连接短长下降沿上升平低解码应答扬试值写入两路；轮询更新历史记录查将使当前追清空重绿操作提域待启预览摄像头失败主菜单竖屏多列网项暂目语识别功能说唤醒词命令表在后台持续运行户嗨乐鑫方法先听打闭增风减高制热模冷送除湿健康睡眠蓝牙播停灯桥请插你好呀我细线默认拐偏安装向同焊背相全镜水静置朝幕丁效找力粒子切驱倾斜演右计板坐反峰≈绕旋转观察变止异常±留白估扩储机管缺必需发分步避免任务占保证至少缩裁剪底尺寸固画幅图侧不整帧做比或直绘齐则再丢刷走

*/
#ifdef __cplusplus
}
#endif

#endif /* MYFONT_H */
