#include <atomic>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>

#include "types.hpp"

#define IMAGE_WIDTH  768u
#define IMAGE_HEIGHT 512u
#define N_PIXELS     393216u

#define FLOAT_WIDTH  768.0f
#define FLOAT_HEIGHT 512.0f

#define N_BOUNCES         32u
#define SAMPLES_PER_PIXEL 32u
#define EPSILON           0.00001f

#define BLOCK_WIDTH  128u
#define BLOCK_HEIGHT 128u
#define X_BLOCKS     6u
#define Y_BLOCKS     4u
#define N_BLOCKS     24u

#define N_THREADS 3u

#define FILEPATH "out/main.bmp"

#include "bmp.hpp"
#include "color.hpp"
#include "math.hpp"
#include "random.hpp"

enum Material {
    LAMBERTIAN,
    METAL,
    DIELECTRIC,
};

union Features {
    f32 fuzz;
    f32 refractive_index;
};

struct Hit {
    Vec3     point;
    Vec3     normal;
    RgbColor albedo;
    f32      t;
    Features features;
    Material material;
    bool     front_face;
};

struct Sphere {
    Vec3     center;
    RgbColor albedo;
    f32      radius;
    Features features;
    Material material;
};

struct Camera {
    Vec3 u;
    Vec3 v;
    Vec3 origin;
    Vec3 horizontal;
    Vec3 vertical;
    Vec3 bottom_left;
};

struct Ray {
    Vec3 origin;
    Vec3 direction;
};

struct Point {
    u32 x;
    u32 y;
};

struct Block {
    Point start;
    Point end;
};

struct Payload {
    Pixel*  buffer;
    Block*  blocks;
    Camera* camera;
};

struct Memory {
    BmpImage image;
    Thread   threads[N_THREADS];
    Block    blocks[N_BLOCKS];
};

static u16Atomic INDEX;

static const Vec3 LOOK_FROM = {
    -0.5f,
    0.75f,
    -0.25f,
};

static const Vec3 LOOK_AT = {
    0.0f,
    0.0f,
    -1.0f,
};

static const Vec3 UP = {
    0.0f,
    1.0f,
    0.0f,
};

#define VERTICAL_FOV 90.0f
#define APERTURE     0.175f
static const f32 ASPECT_RATIO = FLOAT_WIDTH / FLOAT_HEIGHT;
static const f32 LENS_RADIUS = APERTURE / 2.0f;
static const f32 FOCUS_DISTANCE = len(LOOK_FROM - LOOK_AT);

#define N_SPHERES 11u

static const Sphere SPHERES[N_SPHERES] = {
    {{0.0f, -50.5f, -1.0f}, {0.675f, 0.675f, 0.675f}, 50.0f, 0.0f, LAMBERTIAN},
    {{0.0f, 0.0f, -1.0f}, {0.3f, 0.7f, 0.3f}, 0.5f, 0.0f, LAMBERTIAN},
    {{0.0f, 0.0f, 0.35f}, {0.3f, 0.3f, 0.7f}, 0.5f, 0.0f, LAMBERTIAN},
    {{0.0f, 0.0f, -2.0f}, {0.7f, 0.3f, 0.3f}, 0.5f, 0.0f, LAMBERTIAN},
    {{1.15f, 0.0f, -0.85f}, {0.8f, 0.8f, 0.8f}, 0.5f, 0.025f, METAL},
    {{1.0f, 0.0f, 0.25f}, {}, 0.5f, 1.5f, DIELECTRIC},
    {{1.0f, 0.0f, 0.25f}, {}, -0.475f, 1.5f, DIELECTRIC},
    {{-1.0f, 0.0f, -0.35f}, {}, 0.5f, 1.5f, DIELECTRIC},
    {{-1.0f, 0.0f, -0.35f}, {}, -0.4f, 1.5f, DIELECTRIC},
    {{-1.25f, 0.0f, -1.75f}, {}, 0.5f, 1.5f, DIELECTRIC},
    {{-1.25f, 0.0f, -1.75f}, {}, -0.4f, 1.5f, DIELECTRIC},
};

