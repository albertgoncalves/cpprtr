#include "prelude.hpp"

#include "bmp.hpp"
#include "color.hpp"
#include "math.hpp"
#include "random.hpp"

#define MAX_THREADS 8

#define N_BOUNCES         32u
#define SAMPLES_PER_PIXEL 32u
#define EPSILON           0.001f

#define X_BLOCKS     8u
#define Y_BLOCKS     8u
#define BLOCK_WIDTH  (IMAGE_WIDTH / X_BLOCKS)
#define BLOCK_HEIGHT (IMAGE_HEIGHT / Y_BLOCKS)
#define N_BLOCKS     (X_BLOCKS * Y_BLOCKS)

constexpr f32 FLOAT_WIDTH = static_cast<f32>(IMAGE_WIDTH);
constexpr f32 FLOAT_HEIGHT = static_cast<f32>(IMAGE_HEIGHT);

#define VERTICAL_FOV 90.0f
#define APERTURE     0.1f
#define ASPECT_RATIO (FLOAT_WIDTH / FLOAT_HEIGHT)
#define LENS_RADIUS  (APERTURE / 2.0f)

#define LOOK_FROM ((Vec3){-0.5f, 0.75f, -0.25f})
#define LOOK_AT   ((Vec3){0.0f, 0.0f, -1.0f})
#define UP        ((Vec3){0.0f, 1.0f, 0.0f})

enum Material {
    LAMBERTIAN = 0u,
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
    Pixel*        buffer;
    const Block*  blocks;
    const Camera* camera;
};

struct Memory {
    BmpImage image;
    Thread   threads[MAX_THREADS];
    Block    blocks[N_BLOCKS];
};

static u16Atomic BLOCK_INDEX;
static u16Atomic RNG_INCREMENT;

static const Sphere SPHERES[] = {
    {{0.0f, -500.5f, -1.0f}, {0.675f, 0.675f, 0.675f}, 500.0f, {}, LAMBERTIAN},
    {{0.0f, 0.0f, -1.0f}, {0.3f, 0.7f, 0.3f}, 0.5f, {}, LAMBERTIAN},
    {{0.0f, 0.0f, 0.35f}, {0.3f, 0.3f, 0.7f}, 0.5f, {}, LAMBERTIAN},
    {{0.0f, 0.0f, -2.0f}, {0.7f, 0.3f, 0.3f}, 0.5f, {}, LAMBERTIAN},
    {{1.15f, 0.0f, -0.85f}, {0.8f, 0.8f, 0.8f}, 0.5f, {0.025f}, METAL},
    {{1.0f, 0.0f, 0.25f}, {}, 0.5f, {1.5f}, DIELECTRIC},
    {{1.0f, 0.0f, 0.25f}, {}, -0.475f, {1.5f}, DIELECTRIC},
    {{-1.0f, 0.0f, -0.35f}, {}, 0.5f, {1.5f}, DIELECTRIC},
    {{-1.0f, 0.0f, -0.35f}, {}, -0.4f, {1.5f}, DIELECTRIC},
    {{-1.25f, 0.0f, -1.75f}, {}, 0.5f, {1.5f}, DIELECTRIC},
    {{-1.25f, 0.0f, -1.75f}, {}, -0.4f, {1.5f}, DIELECTRIC},
};

#define N_SPHERES (sizeof(SPHERES) / sizeof(SPHERES[0]))

static inline void set_hit(const Sphere* sphere,
                           const Ray*    ray,
                           Hit*          hit,
                           f32           t) {
    hit->t = t;
    const Vec3 point = ray->origin + (ray->direction * t);
    hit->point = point;
    const Vec3 outward_normal = (point - sphere->center) / sphere->radius;
    const bool front_face = dot(ray->direction, outward_normal) < 0.0f;
    hit->front_face = front_face;
    hit->normal = front_face ? outward_normal : -outward_normal;
    hit->material = sphere->material;
    hit->albedo = sphere->albedo;
    hit->features = sphere->features;
}

