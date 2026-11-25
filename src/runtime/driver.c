#define TESTING
#include <stdio.h>
#include <assert.h>
#include <sys/resource.h>
#include "memory_profile.h"
#include "model_runtime.c"
#include "batch.h"
#include "scheduler.h"
#include "sequence.h"

static void emit(FILE *f,const void *p,size_t n) { if (f && fwrite(p,1,n,f)!=n) exit(7); }
static void pack_state(MContext *ctx,int lane,void *dest,int compare)
{
    Config *p=&ctx->lane[0].config; RunState *s=&ctx->lane[lane].state;
    size_t a=(size_t)p->n_layers*p->d_inner*p->d_state*4,b=(size_t)p->n_layers*p->d_inner*p->d_conv*4,c=(size_t)p->rounded_vocab_size*4;
    if (compare) { assert(!memcmp(dest,s->ssm_state,a)); assert(!memcmp((char *)dest+a,s->conv_state,b)); assert(!memcmp((char *)dest+a+b,s->logits,c)); }
    else { memcpy(dest,s->ssm_state,a); memcpy((char *)dest+a,s->conv_state,b); memcpy((char *)dest+a+b,s->logits,c); }
}
static void seed(OSequence *s,char *model,int B,int policy,int ragged,int cap,int ids[8][512],int *length)
{
    memset(s,0,sizeof(*s)); memset(s->tokens,-1,sizeof(s->tokens));
    s->ctx=m_create(model,B,policy==0 ? 0 : 1); assert(s->ctx);
    for (int lane=0;lane<B;++lane) {
        s->owners[lane]=s->ctx->owner[lane]; s->versions[lane]=s->ctx->generation[lane];
        s->limit[lane]=ragged ? 1+lane%cap : cap; s->last[lane]=-1;
        assert(s->ctx->lane[lane].weights.in_proj==s->ctx->lane[0].weights.in_proj);
    }
    (void)ids; (void)length;
}
static uint64_t prefill(OSequence *s,int ids[8][512],int *length)
{
    uint64_t t=lifetime_now();
    for (int lane=0;lane<s->ctx->B;++lane) for (int j=0;j<length[lane];++j) {
        n2_lane=lane;assert(forward(&s->ctx->lane[lane],ids[lane][j])); ++s->ctx->position[lane];
    }
    return lifetime_now()-t;
}
int main(int argc,char **argv)
{
    if (argc!=9) return 2;
    int policy=atoi(argv[3]),B=atoi(argv[4]),ragged=atoi(argv[5]),cap=atoi(argv[6]),verify=atoi(argv[8]);
    if (policy<0 || policy>3 || B<1 || B>8 || ragged<0 || ragged>1 || cap<1 || cap>128) return 2;
    uint64_t invocation=lifetime_now(),t=invocation;
    FILE *in=fopen(argv[2],"rb"); assert(in); uint32_t header[3];
    assert(fread(header,4,3,in)==3 && header[0]==0x3152504f && header[1]==(unsigned)B && header[2]==EOS);
    int ids[8][512],length[8],prompt_tokens=0;
    for (int lane=0;lane<B;++lane) {
        uint32_t count; assert(fread(&count,4,1,in)==1 && count>0 && count<512); length[lane]=count; prompt_tokens+=count;
        assert(fread(ids[lane],4,count,in)==count);
    }
    assert(fgetc(in)==EOF); fclose(in);
    uint64_t input_ns=lifetime_now()-t;
    FILE *raw=strcmp(argv[7],"-") ? fopen(argv[7],"wb") : NULL;
    if (strcmp(argv[7],"-") && !raw) return 2;
    g_mode=6; g_profile=0; g_fixture=NULL;
    OSequence seq,ref;
    t=lifetime_now(); seed(&seq,argv[1],B,policy,ragged,cap,ids,length); uint64_t create_ns=lifetime_now()-t;
    Config *p=&seq.ctx->lane[0].config;
    for (int lane=0;lane<B;++lane) for (int j=0;j<length[lane];++j) assert(ids[lane][j]>=0 && ids[lane][j]<p->vocab_size);
    uint64_t prefill_ns=prefill(&seq,ids,length);
    if(getenv("NJ_STAGE")&&!strcmp(getenv("NJ_STAGE"),"prefill")){n2_dump("logits",seq.ctx->lane[0].state.logits,p->rounded_vocab_size);puts("{\"stage\":\"complete\",\"errors\":0}");exit(0);}
    size_t state_bytes=(size_t)p->n_layers*p->d_inner*(p->d_state+p->d_conv)*4;
    size_t logits_bytes=(size_t)p->rounded_vocab_size*4;
    void *previous[8]={0};
    if (verify) {
        seed(&ref,argv[1],B,0,ragged,cap,ids,length); prefill(&ref,ids,length); o_compare(&seq,&ref);
        for (int lane=0;lane<B;++lane) { previous[lane]=malloc(state_bytes+logits_bytes); assert(previous[lane]); }
    }
    uint32_t raw_header[8]={0x3147524f,(uint32_t)B,(uint32_t)p->rounded_vocab_size,24,1536,16,4,(uint32_t)ragged};
    emit(raw,raw_header,sizeof(raw_header));
    for (int lane=0;lane<B;++lane) { emit(raw,&length[lane],4); emit(raw,ids[lane],length[lane]*4); emit(raw,seq.ctx->lane[lane].state.logits,logits_bytes); }
    uint64_t total=0,selection_ns=0,forward_ns=0,consumer_ns=0,cp_ns=0,cp_bytes=0;
    uint64_t first_emit_ns=0,first_complete_ns=0,final_consume_ns=0;
    uint64_t digest=UINT64_C(1469598103934665603),model_calls=0;
    uint64_t tick_ns[512]={0}; unsigned masks[512]={0}; int tick=0;
    uint64_t emissions[8][128]={{0}};
    n_schedule_ns=0;
    uint64_t decode_start=lifetime_now();
    for (;tick<512;++tick) {
        unsigned alive=0;
        for (int lane=0;lane<B;++lane) if (!seq.reason[lane]) alive|=1u<<lane;
        if (!alive) break;
        unsigned mask=alive;
        if (ragged) {
            if (tick%11==3) mask=0;
            if (tick%7==1 || tick%7==2) for (int lane=1;lane<B;lane+=2) mask&=~(1u<<lane);
        }
        if (verify) for (int lane=0;lane<B;++lane) if (!(mask&(1u<<lane))) pack_state(seq.ctx,lane,previous[lane],0);
        uint64_t tick_start=lifetime_now(),checkpoint_elapsed=0;
        if (ragged && tick && tick%8==0) {
            t=lifetime_now();
            for (int lane=0;lane<B;++lane) if (alive&(1u<<lane)) {
                OCheckpoint *cp=o_save(&seq,lane); cp_bytes+=cp->bytes+sizeof(*cp);
                assert(!o_restore(&seq,lane,cp)); o_drop(cp);
            }
            checkpoint_elapsed=lifetime_now()-t; cp_ns+=checkpoint_elapsed;
        }
        int selected[8],ref_selected[8];
        t=lifetime_now();
        for (int lane=0;lane<B;++lane) selected[lane]=(mask&(1u<<lane)) ? o_argmax(seq.ctx->lane[lane].state.logits,p->vocab_size) : -1;
        uint64_t selected_at=lifetime_now(); selection_ns+=selected_at-t;
        for (int lane=0;lane<B;++lane) if (mask&(1u<<lane)) emissions[lane][seq.emitted[lane]]=selected_at-decode_start;
        if (!first_emit_ns && mask) first_emit_ns=prefill_ns+selected_at-decode_start;
        t=lifetime_now(); assert(!o_feedback(&seq,policy,mask,selected)); forward_ns+=lifetime_now()-t; if (mask) ++model_calls;
        t=lifetime_now();
        for (int lane=0;lane<B;++lane) if (mask&(1u<<lane)) {
            digest=m_hash(digest,&selected[lane],4);
            digest=m_hash(digest,seq.ctx->lane[lane].state.logits,logits_bytes); ++total;
        }
        consumer_ns+=lifetime_now()-t;
        tick_ns[tick]=lifetime_now()-tick_start; masks[tick]=mask;
        if (!first_complete_ns && mask) first_complete_ns=prefill_ns+lifetime_now()-decode_start;
        if (verify) {
            if (ragged && tick && tick%8==0) for (int lane=0;lane<B;++lane) if (alive&(1u<<lane)) {
                OCheckpoint *cp=o_save(&ref,lane); assert(!o_restore(&ref,lane,cp)); o_drop(cp);
            }
            for (int lane=0;lane<B;++lane) {
                ref_selected[lane]=(mask&(1u<<lane)) ? o_argmax(ref.ctx->lane[lane].state.logits,p->vocab_size) : -1;
                assert(selected[lane]==ref_selected[lane]);
            }
            assert(!o_feedback(&ref,0,mask,ref_selected)); o_compare(&seq,&ref);
            for (int lane=0;lane<B;++lane) if (!(mask&(1u<<lane))) pack_state(seq.ctx,lane,previous[lane],1);
        }
        (void)checkpoint_elapsed;
        uint32_t raw_tick[2]={(uint32_t)tick,mask}; emit(raw,raw_tick,sizeof(raw_tick)); emit(raw,selected,B*4);
        for (int lane=0;lane<B;++lane) emit(raw,seq.ctx->lane[lane].state.logits,logits_bytes);
    }
    assert(tick<512 && total>0);
    t=lifetime_now();
    for (int lane=0;lane<B;++lane) {
        RunState *s=&seq.ctx->lane[lane].state;
        digest=m_hash(digest,s->ssm_state,(size_t)p->n_layers*p->d_inner*p->d_state*4);
        digest=m_hash(digest,s->conv_state,(size_t)p->n_layers*p->d_inner*p->d_conv*4);
    }
    final_consume_ns=lifetime_now()-t; consumer_ns+=final_consume_ns;
    uint64_t decode_ns=lifetime_now()-decode_start;
    for (int lane=0;lane<B;++lane) {
        uint64_t tail[4]={seq.ctx->position[lane],seq.emitted[lane],seq.reason[lane],seq.limit[lane]}; emit(raw,tail,sizeof(tail));
        RunState *s=&seq.ctx->lane[lane].state;
        emit(raw,s->ssm_state,(size_t)p->n_layers*p->d_inner*p->d_state*4);
        emit(raw,s->conv_state,(size_t)p->n_layers*p->d_inner*p->d_conv*4);
    }
    if (verify) { m_destroy(ref.ctx); for (int lane=0;lane<B;++lane) free(previous[lane]); }
    t=lifetime_now(); m_destroy(seq.ctx); uint64_t cleanup_ns=lifetime_now()-t;
    if (raw) assert(!fclose(raw));
    struct rusage usage; getrusage(RUSAGE_SELF,&usage);
    fprintf(stdout,"{\"policy\":%d,\"batch\":%d,\"ragged\":%d,\"cap\":%d,\"verified_reference\":%d,\"ticks\":%d,\"active_tokens\":%llu,\"prompt_tokens\":%d,\"input_ns\":%llu,\"create_ns\":%llu,\"prefill_ns\":%llu,\"decode_ns\":%llu,\"selection_ns\":%llu,\"forward_ns\":%llu,\"consumer_ns\":%llu,\"checkpoint_ns\":%llu,\"checkpoint_bytes\":%llu,\"schedule_prepare_ns\":%llu,\"first_emission_after_prefill_ns\":%llu,\"first_feedback_complete_ns\":%llu,\"cleanup_ns\":%llu,\"driver_lifetime_ns\":%llu,\"peak_rss_kib\":%ld,\"state_bytes_per_lane\":%zu,\"source_alloc_ns\":%llu,\"source_free_ns\":%llu,\"source_copy_ns\":%llu,\"source_alloc_calls\":%llu,\"source_free_calls\":%llu,\"source_copy_calls\":%llu,\"source_alloc_bytes\":%llu,\"source_copy_bytes\":%llu,\"model_calls\":%llu,\"digest\":\"%016llx\",\"tokens\":[",
        policy,B,ragged,cap,verify,tick,(unsigned long long)total,prompt_tokens,(unsigned long long)input_ns,(unsigned long long)create_ns,
        (unsigned long long)prefill_ns,(unsigned long long)decode_ns,(unsigned long long)selection_ns,(unsigned long long)forward_ns,(unsigned long long)consumer_ns,
        (unsigned long long)cp_ns,(unsigned long long)cp_bytes,(unsigned long long)n_schedule_ns,(unsigned long long)first_emit_ns,(unsigned long long)first_complete_ns,
        (unsigned long long)cleanup_ns,(unsigned long long)(lifetime_now()-invocation),usage.ru_maxrss,state_bytes,(unsigned long long)o_malloc_ns,(unsigned long long)o_free_ns,
        (unsigned long long)o_copy_ns,(unsigned long long)o_alloc_calls,(unsigned long long)o_free_calls,(unsigned long long)o_copy_calls,(unsigned long long)o_alloc_bytes,
        (unsigned long long)o_copy_bytes,(unsigned long long)model_calls,(unsigned long long)digest);
    for (int lane=0;lane<B;++lane) {
        fprintf(stdout,"%s[",lane ? "," : "");
        for (int j=0;j<seq.emitted[lane];++j) fprintf(stdout,"%s%d",j ? "," : "",seq.tokens[lane][j]);
        fprintf(stdout,"]");
    }
    fprintf(stdout,"],\"stop_reasons\":[");
    for (int lane=0;lane<B;++lane) fprintf(stdout,"%s%d",lane ? "," : "",seq.reason[lane]);
    fprintf(stdout,"],\"tick_ns\":[");
    for (int j=0;j<tick;++j) fprintf(stdout,"%s%llu",j ? "," : "",(unsigned long long)tick_ns[j]);
    fprintf(stdout,"],\"masks\":[");
    for (int j=0;j<tick;++j) fprintf(stdout,"%s%u",j ? "," : "",masks[j]);
    fprintf(stdout,"],\"emission_offsets_ns\":[");
    for (int lane=0;lane<B;++lane) {
        fprintf(stdout,"%s[",lane ? "," : "");
        for (int j=0;j<seq.emitted[lane];++j) fprintf(stdout,"%s%llu",j ? "," : "",(unsigned long long)emissions[lane][j]);
        fprintf(stdout,"]");
    }
    fprintf(stdout,"],\"invocation_monotonic_ns\":%llu,\"first_emission_monotonic_ns\":%llu}\n",(unsigned long long)invocation,(unsigned long long)(decode_start+first_emit_ns-prefill_ns));
    return 0;
}
