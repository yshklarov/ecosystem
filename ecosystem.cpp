#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <vector>

#include "fenster.h"

#define FPS 60

#define BLACK 0x00'00'00
#define WHITE 0xFF'FF'FF
#define RED 0xFF'33'00

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;


// JSF (Jenkins Small Fast) random number generator
// https://burtleburtle.net/bob/rand/smallprng.html
typedef struct jsf_state { u64 a; u64 b; u64 c; u64 d; } jsf_state;
#define ROT32(x,k) (((x)<<(k))|((x)>>(32-(k))))
u64 rand_raw(jsf_state* x) {
    u64 e = x->a - ROT32(x->b, 27);
    x->a = x->b ^ ROT32(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}
void rand_init_from_seed(jsf_state* x, u64 seed) {
    u64 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i=0; i<20; ++i) {
        (void)rand_raw(x);
    }
}
void rand_init_from_time(jsf_state *x) {
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    u64 seed = ts.tv_nsec;
    rand_init_from_seed(x, seed);
}
jsf_state rng;  // Global


typedef struct point {
    u16 x = 0;
    u16 y = 0;
} point;

typedef struct organism {
    point pos = {};
    i32 energy = 0;
    u32 kills = 0;
} organism;

typedef struct population {
    i32 initial_energy = 0;
    i32 energy_delta = 0;
    i32 replication_cost = 0;
    u8 trophic_level = 0;
    size_t max_size = 0;
    u32 color = WHITE;
    std::vector<organism> organisms = {};
} population;

typedef struct world {
    i32 w = 0;
    i32 h = 0;
    std::vector<population> populations = {};
} world;

bool coincide(point a, point b) {
    return a.x == b.x && a.y == b.y;
}

// Add a new population, with population_size distinct organisms. Positions are chosen randomly, independently, and
// uniformly (this means that it's possible for multiple organisms to occupy a single site). The organisms will have
// (initial) energy and other parameters as given.
void add_population(
    world *wld,
    size_t initial_size,
    size_t max_size,
    i32 initial_energy,
    i32 energy_delta,
    i32 replication_cost,
    u8 trophic_level,
    u32 color)
{
    wld->populations.push_back({
            .initial_energy = initial_energy,
            .energy_delta = energy_delta,
            .replication_cost = replication_cost,
            .trophic_level = trophic_level,
            .max_size = max_size,
            .color = color,
        });
    population &pop = wld->populations.back();
    for (size_t i = 0; i < initial_size; ++i) {
        u16 x = (u16)(rand_raw(&rng) % wld->w);
        u16 y = (u16)(rand_raw(&rng) % wld->h);
        pop.organisms.push_back({
                .pos = {.x=x, .y=y},
                .energy = initial_energy,
            });
    }
}

// L1 distance: Sum of X and Y distances, taking into account grid wrap-around (e.g., the distance between NW corner and
// SE corner of grid is 2).
i32 distance(const point &a, const point &b, i32 w, i32 h) {
    i32 dist_x = abs(b.x - a.x);
    dist_x = std::min(dist_x, w - dist_x);
    i32 dist_y = abs(b.y - a.y);
    dist_y = std::min(dist_y, h - dist_y);
    return dist_x + dist_y;
}

// Take one time step.
void evolve(world *wld) {
    for (size_t k = 0; k < wld->populations.size(); ++k) {
        population &pop = wld->populations[k];
        size_t num_to_act = pop.organisms.size();
        for (size_t i = 0; i < num_to_act; ++i) {
            organism *me = &pop.organisms[i];

            // Replicate. Neither me nor the clone will act further until the following step.
            if (me->energy >= pop.replication_cost &&
                pop.organisms.size() < pop.max_size)
            {
                pop.organisms.push_back({
                        .pos = me->pos,
                        .energy = pop.initial_energy,
                    });
                me = &pop.organisms[i];
                me->energy = pop.initial_energy;
            } else {
                // Move.
                me->pos.x = (me->pos.x + wld->w + (rand_raw(&rng) % 3 - 1)) % wld->w;
                me->pos.y = (me->pos.y + wld->h + (rand_raw(&rng) % 3 - 1)) % wld->h;
                me->energy += pop.energy_delta;
                me->energy = std::min(me->energy, pop.replication_cost);  // Cap energy.

                // Predate.
                for (size_t l = 0; l < wld->populations.size(); ++l) {
                    if (wld->populations[l].trophic_level != pop.trophic_level - 1) {
                        // This is not a prey population for me.
                        continue;
                    }
                    population &prey_pop = wld->populations[l];
                    for (size_t j = 0; j < prey_pop.organisms.size(); ++j) {
                        organism *other = &prey_pop.organisms[j];
                        if (coincide(me->pos, other->pos)) {
                            me->energy += other->energy;
                            prey_pop.organisms.erase(prey_pop.organisms.begin() + (j--));
                            other = nullptr;
                            ++me->kills;
                        }
                    }
                }
            }

            // Die.
            if (me->energy < 0) {
                pop.organisms.erase(pop.organisms.begin() + i);
                me = nullptr;
                --i;
                --num_to_act;
            }
        }
    }
}

