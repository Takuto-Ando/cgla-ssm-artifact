#define main historical_suite_main
#include "vendor/hardware_suite/alice110_comprehensive_suite.c"
#undef main
#include "../bitnet/dispatch.c"
#include "../ssm/projection_bounded.inc"
#include "../ssm/state_bounded.inc"
#include "../ntt/split_product_bounded.inc"
#include "../ntt/ntt.c"
#include "ssm_pack_cache.h"
#include "ssm_config_reuse.h"
extern int emax7_conf_dense_tile(void);
extern int emax7_conf_nj_ordered_scan(void);
#define SSM_DENSE_CONFIG ((const void *)(uintptr_t)emax7_conf_dense_tile)
#define SSM_STATE_CONFIG ((const void *)(uintptr_t)emax7_conf_nj_ordered_scan)
#define SSM_CONFIG_BYTES (sizeof(struct conf)*64*4)
_Static_assert(SSM_CONFIG_BYTES == 8192, "SSM CONF image size");
static struct execution_context nj_context;
static int nj_opened;
int nj_session_open(void) {
    const char *scope=getenv("H_RUN_APPROVED"),*fd=getenv("H_LOCK_FD");
    struct stat lease,expected,directory;
    if(nj_opened || !scope || strcmp(scope,"no-reset-v1") || !fd)return 78;
    if(fstat(atoi(fd),&lease)||stat("/tmp/cgla-emax5a.lock",&expected)||lease.st_ino!=expected.st_ino||lease.st_dev!=expected.st_dev||stat("/tmp/alice110-emax.lock",&directory)||directory.st_uid!=getuid())return 78;
    nj_pack_cache_invalidate();ssm_config_invalidate();
    Ull ddr=h_init_lane1();
    nj_context=(struct execution_context){0};
    nj_context.arena=(unsigned char *)(uintptr_t)(ddr+0x40000ULL);
    nj_context.arena_bytes=nj_context.bank_bytes=65536;
    nj_context.ddr_phys=emax_info[1].ddr_phys+0x40000ULL;
    nj_context.lmm_phys=emax_info[1].lmm_phys+0x40000ULL;
    nj_context.backend=BACKEND_NATIVE;nj_context.authorized=nj_context.initialized=1;
    nj_context.coherent=COHERENT_UNCACHED;nj_context.sync=suite_sync;nj_context.invoke=suite_invoke;
    nj_opened=1;return 0;
}
int nj_native_projection(const int8_t *a,const int8_t *w,int rows,int cols,int32_t *out,unsigned *fallbacks){
    if(!nj_opened)return 78;
    int rc=nj_bitnet(&nj_context,a,w,rows,cols,out,fallbacks);
    if(rc)nj_opened=0;return rc;
}
int nj_metrics(uint64_t out[5]){
    if(!nj_opened)return 78;
    out[0]=nj_context.metrics.conf_ns;out[1]=nj_context.metrics.load_ns;
    out[2]=nj_context.metrics.exec_ns;out[3]=nj_context.metrics.drain_ns;out[4]=nj_context.metrics.completed_calls;return 0;
}
/* Library entry has no implicit open or constructor; admission is mandatory. */
#include "ssm_dense_baseline.h"
#include "ssm_input_broadcast.h"
#include "ssm_state.h"
static int nj_mod(int64_t x){int r=x%3329;return r<0?r+3329:r;}
static int nj_native_digit_modmul(const int16_t *a,const int16_t *b,int16_t *out,int n){
 if(!nj_opened)return 78;if(!a||!b||!out||n<1||n>256)return -1;
 Ull base=(Ull)(uintptr_t)nj_context.arena,buf[5];int padded=(n+1)&~1;
 for(int p=0;p<5;p++)buf[p]=base+(Ull)(p+1)*65536;
 for(int j=0;j<n;j++){float values[3]={(float)nj_mod(a[j]),(float)(nj_mod(b[j])&255),(float)(nj_mod(b[j])>>8)};for(int v=0;v<3;v++){uint32_t bits;memcpy(&bits,&values[v],4);((volatile Ull *)(uintptr_t)buf[v])[j]=bits;}}
 h_barrier();h_arm_deadline();reset_nanosec(1);emax7[1].last_conf=0;
 nj_split_product(1,(Ull *)(uintptr_t)buf[0],(Ull *)(uintptr_t)buf[1],(Ull *)(uintptr_t)buf[2],(Ull *)(uintptr_t)buf[3],(Ull *)(uintptr_t)buf[4],n);h_barrier();
 if(((volatile struct reg_ctrl *)(uintptr_t)emax7[1].reg_ctrl)->i[0].stat & 0xffff00ff){nj_opened=0;return -4;}
 for(int j=0;j<n;j++){float low=((volatile float *)(uintptr_t)buf[3])[2*j],high=((volatile float *)(uintptr_t)buf[4])[2*j];if(!isfinite(low)||!isfinite(high)||low<0||low>848640||high<0||high>43264||low!=(int)low||high!=(int)high){
 uint32_t lo_bits,hi_bits;memcpy(&lo_bits,&low,4);memcpy(&hi_bits,&high,4);
 fprintf(stderr,"NJ_NTT_PARTIAL_REJECT index=%d a=%d b=%d low=%.9g high=%.9g low_bits=%08x high_bits=%08x expected_low=%d expected_high=%d CONF=%llu LOAD=%llu EXEC=%llu DRAIN=%llu\n",j,(int)a[j],(int)b[j],low,high,lo_bits,hi_bits,nj_mod(a[j])*(nj_mod(b[j])&255),nj_mod(a[j])*(nj_mod(b[j])>>8),(unsigned long long)nanosec[1][NANOS_CONF],(unsigned long long)nanosec[1][NANOS_LOAD],(unsigned long long)nanosec[1][NANOS_EXEC],(unsigned long long)nanosec[1][NANOS_DRAIN]);
 nj_opened=0;return -5;}out[j]=nj_mod((int64_t)(int)low+256LL*(int)high);}
 nj_context.metrics.conf_ns+=nanosec[1][NANOS_CONF];nj_context.metrics.load_ns+=nanosec[1][NANOS_LOAD];nj_context.metrics.exec_ns+=nanosec[1][NANOS_EXEC];nj_context.metrics.drain_ns+=nanosec[1][NANOS_DRAIN];nj_context.metrics.completed_calls++;return 0;
}
/* Exact digit products: each input <=15, result <=225. No rounding recovery. */
int nj_native_modmul(const int16_t *a,const int16_t *b,int16_t *out,int n){
 if(!nj_opened)return 78;if(!a||!b||!out||n<1||n>256)return -1;
 int64_t sum[256]={0};int16_t da[256],db[256],digit[256];
 for(int x=0;x<3;x++)for(int y=0;y<3;y++){
  for(int i=0;i<n;i++){da[i]=(nj_mod(a[i])>>(4*x))&15;db[i]=(nj_mod(b[i])>>(4*y))&15;}
  int rc=nj_native_digit_modmul(da,db,digit,n);if(rc)return rc;
  for(int i=0;i<n;i++)sum[i]+=(int64_t)digit[i]<<(4*(x+y));
 }
 for(int i=0;i<n;i++)out[i]=nj_mod(sum[i]);return 0;
}
#include "../ntt/native_schedule.inc"

int nj_backend_kind(void){return BACKEND_NATIVE;}

void nj_session_close(void){nj_pack_cache_invalidate();ssm_config_invalidate();nj_opened=0;}