static bool get_hit(const Sphere* sphere,
                    const Ray*    ray,
                    Hit*          hit,
                    f32           t_max) {
    const Vec3 offset = ray->origin - sphere->center;
    const f32  a = dot(ray->direction, ray->direction);
    const f32  half_b = dot(offset, ray->direction);
    const f32  c = dot(offset, offset) - (sphere->radius * sphere->radius);
    const f32  discriminant = (half_b * half_b) - (a * c);
    if (0.0f < discriminant) {
        const f32 root = sqrtf(discriminant);
        f32       t = (-half_b - root) / a;
        if ((EPSILON < t) && (t < t_max)) {
            set_hit(sphere, ray, hit, t);
            return true;
        }
        t = (-half_b + root) / a;
        if ((EPSILON < t) && (t < t_max)) {
            set_hit(sphere, ray, hit, t);
            return true;
        }
    }
    return false;
}

static Vec3 get_random_vec3(PcgRng* rng) {
    return {
        get_random_f32(rng),
        get_random_f32(rng),
        get_random_f32(rng),
    };
}

static Vec3 get_random_in_unit_sphere(PcgRng* rng) {
    for (;;) {
        const Vec3 point = (get_random_vec3(rng) * 2.0f) - 1.0f;
        if (dot(point, point) < 1.0f) {
            return point;
        }
    }
}

static Vec3 get_random_unit_vector(PcgRng* rng) {
    const f32 a = get_random_f32(rng) * 2.0f * PI;
    const f32 z = (get_random_f32(rng) * 2.0f) - 1.0f;
    const f32 r = sqrtf(1.0f - (z * z));
    return {
        r * cosf(a),
        r * sinf(a),
        z,
    };
}

