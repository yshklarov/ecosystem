#define _POSIX_C_SOURCE 199309L
// Fix symbol-not-defined errors due to fenster.h.
// This is not great -- better to change (or eliminate) fenster.h.
#include <time.h>
#undef _POSIX_C_SOURCE

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "fenster.h"

#include "util/util.c"


#define FPS 60

#define BLACK 0x00'00'00
#define WHITE 0xFF'FF'FF
#define YELLOW 0xFF'FF'00
#define RED 0xFF'00'00
#define GREEN 0x00'FF'00
#define BLUE 0x00'00'FF
#define CYAN 0x00'FF'FF


typedef struct population_params {
    buffer name;
    u32 color;
    bool motile;
    u8 trophic_level;
    u32 initial_population_size;
    u16 energy_at_birth;
    u16 energy_maximum;
    u16 energy_threshold_replicate;
    u16 energy_cost_replicate;
    u16 energy_gain;
    u16 energy_cost_move;
    u8 replication_space_needed;
} population_params;

typedef struct simulation_params {
    bool rng_seed_given;
    u64 rng_seed;
    u16 w;
    u16 h;
    bool visual;
    bool run_forever;
    u32 num_steps;
    u16 population_count;
    population_params* populations; // Array
} simulation_params;

simulation_params simulation_params_create(u16 population_count) {
    simulation_params sp = {};
    if ((sp.populations = calloc(population_count, sizeof *sp.populations))) {
        sp.population_count = population_count;
    } else {
        fprintf(stderr, "Failed to allocate memory for simulation_params.\n");
    }
    return sp;
}

void simulation_params_destroy(simulation_params *sp) {
    for (int i = 0; i < sp->population_count; ++i) {
        buffer_destroy(&(sp->populations[i].name));
    }
    sp->population_count = 0;
    free(sp->populations);
    sp->populations = nullptr;
}


typedef struct point {
    u16 x;
    u16 y;
} point;

inline bool coincide(point a, point b) {
    return a.x == b.x && a.y == b.y;
}

typedef struct organism {
    u32 birthday;
    u32 last_active;
    u16 energy;
    u16 kills;
    bool exists;
} organism;

typedef struct world {
    simulation_params params;
    u16 w;
    u16 h;
    u32 step;
    u32* pop_tally;
    organism* map;  // 3D array of dimensions [h][w][num_populations].
} world;

// Return a pointer to the organism wld->map[y][x][pop].
organism* world_map_idx(world const* wld, u16 x, u16 y, u16 pop) {
    return &wld->map[(wld->params.population_count)*(y*(wld->w) + x) + pop];
}

int population_create(world* wld, u16 pop_id);

bool world_create(world* wld, simulation_params params) {
    wld->params = params;
    wld->w = params.w;
    wld->h = params.h;
    wld->step = 0;
    wld->pop_tally = (u32*)calloc(
        (size_t)params.population_count,
        sizeof *wld->pop_tally);
    wld->map = (organism*)calloc(
        (size_t)wld->h * (size_t)wld->w * params.population_count,
        sizeof *wld->map);

    /**** Seed RNG prior to generating populations. ****/
    if (wld->params.rng_seed_given) {
        rand_init_from_seed(&rand_state_global, wld->params.rng_seed);
    } else {
        rand_init_from_time(&rand_state_global);
    }

    for (u16 pop = 0; pop < params.population_count; ++pop) {
        if (0 != population_create(wld, pop)) {
            return false;
        }
    }
    return true;
}

void world_destroy(world* wld) {
    free(wld->pop_tally);
    wld->pop_tally = nullptr;
    free(wld->map);
    wld->map = nullptr;
    *wld = (world){};
}


// Parameters:
//     wld: A valid (created) world.
//     counter: Array of size wld->num_populations.
void population_count(world* wld, u32 counter[]) {
    u16 const num_populations = wld->params.population_count;
    for (int i = 0; i < num_populations; ++i) {
        counter[i] = 0;
    }
    organism const * const end = wld->map + (wld->w * wld->h * num_populations);
    u16 spc = 0;
    for (organism const* org = wld->map;
         org != end;
         ++org, ++spc
        ) {
        spc %= num_populations;
        counter[spc] += org->exists;
    }
}

