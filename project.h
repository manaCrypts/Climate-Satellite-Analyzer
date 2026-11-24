#ifndef PROJECT_H
#define PROJECT_H
#include <pthread.h>
#include <stddef.h>
#define N 1000
#define NSAT 3
#define TILE 100
#define NTILE ((N / TILE) * (N / TILE))
#define T_HOT 35.0
#define T_COLD -10.0
#define TOPK 10

typedef struct { double min, max, mean, variance, stddev; long count, anomalies; } Stats;
typedef struct { int row, col; double value; } Cell;
extern double *sat[NSAT], *global_matrix, *normalized, *risk_scores;
extern unsigned char *hotmask, *coldmask;
extern int next_tile;
extern pthread_mutex_t queue_mutex, stats_mutex, state_mutex, hot_mutex, cold_mutex;
extern pthread_cond_t state_cond;
extern int hot_done, cold_done;
extern Cell *hotspots, *coldspots;
extern size_t hot_count, cold_count;
extern Stats global_stats;
extern int worker_count;
extern int demo_mode;
#define IDX(r,c) ((size_t)(r) * N + (c))
void *prepare_satellite(void *arg);
void *tile_worker(void *arg);
void *hotspot_task(void *arg);
void *coldspot_task(void *arg);
void *normalize_task(void *arg);
void *risk_worker(void *arg);
int load_satellite_csv(const char *path, double *matrix);
void generate_demo_data(void);
#endif
