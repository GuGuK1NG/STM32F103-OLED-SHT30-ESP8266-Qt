#include "app_frame.h"
#include <stdio.h>

int app_fmt_fixed2(char *dst,int32_t v_x100){
	uint32_t a;
	int n=0;
	if(v_x100<0){
		dst[n++]='-';
		a=(uint32_t)(-v_x100);
	}
	else{
		a=(uint32_t)v_x100;
	}
	n+=snprintf(&dst[n],12,"%lu.%02lu",(unsigned long)(a/100),(unsigned long)(a%100));
	return n;
}
int app_frame_json(char *out,uint16_t out_sz,
					uint32_t seq,int32_t t_x100,int32_t h_x100)
{
	if(out_sz<48){
		return -1;
	}
	int n = snprintf(out,out_sz,"{\"seq\":%lu,\"t\":",(unsigned long)seq);
	if(n<0){
		return -1;
	}
	n+=app_fmt_fixed2(&out[n],t_x100);
	n+= snprintf(&out[n], 8, ",\"h\":");
    n+= app_fmt_fixed2(&out[n], h_x100);
	
	if((uint16_t)(n+3)>out_sz){
		return -1;
	}
	out[n++]='}';
	out[n++]='\n';
	out[n]='\0';
	
	return n;
}