# Limit Order Book Simulation (Ongoing)


## Current State

Order book with price levels and FIFO queues.
Basic order generation and matching.
Simple statistics (orders added, cancelled, matched).
poisson process done
- to be done:
  - Proper cancellation (pick a real order, not random ID).
  - Waiting‑time distributions and fill probabilities.
  - Performance optimisations for large‑scale simulation.
  - Output to CSV / visualisation.

## Build & Run

g++ -std=c++17 main.cpp -o lob_sim
./lob_sim
