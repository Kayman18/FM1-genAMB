/* Control-rate adaptation of Don Cross's SlothCircuit.hpp (CC0-1.0),
 * github.com/mhetrick/nonlinearcircuits. Circuit design by Andrew Fitch.
 * Balance knobs are centred; CV inputs are unpatched. */
typedef struct { float x, w, y; } sloth_node_t;
typedef struct {
    sloth_node_t node[3];
    float fraction, previous, next;
} triple_sloth_t;

static void sloth_reset(triple_sloth_t *s)
{
    *s = (triple_sloth_t){0};
    s->node[1].w = 0.017f;
    s->node[2].w = -0.023f;
}

static sloth_node_t sloth_slope(sloth_node_t v, float gain)
{
    float comparator = v.y > 0.0f ? 11.38f : -10.64f;
    return (sloth_node_t){
        0.5f*v.y - comparator/9.4f - gain*v.w,
        0.2f*v.x - (0.4f + 0.04f*gain)*v.w,
        -v.w/0.142f
    };
}

static void sloth_integrate(sloth_node_t *v, float dt, float gain)
{
    sloth_node_t a = sloth_slope(*v, gain);
    sloth_node_t mid = {v->x + 0.5f*dt*a.x, v->w + 0.5f*dt*a.w, v->y + 0.5f*dt*a.y};
    sloth_node_t b = sloth_slope(mid, gain);
    v->x += dt*b.x;
    v->w += dt*b.w;
    v->y += dt*b.y;
}

static float sloth_step(triple_sloth_t *s, float elapsed_periods)
{
    /* Fixed steps in circuit time keep both buffer sizes on the same
     * trajectory. Interpolate the bipolar Z-sum between model steps. */
    const float dt = 0.125f;
    s->fraction += elapsed_periods * 128.0f;
    while (s->fraction >= 1.0f) {
        s->fraction -= 1.0f;
        sloth_integrate(&s->node[0], dt, 100.0f/21.0f);
        sloth_integrate(&s->node[1], dt*0.27391343f, 100.0f/21.0f);
        sloth_integrate(&s->node[2], dt*0.0096971184f, 5.0f);
        s->previous = s->next;
        s->next = fm_clampf(0.07f*(-s->node[0].y + s->node[1].y + s->node[2].y), -1.0f, 1.0f);
    }
    return s->previous + s->fraction*(s->next - s->previous);
}
