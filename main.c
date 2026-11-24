#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

double *sat[NSAT], *global_matrix, *normalized, *risk_scores;
unsigned char *hotmask, *coldmask;
int next_tile = 0, worker_count = 4, demo_mode = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hot_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cold_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t state_cond = PTHREAD_COND_INITIALIZER;
int hot_done = 0, cold_done = 0;
Cell *hotspots, *coldspots;
size_t hot_count = 0, cold_count = 0;
Stats global_stats = {.min = INFINITY, .max = -INFINITY};

static void usage(const char *p) {
    printf("Usage: %s [--threads COUNT] [--demo | --input-dir DIR]\n", p);
    printf("Default input files: data/satellite1.csv, satellite2.csv, satellite3.csv\n");
}

int main(int argc, char **argv) {
    const char *input_dir = "data";
    demo_mode = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--demo")) demo_mode = 1;
        else if (!strcmp(argv[i], "--threads") && i + 1 < argc) {
            worker_count = atoi(argv[++i]);
            if (worker_count < 1 || worker_count > 256) { fprintf(stderr, "Threads must be 1..256\n"); return 1; }
        } else if (!strcmp(argv[i], "--input-dir") && i + 1 < argc) input_dir = argv[++i];
        else if (!strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 1; }
    }
    const size_t cells = (size_t)N * N;
    for (int s = 0; s < NSAT; ++s) {
        sat[s] = malloc(cells * sizeof(double));
        if (!sat[s]) { perror("malloc satellite"); return 1; }
        for (size_t k = 0; k < cells; ++k) sat[s][k] = NAN;
    }
    global_matrix = malloc(cells * sizeof(double));
    normalized = malloc(cells * sizeof(double));
    risk_scores = malloc(cells * sizeof(double));
    hotmask = calloc(cells, 1); coldmask = calloc(cells, 1);
    hotspots = malloc(cells * sizeof(Cell));
    coldspots = malloc(cells * sizeof(Cell));
    if (!global_matrix || !normalized || !risk_scores || !hotmask || !coldmask || !hotspots || !coldspots) { perror("malloc project data"); return 1; }

    if (demo_mode) generate_demo_data();
    else {
        char path[1024];
        for (int s = 0; s < NSAT; ++s) {
            snprintf(path, sizeof(path), "%s/satellite%d.csv", input_dir, s + 1);
            if (load_satellite_csv(path, sat[s]) != 0) {
                fprintf(stderr, "Could not load %s. Use --demo or provide three N x N CSV files.\n", path);
                return 1;
            }
        }
    }

    printf("\nCLIMATE SATELLITE TEMPERATURE MATRIX ANALYZER\n");
    printf("Matrix: %d x %d | Satellites: %d | Tile: %d x %d | Workers: %d\n", N, N, NSAT, TILE, TILE, worker_count);
    printf("Thresholds: hotspot > %.1f C, coldspot < %.1f C\n", T_HOT, T_COLD);
    printf("Input: %s\n\n", demo_mode ? "simulated demonstration data" : input_dir);

    pthread_t prep[NSAT]; int ids[NSAT];
    for (int s = 0; s < NSAT; ++s) { ids[s] = s; pthread_create(&prep[s], NULL, prepare_satellite, &ids[s]); }
    for (int s = 0; s < NSAT; ++s) pthread_join(prep[s], NULL);
    puts("[1/5] Satellite preparation and missing-value interpolation complete.");

    pthread_t *workers = calloc((size_t)worker_count, sizeof(pthread_t));
    for (int i = 0; i < worker_count; ++i) pthread_create(&workers[i], NULL, tile_worker, NULL);
    for (int i = 0; i < worker_count; ++i) pthread_join(workers[i], NULL);
    printf("[2/5] Matrix merge and tile analysis complete (%d tiles).\n", NTILE);

    pthread_t hot_thread, cold_thread, norm_thread;
    pthread_create(&hot_thread, NULL, hotspot_task, NULL);
    pthread_create(&cold_thread, NULL, coldspot_task, NULL);
    pthread_create(&norm_thread, NULL, normalize_task, NULL);
    pthread_join(hot_thread, NULL); pthread_join(cold_thread, NULL); pthread_join(norm_thread, NULL);
    printf("[3/5] Hotspot and coldspot detection complete. Hot: %zu | Cold: %zu\n", hot_count, cold_count);
    puts("[4/5] Weighted adjustment and normalization complete.");

    next_tile = 0;
    for (int i = 0; i < worker_count; ++i) pthread_create(&workers[i], NULL, risk_worker, NULL);
    for (int i = 0; i < worker_count; ++i) pthread_join(workers[i], NULL);
    puts("[5/5] Parallel risk scoring complete.");

    if (global_stats.count > 0) { global_stats.mean /= global_stats.count; global_stats.variance = global_stats.variance / global_stats.count - global_stats.mean * global_stats.mean; if (global_stats.variance < 0) global_stats.variance = 0; global_stats.stddev = sqrt(global_stats.variance); }

    printf("\nGLOBAL STATISTICS (merged matrix)\n");
    printf("Valid cells: %ld / %zu\n", global_stats.count, cells);
    printf("Minimum: %.2f C | Maximum: %.2f C | Mean: %.2f C\n", global_stats.min, global_stats.max, global_stats.mean);
    printf("Variance: %.4f | Standard deviation: %.4f | Anomalies: %ld\n", global_stats.variance, global_stats.stddev, global_stats.anomalies);
    printf("\nTOP %d RISK CELLS\n", TOPK);
    Cell top[TOPK]; double scores[TOPK];
    for (int i = 0; i < TOPK; ++i) { scores[i] = -INFINITY; top[i] = (Cell){-1,-1,0}; }
    for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) {
        double score = risk_scores[IDX(r,c)]; if (!isfinite(score)) continue;
        for (int k = 0; k < TOPK; ++k) if (score > scores[k]) {
            for (int m = TOPK-1; m > k; --m) { scores[m]=scores[m-1]; top[m]=top[m-1]; }
            scores[k]=score; top[k]=(Cell){r,c,global_matrix[IDX(r,c)]}; break;
        }
    }
    for (int i = 0; i < TOPK; ++i) if (top[i].row >= 0)
        printf("%2d. row=%4d col=%4d temp=%7.2f C risk=%0.6f\n", i+1, top[i].row, top[i].col, top[i].value, scores[i]);
    else printf("%2d. No valid cell\n", i+1);

    FILE *out = fopen("output/summary.txt", "w");
    if (out) {
        fprintf(out, "Climate Satellite Analyzer Summary\nMatrix: %d x %d\nWorkers: %d\nHotspots: %zu\nColdspots: %zu\nValid cells: %ld\nMin: %.4f\nMax: %.4f\nMean: %.4f\nVariance: %.6f\nStddev: %.6f\nAnomalies: %ld\n", N,N,worker_count,hot_count,cold_count,global_stats.count,global_stats.min,global_stats.max,global_stats.mean,global_stats.variance,global_stats.stddev,global_stats.anomalies);
        for (int i=0;i<TOPK;i++) if (top[i].row>=0) fprintf(out,"Top %d: row=%d col=%d temp=%.4f risk=%.6f\n",i+1,top[i].row,top[i].col,top[i].value,scores[i]);
        fclose(out); puts("\nSaved report: output/summary.txt");
    }
    free(workers); for (int s=0;s<NSAT;s++) free(sat[s]); free(global_matrix); free(normalized); free(risk_scores); free(hotmask); free(coldmask); free(hotspots); free(coldspots);
    return 0;
}