static RgbColor get_color(const Ray* ray, PcgRng* rng) {
    Ray      last_ray = *ray;
    RgbColor attenuation = {
        1.0f,
        1.0f,
        1.0f,
    };
    for (u8 _ = 0u; _ < N_BOUNCES; ++_) {
        Hit  last_hit = {};
        Hit  nearest_hit = {};
        bool hit_anything = false;
        f32  t_nearest = F32_MAX;
        for (u8 i = 0u; i < N_SPHERES; ++i) {
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
                const f32 etai_over_etat =
                    nearest_hit.front_face
                        ? 1.0f / nearest_hit.features.refractive_index
                        : nearest_hit.features.refractive_index;
                const Vec3 direction = unit(last_ray.direction);
                const f32  cos_theta =
                    fminf(dot(-direction, nearest_hit.normal), 1.0f);
                const f32 sin_theta = sqrtf(1.0f - (cos_theta * cos_theta));
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
            const f32 t = 0.5f * (unit(last_ray.direction).y + 1.0f);
            RgbColor  color = {
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
        const Vec3 point = {
            (get_random_f32(rng) * 2.0f) - 1.0f,
            (get_random_f32(rng) * 2.0f) - 1.0f,
            0.0f,
        };
        if (dot(point, point) < 1.0f) {
            return point;
        }
    }
}

#define RGB_COLOR_SCALE 255.0f

static void render_block(const Camera* camera,
                         Pixel*        pixels,
                         Block         block,
                         PcgRng*       rng) {
    for (u32 j = block.start.y; j < block.end.y; ++j) {
        const u32 j_offset = j * IMAGE_WIDTH;
        for (u32 i = block.start.x; i < block.end.x; ++i) {
            RgbColor color = {};
            for (u8 _ = 0u; _ < SAMPLES_PER_PIXEL; ++_) {
                const f32 x =
                    (static_cast<f32>(i) + get_random_f32(rng)) / FLOAT_WIDTH;
                const f32 y =
                    (static_cast<f32>(j) + get_random_f32(rng)) / FLOAT_HEIGHT;
                const Vec3 lens_point = LENS_RADIUS * random_in_unit_disk(rng);
                const Vec3 lens_offset =
                    (camera->u * lens_point.x) + (camera->v * lens_point.y);
                const Ray ray = {
                    camera->origin + lens_offset,
                    (camera->bottom_left + (x * camera->horizontal) +
                     (y * camera->vertical)) -
                        camera->origin - lens_offset,
                };
                color += get_color(&ray, rng);
            }
            color /= static_cast<f32>(SAMPLES_PER_PIXEL);
            clamp(&color, 0.0f, 1.0f);
            pixels[i + j_offset] = {
                static_cast<u8>(RGB_COLOR_SCALE * sqrtf(color.blue)),
                static_cast<u8>(RGB_COLOR_SCALE * sqrtf(color.green)),
                static_cast<u8>(RGB_COLOR_SCALE * sqrtf(color.red)),
            };
        }
    }
}

static u64 get_microseconds() {
    TimeValue time;
    gettimeofday(&time, nullptr);
    return static_cast<u64>(time.tv_usec);
}

static void* thread_render(void* payload) {
    Pixel*        buffer = reinterpret_cast<Payload*>(payload)->buffer;
    const Block*  blocks = reinterpret_cast<Payload*>(payload)->blocks;
    const Camera* camera = reinterpret_cast<Payload*>(payload)->camera;
    PcgRng        rng = {};
    set_seed(&rng, get_microseconds(), RNG_INCREMENT.fetch_add(1u, SEQ_CST));
    for (;;) {
        const u16 index = BLOCK_INDEX.fetch_add(1u, SEQ_CST);
        if (N_BLOCKS <= index) {
            return nullptr;
        }
        render_block(camera, buffer, blocks[index], &rng);
    }
}

static void set_pixels(Memory* memory) {
    const f32    theta = degrees_to_radians(VERTICAL_FOV);
    const f32    h = tanf(theta / 2.0f);
    const f32    viewport_height = 2.0f * h;
    const f32    viewport_width = ASPECT_RATIO * viewport_height;
    const Vec3   w = unit(LOOK_FROM - LOOK_AT);
    const Vec3   u = unit(cross(UP, w));
    const Vec3   v = cross(w, u);
    const Vec3   origin = LOOK_FROM;
    const f32    focus_distance = len(LOOK_FROM - LOOK_AT);
    const Vec3   horizontal = focus_distance * viewport_width * u;
    const Vec3   vertical = focus_distance * viewport_height * v;
    const Camera camera = {
        u,
        v,
        origin,
        horizontal,
        vertical,
        origin - (horizontal / 2.0f) - (vertical / 2.0f) -
            (focus_distance * w),
    };
    Payload payload = {
        memory->image.pixels,
        memory->blocks,
        &camera,
    };
    u16 index = 0u;
    for (u32 y = 0u; y < Y_BLOCKS; ++y) {
        for (u32 x = 0u; x < X_BLOCKS; ++x) {
            const Point start = {
                x * BLOCK_WIDTH,
                y * BLOCK_HEIGHT,
            };
            Point end = {
                start.x + BLOCK_WIDTH,
                start.y + BLOCK_HEIGHT,
            };
            end.x = end.x < IMAGE_WIDTH ? end.x : IMAGE_WIDTH;
            end.y = end.y < IMAGE_HEIGHT ? end.y : IMAGE_HEIGHT;
            const Block block = {
                start,
                end,
            };
            memory->blocks[index++] = block;
        }
    }
    i32 n = get_nprocs() - 1;
    if ((n < 2) || (MAX_THREADS < n)) {
        exit(EXIT_FAILURE);
    }
    for (u8 i = 0u; i < n; ++i) {
        pthread_create(&memory->threads[i], nullptr, thread_render, &payload);
    }
    for (u8 i = 0u; i < n; ++i) {
        pthread_join(memory->threads[i], nullptr);
    }
}

i32 main(i32 n, const char** args) {
    printf("sizeof(void*)    : %zu\n"
           "sizeof(Vec3)     : %zu\n"
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
           sizeof(void*),
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
    if (n < 2) {
        exit(EXIT_FAILURE);
    }
    File* file = fopen(args[1u], "wb");
    if (!file) {
        exit(EXIT_FAILURE);
    }
    Memory* memory = reinterpret_cast<Memory*>(calloc(1u, sizeof(Memory)));
    if (!memory) {
        exit(EXIT_FAILURE);
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