static Vec3 at(const Ray* ray, f32 t) {
    return ray->origin + (ray->direction * t);
}

#define SET_HIT                                                      \
    hit->t = t;                                                      \
    Vec3 point = at(ray, t);                                         \
    hit->point = point;                                              \
    Vec3 outward_normal = (point - sphere->center) / sphere->radius; \
    bool front_face = dot(ray->direction, outward_normal) < 0;       \
    hit->front_face = front_face;                                    \
    hit->normal = front_face ? outward_normal : -outward_normal;     \
    hit->material = sphere->material;                                \
    hit->albedo = sphere->albedo;                                    \
    hit->features = sphere->features

static bool get_hit(const Sphere* sphere,
                    const Ray*    ray,
                    Hit*          hit,
                    f32           t_max) {
    Vec3 offset = ray->origin - sphere->center;
    f32  a = dot(ray->direction, ray->direction);
    f32  half_b = dot(offset, ray->direction);
    f32  c = dot(offset, offset) - (sphere->radius * sphere->radius);
    f32  discriminant = (half_b * half_b) - (a * c);
    if (0.0f < discriminant) {
        f32 root = sqrtf(discriminant);
        f32 t = (-half_b - root) / a;
        if ((EPSILON < t) && (t < t_max)) {
            SET_HIT;
            return true;
        }
        t = (-half_b + root) / a;
        if ((EPSILON < t) && (t < t_max)) {
            SET_HIT;
            return true;
        }
    }
    return false;
}

#undef SET_HIT

static Vec3 get_random_vec3(PcgRng* rng) {
    return {
        get_random_f32(rng),
        get_random_f32(rng),
        get_random_f32(rng),
    };
}

static Vec3 get_random_in_unit_sphere(PcgRng* rng) {
    for (;;) {
        Vec3 point = (get_random_vec3(rng) * 2.0f) - 1.0f;
        if (dot(point, point) < 1.0f) {
            return point;
        }
    }
}

static Vec3 get_random_unit_vector(PcgRng* rng) {
    f32 a = get_random_f32(rng) * 2.0f * PI;
    f32 z = (get_random_f32(rng) * 2.0f) - 1.0f;
    f32 r = sqrtf(1.0f - (z * z));
    return {
        r * cosf(a),
        r * sinf(a),
        z,
    };
}

static Vec3 reflect(Vec3 v, Vec3 n) {
    return v - (2.0f * dot(v, n) * n);
}

static Vec3 refract(Vec3 uv, Vec3 n, f32 etai_over_etat) {
    f32  cos_theta = dot(-uv, n);
    Vec3 parallel = etai_over_etat * (uv + (cos_theta * n));
    f32  length_squared = dot(parallel, parallel);
    Vec3 perpendicular;
    if (1.0f <= length_squared) {
        perpendicular = {};
    } else {
        perpendicular = (-sqrtf(1.0f - length_squared)) * n;
    }
    return parallel + perpendicular;
}

static f32 schlick(f32 cosine, f32 refreactive_index) {
    f32 r0 = (1.0f - refreactive_index) / (1.0f + refreactive_index);
    r0 *= r0;
    return r0 + ((1.0f - r0) * powf(1.0f - cosine, 5.0f));
}

