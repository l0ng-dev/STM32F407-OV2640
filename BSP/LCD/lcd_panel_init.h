/** @file lcd_panel_init.h
 * @brief 各 LCD 控制器的寄存器初始化入口，由 lcd.c 根据识别结果调用。
 */
#ifndef BSP_LCD_PANEL_INIT_H
#define BSP_LCD_PANEL_INIT_H

#include "lcd.h"

void lcd_ex_st7789_reginit(void);
void lcd_ex_st7796_reginit(void);
void lcd_ex_ili9341_reginit(void);
void lcd_ex_nt35310_reginit(void);
void lcd_ex_nt35510_reginit(void);
void lcd_ex_ssd1963_reginit(void);
void lcd_ex_ili9806_reginit(void);

#endif