// Precondition: world_create(wld) has already been called, and  0 <= id < num_populations.
// Returns: 0 on success, nonzero on failure.
int population_create(world* wld, u16 pop_id) {
    population_params const*const params = &wld->params.populations[pop_id];

    // u32 is big enough for maximum u16 * u16.
    u32 const map_cells = (u32)wld->w * (u32)wld->h;
    if (map_cells < params->initial_population_size) {
        fprintf(stderr, "Cannot create population: World map is too small.\n");
        return 1;
    }

    bool occupancy[map_cells];
    rand_combination(map_cells, params->initial_population_size, occupancy);
    for (u16 y = 0; y < wld->h; ++y) {
        for (u16 x = 0; x < wld->w; ++x) {
            organism* const org = world_map_idx(wld, x, y, pop_id);
            if (occupancy[y*wld->w + x]) {
                *org = (organism){
                    .exists = true,
                    .birthday = wld->step,
                    .energy = params->energy_at_birth,
                    .kills = 0,
                };
                ++wld->pop_tally[pop_id];
            } else {
                *org = (organism){};
            }
        }
    }

    if (wld->pop_tally[pop_id] != params->initial_population_size) {
        fprintf(
            stderr,
            "Error creating population %u: Created %u/%u organisms.\n",
            pop_id,
            wld->pop_tally[pop_id],
            params->initial_population_size);
        return 1;
    }

    return 0;
}

// Take one time step.
void evolve(world* wld) {
    population_params const*const pop_params = wld->params.populations;
    u16 const npops = wld->params.population_count;
    for (u16 y = 0; y < wld->h; ++y) {
        for (u16 x = 0; x < wld->w; ++x) {
            for (u16 pop = 0; pop < npops; ++pop ) {
                organism* org = world_map_idx(wld, x, y, pop);
                if (!org->exists) {
                    continue;
                }
                if (org->birthday == wld->step) {
                    // This one was born by replication during this very step; don't let it
                    // act on the same step when it was born.
                    continue;
                }
                if (org->last_active == wld->step) {
                    // Don't let a single organism act twice in one step.
                    continue;
                }

                // Two organisms from the same population cannot occupy the same site.
                // Find a random available direction to move or replicate.
                // Algorithm: Reservoir sampling: Each free neighboring cell is selected
                // with equal probability.
                u8 k = 0;  // Neighboring spaces unoccupied by members of same population.
                point new_pos = {};
                u16 xs[3] = { (x == 0 ? wld->w - 1 : x - 1), x, (u16)(x + 1) % wld->w };
                u16 ys[3] = { (y == 0 ? wld->h - 1 : y - 1), y, (u16)(y + 1) % wld->h };
                for (size_t i = 0; i < 3; ++i) {
                    for (size_t j = 0; j < 3; ++j) {
                        if (i == 1 && j == 1) continue;
                        point maybe = { .x = xs[j], .y = ys[i] };
                        if (!world_map_idx(wld, maybe.x, maybe.y, pop)->exists) {
                            // This cell is unoccupied.
                            ++k;
                            if (rand_unif(1, k) == k) {
                                new_pos = maybe;
                            }
                        }
                    }
                }

                org->energy += pop_params[pop].energy_gain;

                if (k == 0) {
                    // There's no neighboring space available.
                } else if ( (org->energy >= pop_params[pop].energy_threshold_replicate) &&
                            (k >= pop_params[pop].replication_space_needed) ) {
                    // Replicate.
                    organism* new_slot = world_map_idx(wld, new_pos.x, new_pos.y, pop);
                    *new_slot = (organism){
                        .birthday = wld->step,
                        .last_active = wld->step,
                        .energy = pop_params[pop].energy_at_birth,
                        .kills = 0,
                        .exists = true,
                    };
                    ++wld->pop_tally[pop];
                    if (org->energy > pop_params[pop].energy_cost_replicate) {
                        org->energy -= pop_params[pop].energy_cost_replicate;
                    } else {
                        // Die: No energy remaining.
                        *org = (organism){};                        
                        --wld->pop_tally[pop];
                        continue;
                    }
                } else {
                    // Move.
                    if (pop_params[pop].motile) {
                        organism* new_slot = world_map_idx(wld, new_pos.x, new_pos.y, pop);
                        *new_slot = *org;
                        *org = (organism){};
                        org = new_slot;
                    }

                    // Predate.
                    // Do this after moving, but before possibly dying of hunger.
                    for (u16 other_pop = 0; other_pop < npops; ++other_pop) {
                        if (pop_params[pop].trophic_level == 0 ||
                            pop_params[pop].trophic_level - 1 != pop_params[other_pop].trophic_level) {
                            // This is not a prey population.
                            continue;
                        }
                        organism* prey = world_map_idx(wld, x, y, other_pop);
                        if (prey->exists) {
                            org->energy += prey->energy;
                            *prey = (organism){};
                            --wld->pop_tally[other_pop];
                            ++org->kills;
                        }
                    }
                    
                    if (pop_params[pop].motile) {
                        if (org->energy > pop_params[pop].energy_cost_move) {
                            org->energy -= pop_params[pop].energy_cost_move;
                        } else {
                            // Die.
                            *org = (organism){};
                            --wld->pop_tally[pop];
                            continue;
                        }
                    }
                }
                org->energy = MIN(pop_params[pop].energy_maximum, org->energy);
                org->last_active = wld->step;
            }
        }
    }
    ++wld->step;
}

