# Climate Satellite Temperature Matrix Analyzer

A C / POSIX Threads demonstration for the CS2006 Operating Systems project.

## Requirements
- Ubuntu/Linux
- GCC
- GNU Make

Install tools:
```bash
sudo apt update
sudo apt install build-essential
```

## Compile
From this directory:
```bash
make
```

## Run immediately (simulated 1000 x 1000 satellite data)
```bash
./climate_analyzer --demo --threads 4
```
Change the number of workers:
```bash
./climate_analyzer --demo --threads 8
```

## Run using CSV input
Put exactly three files in `data/`: `satellite1.csv`, `satellite2.csv`, and `satellite3.csv`. Each file must contain exactly 1000 rows and 1000 comma-separated values per row. Use `NaN` for missing readings. Then run:
```bash
./climate_analyzer --threads 4
```
Or specify another directory:
```bash
./climate_analyzer --input-dir /path/to/csvs --threads 6
```

## Output
- Console: processing stages, global statistics, detected hotspot/coldspot counts, top 10 risk cells.
- `output/summary.txt`: saved summary and top-risk list.

## Implemented OS concepts
- POSIX threads (`pthread_create`, `pthread_join`)
- Satellite-level parallel preparation
- Mutex-protected dynamic tile work queue
- Parallel tile processing and risk-stage workers
- Mutex-protected statistics reduction
- Hotspot/coldspot tasks running concurrently
- Condition variable for normalization dependency
- Dynamically allocated matrices

## Documented assumptions
- Missing values are interpolated from valid immediate up/down/left/right neighbours using a snapshot of each satellite matrix. If no valid neighbour exists, the value remains missing and is ignored in merging.
- The global cell is the average of valid satellite values.
- Tiles are 100 x 100; there are 100 tiles for N=1000.
- Tile anomaly: absolute deviation from tile mean is greater than twice the tile standard deviation.
- Hotspot: temperature > 35 C. Coldspot: temperature < -10 C.
- If a cell is within Manhattan distance 2 of both a hotspot and a coldspot, the adjustment is 0.5*cell + 0.25*mean nearby hotspot + 0.25*mean nearby coldspot. Normalization is min-max after adjustment.
- Risk score: normalized temperature * nearby hotspot count / (nearby coldspot count + 1).

Note: this is a course-project simulation using synthetic values unless real CSV files are supplied. It does not connect to live satellites.