static RgbColor get_color(const Ray* ray, PcgRng* rng) {
    Ray      last_ray = *ray;
    RgbColor attenuation = {
        1.0f,
        1.0f,
        1.0f,
    };
    for (u8 _ = 0; _ < N_BOUNCES; ++_) {
        Hit  last_hit = {};
        Hit  nearest_hit = {};
        bool hit_anything = false;
        f32  t_nearest = F32_MAX;
        for (u8 i = 0; i < N_SPHERES; ++i) {
            if (get_hit(&SPHERES[i], &last_ray, &last_hit, t_nearest)) {
                hit_anything = true;
                t_nearest = last_hit.t;
                nearest_hit = last_hit;
            }
        }
        if (hit_anything) {
            switch (nearest_hit.material) {
            case LAMBERTIAN: {
                last_ray = {
                    nearest_hit.point,
                    nearest_hit.normal + get_random_unit_vector(rng),
                };
                attenuation *= nearest_hit.albedo;
                break;
            }
            case METAL: {
                last_ray = {
                    nearest_hit.point,
                    reflect(unit(last_ray.direction), nearest_hit.normal) +
                        (nearest_hit.features.fuzz *
                         get_random_in_unit_sphere(rng)),
                };
                if (dot(last_ray.direction, nearest_hit.normal) <= 0.0f) {
                    return {};
                }
                attenuation *= nearest_hit.albedo;
                break;
            }
            case DIELECTRIC: {
                f32 etai_over_etat =
                    nearest_hit.front_face
                        ? 1.0f / nearest_hit.features.refractive_index
                        : nearest_hit.features.refractive_index;
                Vec3 direction = unit(last_ray.direction);
                f32  cos_theta =
                    fminf(dot(-direction, nearest_hit.normal), 1.0f);
                f32 sin_theta = sqrtf(1.0f - (cos_theta * cos_theta));
                if ((1.0f < (etai_over_etat * sin_theta)) ||
                    (get_random_f32(rng) < schlick(cos_theta, etai_over_etat)))
                {
                    last_ray = {
                        nearest_hit.point,
                        reflect(direction, nearest_hit.normal),
                    };
                } else {
                    last_ray = {
                        nearest_hit.point,
                        refract(direction, nearest_hit.normal, etai_over_etat),
                    };
                }
                break;
            }
            }
        } else {
            f32      t = 0.5f * (unit(last_ray.direction).y + 1.0f);
            RgbColor color = {
                t * 0.5f,
                t * 0.7f,
                t,
            };
            color += 1.0f - t;
            return attenuation * color;
        }
    }
    return attenuation;
}

static Vec3 random_in_unit_disk(PcgRng* rng) {
    for (;;) {
        Vec3 point = {
            (get_random_f32(rng) * 2.0f) - 1.0f,
            (get_random_f32(rng) * 2.0f) - 1.0f,
            0.0f,
        };
        if (dot(point, point) < 1.0f) {
            return point;
        }
    }
}

static void set_color(Pixel* pixel, RgbColor color) {
    color /= (f32)SAMPLES_PER_PIXEL;
    clamp(&color, 0.0f, 1.0f);
    pixel->red = (u8)(RGB_COLOR_SCALE * sqrtf(color.red));
    pixel->green = (u8)(RGB_COLOR_SCALE * sqrtf(color.green));
    pixel->blue = (u8)(RGB_COLOR_SCALE * sqrtf(color.blue));
}

static void render_block(Pixel*  pixels,
                         Camera  camera,
                         Block   block,
                         PcgRng* rng) {
    for (u32 j = block.start.y; j < block.end.y; ++j) {
        u32 j_offset = j * IMAGE_WIDTH;
        for (u32 i = block.start.x; i < block.end.x; ++i) {
            RgbColor color = {};
            for (u8 _ = 0; _ < SAMPLES_PER_PIXEL; ++_) {
                f32  x = ((f32)i + get_random_f32(rng)) / FLOAT_WIDTH;
                f32  y = ((f32)j + get_random_f32(rng)) / FLOAT_HEIGHT;
                Vec3 lens_point = LENS_RADIUS * random_in_unit_disk(rng);
                Vec3 lens_offset =
                    (camera.u * lens_point.x) + (camera.v * lens_point.y);
                Ray ray = {
                    camera.origin + lens_offset,
                    (camera.bottom_left + (x * camera.horizontal) +
                     (y * camera.vertical)) -
                        camera.origin - lens_offset,
                };
                color += get_color(&ray, rng);
            }
            set_color(&pixels[i + j_offset], color);
        }
    }
}