void render(
    world const* wld,
    struct fenster const* f,
    u8 zoom
    ) {
    // Clear display.
    for (int i = 0; i < f->width; i++) {
        for (int j = 0; j < f->height; j++) {
            fenster_pixel(f, i, j) = BLACK;
        }
    }
    for (u16 y = 0; y < wld->h; ++y) {
        for (u16 x = 0; x < wld->w; ++x) {
            for (u16 pop = 0; pop < wld->params.population_count; ++pop ) {
                u32 color = wld->params.populations[pop].color;
                organism* const org = world_map_idx(wld, x, y, pop);
                if (org->exists) {
                    for (u8 i = 0; i < zoom; ++i) {
                        for (u8 j = 0; j < zoom; ++j) {
                            fenster_pixel(f, zoom*x + i, zoom*y + j) = color;
                        }
                    }
                }
            }
        }
    }
}


void run(world* wld, u8 zoom, bool verbose) {
    bool const forever = wld->params.run_forever;
    i64 prev_render = 0;
    bool display_fenster = wld->params.visual;

    u32* buf = nullptr;
    struct fenster f = {
        .title = "Ecosystem Simulation",
        .width = (wld->w * zoom),
        .height = (wld->h * zoom),
        .buf = nullptr,
    };
    if (display_fenster) {
        buf = malloc((sizeof *buf) * wld->w * wld->h * zoom * zoom);
        f.buf = buf;
        fenster_open(&f);

        // Bugfix: no fenster display for first frame.
        fenster_sleep(1000/FPS);
        prev_render = fenster_time();
        render(wld, &f, zoom);
    }

    while (true) {
        if (verbose) {
            if (!forever) {
                fprintf(stdout, "Time %u/%u: Population sizes: { ", wld->step, wld->params.num_steps);
            } else {
                fprintf(stdout, "Time %u: Population sizes: { ", wld->step);
            }
            for (int pop = 0; pop < wld->params.population_count; ++pop) {
                if (pop) {
                    printf(" | ");
                }
                buffer_printf(wld->params.populations[pop].name, stdout);
                fprintf(stdout, ": %u", wld->pop_tally[pop]);
            }
            fprintf(stdout, " }\n");
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
            //fenster_sleep(30);
        }

        if (!forever && wld->step == wld->params.num_steps) {
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

u32 parse_color(buffer *buf) {
    u32 result = 0;
    for (size_t i = 0; i < buf->len; ++i) {
        if ('0' <= buf->p[i] && buf->p[i] <= '9') {
            result <<= 4;
            result += (u32)(buf->p[i] - '0');
        } else if ('A' <= buf->p[i] && buf->p[i] <= 'F') {
            result <<= 4;
            result += 10 + (u32)(buf->p[i] - 'A');
        } else if ('\'' == buf->p[i]) {
            continue;
        } else {
            fprintf(stderr, "Error: Invalid character in color string.\n");
            return WHITE;
        }
    }
    return result;
}

bool config_load(char const* filename, simulation_params* params) {
    bool config_valid = true;

    json_data data;
    if (!json_read_from_file(filename, &data)) {
        fprintf(stderr, "Failed to parse file %s: Invalid JSON format.\n", filename);
        config_valid = false;
    }

    if (config_valid) {
        json_value *jv = {};

        if (!(jv = json_find_child_of_type(data, "populations", JSON_TYPE_ARRAY))) {
            fprintf(stderr, "No 'populations' found.\n");
            config_valid = false;
        } else {
            *params = simulation_params_create(clamp_size_t_u16(json_count_children(jv)));
            json_value *json_pop_params = jv->child;
            size_t popid = 0;
            while (json_pop_params) {
                if(json_pop_params->type != JSON_TYPE_OBJECT) {
                    fprintf(stderr, "Child of 'populations' is not a JSON object.\n");
                    config_valid = false;
                } else {
                    json_value *jvp = {};
                    if (!(jvp = json_find_child_of_type(json_pop_params, "name", JSON_TYPE_STRING))) {
                        fprintf(stderr, "Population has no 'name'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].name = buffer_clone(&jvp->datum.string);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "color", JSON_TYPE_STRING))) {
                        fprintf(stderr, "Population has no 'color'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].color = parse_color(&jvp->datum.string);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "motile", JSON_TYPE_BOOLEAN))) {
                        fprintf(stderr, "Population has no 'motile'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].motile = jvp->datum.boolean;
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "trophic_level", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'trophic_level'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].trophic_level = clamp_i64_u8(jvp->datum.integer);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "initial_population_size", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'initial_population_size'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].initial_population_size = clamp_i64_u32(jvp->datum.integer);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "energy_at_birth", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'energy_at_birth'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].energy_at_birth = clamp_i64_u16(jvp->datum.integer);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "energy_maximum", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'energy_maximum'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].energy_maximum = clamp_i64_u16(jvp->datum.integer);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "energy_threshold_replicate", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'energy_threshold_replicate'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].energy_threshold_replicate = clamp_i64_u16(jvp->datum.integer);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "energy_cost_replicate", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'energy_cost_replicate'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].energy_cost_replicate = clamp_i64_u16(jvp->datum.integer);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "energy_gain", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'energy_gain'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].energy_gain = clamp_i64_u16(jvp->datum.integer);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "energy_cost_move", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'energy_cost_move'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].energy_cost_move = clamp_i64_u16(jvp->datum.integer);
                    }
                    if (!(jvp = json_find_child_of_type(json_pop_params, "replication_space_needed", JSON_TYPE_INTEGER))) {
                        fprintf(stderr, "Population has no 'replication_space_needed'.\n");
                        config_valid = false;
                    } else {
                        params->populations[popid].replication_space_needed = clamp_i64_u8(jvp->datum.integer);
                    }

                }
                json_pop_params = json_pop_params->next;
                ++popid;
            }
        }

        if (!(jv = json_find_child(data, "random_seed")) ||
            !(jv->type == JSON_TYPE_NULL || jv->type == JSON_TYPE_INTEGER)) {
            fprintf(stderr, "Failed to find 'random_seed'.\n");
            config_valid = false;
        } else {
            if (jv->type == JSON_TYPE_NULL) {
                params->rng_seed_given = false;
            } else {
                params->rng_seed_given = true;
                params->rng_seed = (u64)(jv->datum.integer);
            }
        }
        if (!(jv = json_find_child_of_type(data, "width", JSON_TYPE_INTEGER))) {
            fprintf(stderr, "Failed to find 'width'.\n");
            config_valid = false;
        } else {
            params->w = clamp_i64_u16(jv->datum.integer);
        }
        if (!(jv = json_find_child_of_type(data, "height", JSON_TYPE_INTEGER))) {
            fprintf(stderr, "Failed to find 'height'.\n");
            config_valid = false;
        } else {
            params->h = clamp_i64_u16(jv->datum.integer);
        }
        if (!(jv = json_find_child_of_type(data, "visual", JSON_TYPE_BOOLEAN))) {
            fprintf(stderr, "Failed to find 'visual'.\n");
            config_valid = false;
        } else {
            params->visual = jv->datum.boolean;
        }
        if (!(jv = json_find_child_of_type(data, "run_forever", JSON_TYPE_BOOLEAN))) {
            fprintf(stderr, "Failed to find 'run_forever'.\n");
            config_valid = false;
        } else {
            params->run_forever = jv->datum.boolean;
        }
        if (!(jv = json_find_child_of_type(data, "num_steps", JSON_TYPE_INTEGER))) {
            fprintf(stderr, "Failed to find 'num_steps'.\n");
            config_valid = false;
        } else {
            params->num_steps = clamp_i64_u32(jv->datum.integer);
        }
    }

    json_data_destroy(&data);
    if (!config_valid) {
        simulation_params_destroy(params);
    }
    return config_valid;
}

