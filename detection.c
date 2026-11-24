#include "project.h"
#include <math.h>
#include <stdlib.h>

static int local_count(int r, int c, const unsigned char *mask) {
    int count = 0;
    for (int dr = -2; dr <= 2; ++dr) for (int dc = -2; dc <= 2; ++dc) {
        if (abs(dr) + abs(dc) > 2) continue;
        int rr = r + dr, cc = c + dc;
        if (rr >= 0 && rr < N && cc >= 0 && cc < N && mask[IDX(rr,cc)]) ++count;
    }
    return count;
}

void *hotspot_task(void *arg) {
    (void)arg; size_t n = 0;
    for (int r=0;r<N;r++) for (int c=0;c<N;c++) { double v=global_matrix[IDX(r,c)]; if(isfinite(v)&&v>T_HOT){hotspots[n++]=(Cell){r,c,v};hotmask[IDX(r,c)]=1;} }
    pthread_mutex_lock(&hot_mutex); hot_count=n; pthread_mutex_unlock(&hot_mutex);
    pthread_mutex_lock(&state_mutex); hot_done=1; pthread_cond_broadcast(&state_cond); pthread_mutex_unlock(&state_mutex); return NULL;
}
void *coldspot_task(void *arg) {
    (void)arg; size_t n = 0;
    for (int r=0;r<N;r++) for (int c=0;c<N;c++) { double v=global_matrix[IDX(r,c)]; if(isfinite(v)&&v<T_COLD){coldspots[n++]=(Cell){r,c,v};coldmask[IDX(r,c)]=1;} }
    pthread_mutex_lock(&cold_mutex); cold_count=n; pthread_mutex_unlock(&cold_mutex);
    pthread_mutex_lock(&state_mutex); cold_done=1; pthread_cond_broadcast(&state_cond); pthread_mutex_unlock(&state_mutex); return NULL;
}
void *normalize_task(void *arg) {
    (void)arg;
    pthread_mutex_lock(&state_mutex); while(!hot_done||!cold_done) pthread_cond_wait(&state_cond,&state_mutex); pthread_mutex_unlock(&state_mutex);
    double *adjusted = malloc((size_t)N * N * sizeof(double));
    if (!adjusted) return NULL;
    for (size_t k=0;k<(size_t)N*N;k++) adjusted[k]=global_matrix[k];
    for (int r=0;r<N;r++) for (int c=0;c<N;c++) {
        size_t k=IDX(r,c); double v=adjusted[k]; if(!isfinite(v)){normalized[k]=NAN;continue;}
        int hp=local_count(r,c,hotmask), cp=local_count(r,c,coldmask);
        if(hp>0 && cp>0) {
            double hs=0,cs=0;int hn=0,cn=0;
            for(int dr=-2;dr<=2;dr++)for(int dc=-2;dc<=2;dc++)if(abs(dr)+abs(dc)<=2){int rr=r+dr,cc=c+dc;if(rr<0||rr>=N||cc<0||cc>=N)continue;size_t q=IDX(rr,cc);if(hotmask[q]){hs+=adjusted[q];hn++;}if(coldmask[q]){cs+=adjusted[q];cn++;}}
            if(hn&&cn) v=0.5*v+0.25*(hs/hn)+0.25*(cs/cn);
        }
        global_matrix[k]=v;
    }
    free(adjusted);
    double mn=INFINITY,mx=-INFINITY;
    for(size_t k=0;k<(size_t)N*N;k++) if(isfinite(global_matrix[k])) { if(global_matrix[k]<mn)mn=global_matrix[k]; if(global_matrix[k]>mx)mx=global_matrix[k]; }
    for(size_t k=0;k<(size_t)N*N;k++){double v=global_matrix[k];normalized[k]=!isfinite(v)?NAN:(mx>mn?(v-mn)/(mx-mn):0.0);}
    return NULL;
}