static void* thread_render(void* args) {
    Payload* payload = (Payload*)args;
    Pixel*   buffer = payload->buffer;
    Camera   camera = *payload->camera;
    PcgRng   rng = {};
    init_random(&rng);
    for (;;) {
        u16 index = INDEX.fetch_add(1, std::memory_order_seq_cst);
        if (N_BLOCKS <= index) {
            return NULL;
        }
        render_block(buffer, camera, payload->blocks[index], &rng);
    }
}

static void set_pixels(Memory* memory) {
    f32    theta = degrees_to_radians(VERTICAL_FOV);
    f32    h = tanf(theta / 2.0f);
    f32    viewport_height = 2.0f * h;
    f32    viewport_width = ASPECT_RATIO * viewport_height;
    Vec3   w = unit(LOOK_FROM - LOOK_AT);
    Vec3   u = unit(cross(UP, w));
    Vec3   v = cross(w, u);
    Vec3   origin = LOOK_FROM;
    Vec3   horizontal = FOCUS_DISTANCE * viewport_width * u;
    Vec3   vertical = FOCUS_DISTANCE * viewport_height * v;
    Camera camera = {
        u,
        v,
        origin,
        horizontal,
        vertical,
        origin - (horizontal / 2.0f) - (vertical / 2.0f) -
            (FOCUS_DISTANCE * w),
    };
    Payload payload = {
        memory->image.pixels,
        memory->blocks,
        &camera,
    };
    u16 index = 0;
    for (u32 y = 0; y < Y_BLOCKS; ++y) {
        for (u32 x = 0; x < X_BLOCKS; ++x) {
            Point start = {
                x * BLOCK_WIDTH,
                y * BLOCK_HEIGHT,
            };
            Point end = {
                start.x + BLOCK_WIDTH,
                start.y + BLOCK_HEIGHT,
            };
            end.x = end.x < IMAGE_WIDTH ? end.x : IMAGE_WIDTH;
            end.y = end.y < IMAGE_HEIGHT ? end.y : IMAGE_HEIGHT;
            Block block = {
                start,
                end,
            };
            memory->blocks[index++] = block;
        }
    }
    for (u8 i = 0; i < N_THREADS; ++i) {
        pthread_create(&memory->threads[i], NULL, thread_render, &payload);
    }
    for (u8 i = 0; i < N_THREADS; ++i) {
        pthread_join(memory->threads[i], NULL);
    }
}

int main() {
    printf("sizeof(Vec3)     : %zu\n"
           "sizeof(RgbColor) : %zu\n"
           "sizeof(Material) : %zu\n"
           "sizeof(Features) : %zu\n"
           "sizeof(Hit)      : %zu\n"
           "sizeof(Sphere)   : %zu\n"
           "sizeof(Camera)   : %zu\n"
           "sizeof(Ray)      : %zu\n"
           "sizeof(Point)    : %zu\n"
           "sizeof(Block)    : %zu\n"
           "sizeof(Payload)  : %zu\n"
           "sizeof(Memory)   : %zu\n"
           "\n",
           sizeof(Vec3),
           sizeof(RgbColor),
           sizeof(Material),
           sizeof(Features),
           sizeof(Hit),
           sizeof(Sphere),
           sizeof(Camera),
           sizeof(Ray),
           sizeof(Point),
           sizeof(Block),
           sizeof(Payload),
           sizeof(Memory));
    FileHandle* file = fopen(FILEPATH, "wb");
    if (file == NULL) {
        return EXIT_FAILURE;
    }
    Memory* memory = (Memory*)calloc(1, sizeof(Memory));
    if (memory == NULL) {
        return EXIT_FAILURE;
    }
    set_bmp_header(&memory->image.bmp_header);
    set_dib_header(&memory->image.dib_header);
    set_pixels(memory);
    write_bmp(file, &memory->image);
    fclose(file);
    free(memory);
    printf("Done!\n");
    return EXIT_SUCCESS;
}