bool config_validate(simulation_params const* params) {
    char const*const error_prefix = "Invalid simulation parameters: ";
    if (params->w < 1 || params->h < 1) {
        fprintf(stderr, "%sInvalid world dimensions.\n", error_prefix);
        return false;
    }
    for (u32 popid = 0; popid < params->population_count; ++popid) {
        population_params const* p_params = &params->populations[popid];
        if (!(0 <= p_params->replication_space_needed && p_params->replication_space_needed <= 8)) {
            fprintf(stderr, "%sParameter 'replication_space_needed' is out of range.\n", error_prefix);
            return false;
        }
    }
    return true;
}


int main(int argc, char* argv[]) {

    /**** Parse command-line arguments. ****/

    if (argc != 2) {
        fprintf(stderr, "Usage: ecosystem <config.json>\n");
        return EXIT_FAILURE;
    }
    char const* const filename = argv[1];


    /**** Import parameters from file. ****/

    simulation_params params = {};
    if (!config_load(filename, &params) || !config_validate(&params)) {
        fprintf(stderr, "Failed to load simulation parameters.\n");
        simulation_params_destroy(&params);
        return EXIT_FAILURE;
    }


    /**** Simulate. ****/

    world wld = {};
    if (!world_create(&wld, params)) {
        fprintf(stderr, "Failed to create world.\n");
    } else {
        const u8 zoom = 4;
        run(&wld, zoom, true);
        world_destroy(&wld);
    }

    simulation_params_destroy(&params);
    return EXIT_SUCCESS;
}
