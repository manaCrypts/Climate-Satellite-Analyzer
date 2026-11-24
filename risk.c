#include "project.h"
#include <stdlib.h>
#include <math.h>
static int nearby(int r,int c,const unsigned char *mask){int n=0;for(int dr=-2;dr<=2;dr++)for(int dc=-2;dc<=2;dc++)if(abs(dr)+abs(dc)<=2){int rr=r+dr,cc=c+dc;if(rr>=0&&rr<N&&cc>=0&&cc<N&&mask[IDX(rr,cc)])n++;}return n;}
void *risk_worker(void *arg) {
    (void)arg;
    while(1){pthread_mutex_lock(&queue_mutex);int tile=next_tile++;pthread_mutex_unlock(&queue_mutex);if(tile>=NTILE)break;
        int tr=tile/(N/TILE),tc=tile%(N/TILE);
        for(int r=tr*TILE;r<(tr+1)*TILE;r++)for(int c=tc*TILE;c<(tc+1)*TILE;c++){
            double v=normalized[IDX(r,c)];if(!isfinite(v)){risk_scores[IDX(r,c)]=NAN;continue;}
            int hp=nearby(r,c,hotmask),cp=nearby(r,c,coldmask);
            risk_scores[IDX(r,c)]=v*hp/(cp+1.0);
        }
    }return NULL;
}
