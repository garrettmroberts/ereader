#pragma once

// One full update, then two partial updates. A forced full update starts that run over.
constexpr int kFullEvery = 3;
constexpr int kRefreshStart = kFullEvery - 1;

struct RefreshPlan {
    bool full;
    int paintsSinceFull;
};

inline RefreshPlan chooseRefresh(int paintsSinceFull, bool forceFull) {
    paintsSinceFull++;
    const bool full = forceFull || paintsSinceFull >= kFullEvery;
    if (full) {
        paintsSinceFull = 0;
    }
    return {full, paintsSinceFull};
}