void render(const world *wld, const fenster *f, int zoom) {
    // Clear display.
    for (int i = 0; i < f->width; i++) {
        for (int j = 0; j < f->height; j++) {
            fenster_pixel(f, i, j) = BLACK;
        }
    }
    for (const population &pop: wld->populations) {
        u32 color = pop.color;
        for (const organism &org : pop.organisms) {
            for (int i = 0; i < zoom; ++i) {
                for (int j = 0; j < zoom; ++j) {
                    fenster_pixel(f, zoom*org.pos.x + i, zoom*org.pos.y + j) = color;
                }
            }
        }
    }
}

void run(world *wld, i32 steps, bool display_fenster = true, int zoom = 1, bool verbose = false) {
    bool forever = steps == 0;
    i64 prev_render = 0;

    u32 *buf = nullptr;
    struct fenster f = {
        .title = "Ecosystem Simulation",
        .width = wld->w * zoom,
        .height = wld->h * zoom,
        .buf = nullptr,
    };
    if (display_fenster) {
        buf = (u32 *)malloc(sizeof(u32) * wld->w * wld->h * zoom * zoom);
        f.buf = buf;
        fenster_open(&f);

        // Bugfix: no fenster display for first frame.
        fenster_sleep(1000/FPS);
        prev_render = fenster_time();
        render(wld, &f, zoom);
    }

    for (i32 step = 0; true; ++step) {
        if (verbose) {
            if (steps > 0) {
                fprintf(stdout, "Time %d/%d: Population sizes: ", step, steps);
            } else {
                fprintf(stdout, "Time %d: Population sizes: ", step);
            }
            for (population &pop : wld->populations) {
                fprintf(stdout, "%zu ", pop.organisms.size());
            }
            fprintf(stdout, "\n");
        }

        if (display_fenster) {
            if (fenster_loop(&f) != 0) {
                // User closed window?
                break;
            }
            // Simple framerate controller.
            i64 now = fenster_time();
            if (now - prev_render > 1000/FPS) {
                prev_render = now;
                render(wld, &f, zoom);
            }
            // Can change this to slow down the simulation.
            fenster_sleep(0);
        }

        if (!forever && step == steps) {
            break;
        }

        evolve(wld);
    }

    if (display_fenster) {
        fenster_close(&f);
        f.buf = nullptr;
        free(buf);
        buf = nullptr;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 8 || 9 < argc) {
        fprintf(stderr, "Usage: ecosystem <width> <height> <rabbits> <rabbits_max> <foxes> <foxes_max>\n"
                        "                 <simulation_length> [random_seed]\n");
        fprintf(stderr, "  width, height: Size of grid, e.g., 1000 by 1000\n");
        fprintf(stderr, "  rabbits: Initial number of rabbits.\n");
        fprintf(stderr, "  rabbits_max: Maximum number of rabbits.\n");
        fprintf(stderr, "  foxes: Initial number of rabbits.\n");
        fprintf(stderr, "  foxes_max: Maximum number of foxes.\n");
        fprintf(stderr, "  simulation_length: Number of time steps to run. For infinite simulation, put 0.\n");
        fprintf(stderr, "  random_seed: Seed for random number generator. If not given, will be picked randomly.\n");
        return EXIT_FAILURE;
    }

    i32 w = atoi(argv[1]);
    i32 h = atoi(argv[2]);
    f64 rabbits = atof(argv[3]);
    f64 rabbits_max = atof(argv[4]);
    f64 foxes = atof(argv[5]);
    f64 foxes_max = atof(argv[6]);
    i32 steps = atoi(argv[7]);
    bool seed_given = (argc == 9);
    u64 seed = 0;
    if (seed_given) {
        seed = (u64)atoi(argv[8]);
    }
    bool display = true;
    int zoom = 5;

    if (w < 1 || h < 1) {
        fprintf(stderr, "Invalid world dimensions.\n");
        return EXIT_FAILURE;
    }
    if (! (0 <= rabbits && rabbits <= rabbits_max && 0 <= foxes && foxes <= foxes_max)) {
        fprintf(stderr, "Invalid population counts.\n");
        return EXIT_FAILURE;
    }
    if (steps < 0) {
        fprintf(stderr, "Steps must be at least 0.\n");
        return EXIT_FAILURE;
    }

    // Seed RNG
    if (seed_given) {
        rand_init_from_seed(&rng, seed);
    } else {
        rand_init_from_time(&rng);
    }

    world wld{.w=w, .h=h};

    // Rabbits
    add_population(
        &wld,
        rabbits,
        rabbits_max,
        10,
        1,
        20,
        1,
        WHITE);

    // Foxes
    add_population(
        &wld,
        foxes,
        foxes_max,
        100,
        -2,
        400,
        2,
        RED);

    run(&wld, steps, display, zoom, true);

    return EXIT_SUCCESS;
}
