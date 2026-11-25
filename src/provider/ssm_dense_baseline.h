#include "ssm_projection_batch.h"
/* 4 rows per call; K tiles preserve scalar-order FP32 accumulator seeds. */
static int nj_dense_baseline(const float *weights,const float *input,float *output,int rows,int columns){
    if(!nj_opened)return 78;
    if(!weights||!input||!output||rows<1||columns<1||columns>1048576)return -1;
    const float *packed=ssm_pack_lookup(weights,rows,columns);
    const int pack_options=ssm_pack_stream_options();
    const int tile_columns=ssm_projection_tile_columns();
    const int lane=1;Ull base=(Ull)(uintptr_t)nj_context.arena;
    Ull x[1]={base+0x10000},w0[1]={base+0x20000},w1[1]={base+0x30000},y0[1]={base+0x40000},y1[1]={base+0x50000};
    for(int r=0;r<rows;r+=4){
      Ull acc0=0,acc1=0;
      for(int b=0;b<columns;b+=tile_columns){
        int n=columns-b<tile_columns?columns-b:tile_columns;
        volatile float *xx=(volatile float *)(uintptr_t)x[0],*ww0=(volatile float *)(uintptr_t)w0[0],*ww1=(volatile float *)(uintptr_t)w1[0];
        if(packed){
          ssm_pack_stage(pack_options,packed,weights,rows,columns,r,b,n,ww0,ww1,input,xx);
        }else{for(int j=0;j<n;j++){xx[j]=input[b+j];for(int p=0;p<2;p++){ww0[2*j+p]=r+p<rows?weights[(size_t)(r+p)*columns+b+j]:0;ww1[2*j+p]=r+2+p<rows?weights[(size_t)(r+2+p)*columns+b+j]:0;}}}
        h_barrier();h_arm_deadline();ssm_config_prepare(lane,SSM_DENSE_CONFIG,SSM_CONFIG_BYTES);reset_nanosec(lane);
        dense_tile(lane,x,w0,acc0,y0,w1,acc1,y1,(Ull)n*2,(Ull)n);
        h_barrier();
        if(((volatile struct reg_ctrl *)(uintptr_t)emax7[lane].reg_ctrl)->i[0].stat & 0xffff00ff){nj_opened=0;return -4;}
        acc0=*(volatile Ull *)(uintptr_t)y0[0];acc1=*(volatile Ull *)(uintptr_t)y1[0];
        nj_context.metrics.conf_ns+=nanosec[lane][NANOS_CONF];nj_context.metrics.load_ns+=nanosec[lane][NANOS_LOAD];nj_context.metrics.exec_ns+=nanosec[lane][NANOS_EXEC];nj_context.metrics.drain_ns+=nanosec[lane][NANOS_DRAIN];nj_context.metrics.completed_calls++;
      }
      float values[4];memcpy(values,&acc0,8);memcpy(values+2,&acc1,8);
      for(int p=0;p<4&&r+p<rows;p++)output[r+p]=values[p];
    }return 0;
}
