#ifndef __APP_FRAME_H
#define __APP_FRAME_H
#include "main.h"
int app_fmt_fixed2(char *dst,int32_t v_x100);
int app_frame_json(char *out,uint16_t out_sz,
					uint32_t seq,int32_t t_x100,int32_t h_x100);
#endif