#include "bsp_oled.h"
#include "bsp_dwt.h"
#include "oledfont.h"

static i2c_soft_t *oled_bus;
static uint8_t oled_gram[8][128];
static uint8_t oled_dirty[8];


static void oled_write_cmd(uint8_t cmd){
    i2c_soft_start(oled_bus);
    i2c_soft_send_byte(oled_bus,OLED_ADDR);
    i2c_soft_send_byte(oled_bus,0x00);      //0x00表示后面是命令
    i2c_soft_send_byte(oled_bus,cmd);
    i2c_soft_stop(oled_bus);
}

static void oled_write_data(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    i2c_soft_start(oled_bus);
    i2c_soft_send_byte(oled_bus, OLED_ADDR);
    i2c_soft_send_byte(oled_bus, 0x40);     //0x40表示后面是数据 
    for (i = 0; i < len; i++)
    {
        i2c_soft_send_byte(oled_bus, buf[i]);
    }
    i2c_soft_stop(oled_bus);
}

void oled_clear(void)
{
    uint8_t page, x;
    for (page = 0; page < 8; page++)
        for (x = 0; x < 128; x++){
            oled_gram[page][x] = 0x00;
        }
        oled_dirty[page] = 1;  /* 全部标记为脏页 */
}

void oled_refresh(void)
{
    uint8_t page;
    for (page = 0; page < 8; page++)
    {
        if(oled_dirty[page] == 0) continue;  /* 跳过干净页 */
        oled_dirty[page] = 0;  /* 清除脏页标记 */
        oled_write_cmd(0xB0 | page);      /* 页地址 */
        oled_write_cmd(0x00);             /* 列低 4 位 = 0 */
        oled_write_cmd(0x10);             /* 列高 4 位 = 0 */
        oled_write_data(oled_gram[page], 128);
    }
}
void oled_init(i2c_soft_t *bus)
{
    oled_bus = bus;

    dwt_delay_ms(100);          /* 上电等一等 */

    oled_write_cmd(0xAE);                            /* 关显示 */
    oled_write_cmd(0xD5); oled_write_cmd(0x80);      /* 时钟分频 */
    oled_write_cmd(0xA8); oled_write_cmd(0x3F);      /* 多路复用 64 行 */
    oled_write_cmd(0xD3); oled_write_cmd(0x00);      /* 显示偏移 0 */
    oled_write_cmd(0x40);                            /* 起始行 0 */
    oled_write_cmd(0x8D); oled_write_cmd(0x14);      /* 电荷泵使能 */
    oled_write_cmd(0x20); oled_write_cmd(0x02);      /* 页寻址模式 */
    oled_write_cmd(0xA1);                            /* 段重映射 */
    oled_write_cmd(0xC8);                            /* COM 扫描方向 */
    oled_write_cmd(0xDA); oled_write_cmd(0x12);      /* COM 引脚配置 */
    oled_write_cmd(0x81); oled_write_cmd(0xCF);      /* 对比度 */
    oled_write_cmd(0xD9); oled_write_cmd(0xF1);      /* 预充电周期 */
    oled_write_cmd(0xDB); oled_write_cmd(0x40);      /* VCOMH */
    oled_write_cmd(0xA4);                            /* 输出跟随 RAM */
    oled_write_cmd(0xA6);                            /* 正常显示（非反色）*/
    oled_write_cmd(0xAF);                            /* 开显示 */

    oled_clear();
    oled_refresh();
}
void oled_draw_point(uint8_t x, uint8_t y, uint8_t color){
    uint8_t page,bit;
    if(x>127||y>63)return;  
    page=y/8;
    bit=y%8;

    if(color)oled_gram[page][x]|=1<<bit;
    else oled_gram[page][x]&=~(1<<bit);
    oled_dirty[page] = 1;  /* 标记脏页 */
}
//照抄官方算法
void oled_show_char(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode){
    uint8_t temp, t, t1;
    uint8_t y0 = y;
    const uint8_t *pfont;
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);

    if (chr < ' ' || chr > '~') return;    /* 只支持可见 ASCII */
    chr -= ' ';                             /* 字库从空格开始存 */

    if (size == 12)      pfont = (const uint8_t *)oled_asc2_1206[(uint8_t)chr];
    else if (size == 16) pfont = (const uint8_t *)oled_asc2_1608[(uint8_t)chr];
    else if (size == 24) pfont = (const uint8_t *)oled_asc2_2412[(uint8_t)chr];
    else return;

    for (t = 0; t < csize; t++)
    {
        temp = pfont[t];
        for (t1 = 0; t1 < 8; t1++)
        {
            oled_draw_point(x, y, (temp & 0x80) ? mode : (uint8_t)!mode);
            temp <<= 1;
            y++;
            if ((uint8_t)(y - y0) == size)   /* 满 size 行 → 换下一列 */
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}

void oled_show_string(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    while (*str >= ' ' && *str <= '~')
    {
        if (x > (uint8_t)(128 - size / 2)) { x = 0; y += size; }      /* 自动换行 */
        if (y > (uint8_t)(64 - size))      { x = 0; y = 0; oled_clear(); }

        oled_show_char(x, y, *str, size, 1);
        x += size / 2;              /* 字符宽度 = size/2 */
        str++;
    }
}

static uint32_t oled_pow(uint8_t m, uint8_t n)
{
    uint32_t r = 1;
    while (n--) r *= m;
    return r;
}

void oled_show_num(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
    uint8_t t, temp, enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / oled_pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0) { oled_show_char(x + (size / 2) * t, y, ' ', size, 1); continue; }
            else enshow = 1;
        }
        oled_show_char(x + (size / 2) * t, y, (char)(temp + '0'), size, 1);
    }
}