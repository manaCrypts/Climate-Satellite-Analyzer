#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <strings.h>

int load_satellite_csv(const char *path, double *matrix) {
    FILE *f=fopen(path,"r"); if(!f) return -1;
    char *line=NULL; size_t cap=0; int r=0;
    while(getline(&line,&cap,f)!=-1 && r<N) {
        char *save=NULL, *tok=strtok_r(line,",\r\n",&save); int c=0;
        while(tok && c<N) { if(!strcasecmp(tok,"nan") || !*tok) matrix[IDX(r,c)]=NAN; else matrix[IDX(r,c)]=strtod(tok,NULL); ++c; tok=strtok_r(NULL,",\r\n",&save); }
        if(c!=N) { fprintf(stderr,"Invalid CSV row %d in %s: expected %d columns, got %d\n",r+1,path,N,c); free(line); fclose(f); return -1; }
        ++r;
    }
    free(line); fclose(f);
    if(r!=N) { fprintf(stderr,"Invalid CSV file %s: expected %d rows, got %d\n",path,N,r); return -1; }
    return 0;
}

void generate_demo_data(void) {
    for(int s=0;s<NSAT;s++) for(int r=0;r<N;r++) for(int c=0;c<N;c++) {
        double v=18.0 + 9.0*sin((r+s*13)*0.013) + 7.0*cos((c+s*7)*0.017) + s*0.7;
        if ((r>190+s*12 && r<220+s*12 && c>300 && c<340) || (r>700 && r<720 && c>780-s*8 && c<810-s*8)) v += 27.0;
        if ((r>470 && r<495 && c>100+s*20 && c<135+s*20)) v -= 38.0;
        if (((r*31+c*17+s*13)%997)==0) v=NAN;
        sat[s][IDX(r,c)]=v;
    }
}

void *prepare_satellite(void *arg) {
    int s=*(int*)arg; double *m=sat[s];
    /* Interpolate each NaN from valid up/down/left/right neighbours, using a snapshot. */
    double *copy=malloc((size_t)N*N*sizeof(double)); if(!copy) return NULL;
    memcpy(copy,m,(size_t)N*N*sizeof(double));
    for(int r=0;r<N;r++) for(int c=0;c<N;c++) if(!isfinite(copy[IDX(r,c)])) {
        double sum=0; int count=0;
        if(r>0 && isfinite(copy[IDX(r-1,c)])){sum+=copy[IDX(r-1,c)];count++;}
        if(r<N-1 && isfinite(copy[IDX(r+1,c)])){sum+=copy[IDX(r+1,c)];count++;}
        if(c>0 && isfinite(copy[IDX(r,c-1)])){sum+=copy[IDX(r,c-1)];count++;}
        if(c<N-1 && isfinite(copy[IDX(r,c+1)])){sum+=copy[IDX(r,c+1)];count++;}
        if(count) m[IDX(r,c)]=sum/count;
    }
    free(copy); return NULL;
}

void *tile_worker(void *arg) {
    (void)arg;
    while(1) {
        pthread_mutex_lock(&queue_mutex); int tile=next_tile++; pthread_mutex_unlock(&queue_mutex);
        if(tile>=NTILE) break;
        int tr=tile/(N/TILE), tc=tile%(N/TILE), r0=tr*TILE, c0=tc*TILE;
        double sum=0,sumsq=0,min=INFINITY,max=-INFINITY; long count=0;
        for(int r=r0;r<r0+TILE;r++) for(int c=c0;c<c0+TILE;c++) {
            double total=0; int n=0;
            for(int s=0;s<NSAT;s++){double v=sat[s][IDX(r,c)];if(isfinite(v)){total+=v;n++;}}
            double v=n?total/n:NAN; global_matrix[IDX(r,c)]=v;
            if(isfinite(v)){sum+=v;sumsq+=v*v;if(v<min)min=v;if(v>max)max=v;count++;}
        }
        if(count) {
            double mean=sum/count, variance=sumsq/count-mean*mean; if(variance<0 && variance>-1e-8)variance=0;
            double sd=sqrt(fmax(0,variance)); long anomalies=0;
            for(int r=r0;r<r0+TILE;r++) for(int c=c0;c<c0+TILE;c++){double v=global_matrix[IDX(r,c)];if(isfinite(v)&&fabs(v-mean)>2*sd)anomalies++;}
            pthread_mutex_lock(&stats_mutex);
            global_stats.count+=count; global_stats.mean+=sum; global_stats.variance+=sumsq;
            if (min < global_stats.min) global_stats.min = min;
            if (max > global_stats.max) global_stats.max = max;
            global_stats.anomalies += anomalies;
            pthread_mutex_unlock(&stats_mutex);
        }
    }
    return NULL;
}
