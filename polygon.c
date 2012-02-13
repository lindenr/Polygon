#include <stdlib.h>
#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>

SDL_Surface *orig_image;
float cur_best = 10000;

SDL_Surface *surface_from(SDL_Surface *sf, int w, int h)
{
    return SDL_CreateRGBSurface(sf->flags, w, h, 32, sf->format->Rmask, sf->format->Gmask, sf->format->Bmask, sf->format->Amask);
}

SDL_Surface *crop_surface(SDL_Surface* sprite_sheet, int x, int y, int width, int height)
{
    SDL_Surface *surface = surface_from(sprite_sheet, width, height);
    SDL_Rect rect = {x, y, width, height};
    SDL_BlitSurface(sprite_sheet, &rect, surface, 0);
    return surface;
}

struct Polygon
{
    Uint16 x1, x2, x3, x4, x5, x6, y1, y2, y3, y4, y5, y6;
    Uint8 r, g, b, a;
};

struct Image
{
    Uint32 fitness;
    SDL_Surface *img;
    struct Polygon **polys;
    int num_polys;
    Uint8 br, bg, bb;
} *cur_image = NULL;
#define RR(n) ((int)((((0.0+n)*rand()) / (RAND_MAX + 1.0))))
struct Image *random_change(struct Image *i)
{
    int n;
    if (RR (10) && i->num_polys > 0)
    {
        int p = i->num_polys;
        /* p will be > 0 if we get to here */
        /* This makes the chosen poly most likely to be the most recently-made one. */
        while (--p) if (!RR(2)) break;
        Uint16 *P = &(i->polys[p]->x1);
        Sint16 p16;
        Uint8  *P8 = &(i->polys[p]->r);
        for (n = 0; n < 12; ++ n)
            if (!RR(3))
            {
                p16 = P[n];
                p16 += RR(30) - 15;
                if (p16 >= 0 && p16 < 256) P[n] = p16;
            }

        for (n = 0; n < 3; ++ n)
        {
            if (!RR(3))
                P8[n] += RR(30) - 15;
        }

        if (P8[n] < 40) P8[n] += RR(10);
        else if (P8[n] > 50) P8[n] -= RR(10);
    }
    else if (!RR (3))
        i->br += RR(20) - 10;
    else if (!RR (2))
        i->bg += RR(20) - 10;
    else
        i->bb += RR(20) - 10;
    return i;
}

struct Image *add_poly(struct Image *i, struct Polygon *p)
{
    int n;
    for (n = 0; n < i->num_polys; ++ n)
        if (!i->polys[n]) break;
    if (n < i->num_polys)
    {
        i->polys[n] = p;
        return i;
    }
    struct Polygon **new_polys = malloc(sizeof(struct Polygon *)*(i->num_polys + 1));
    memcpy(new_polys, i->polys, sizeof(struct Polygon *)*(i->num_polys));
    free(i->polys);
    i->polys = new_polys;
    i->polys[i->num_polys] = p;
    ++ i->num_polys;
    return i;
}

void free_image(struct Image *i)
{
    int n;
    /* free the surface */
    SDL_FreeSurface(i->img);
    /* All the polys */
    for (n = 0; n < i->num_polys; ++ n)
        free(i->polys[n]);
    /* The poly-pointer */
    if (i->num_polys)
        free(i->polys);
    /* The image iteslf */
    free(i);
}

struct Image *duplicate_image(struct Image* i)
{
    int n;
    struct Image *ret = malloc(sizeof(struct Image));
    ret->fitness = ~0;
    /* Blank surface */
    ret->img = surface_from(i->img, i->img->w, i->img->h);
    ret->polys = malloc(sizeof(struct Polygon*)*i->num_polys);
    for (n = 0; n < i->num_polys; ++ n)
    {
        ret->polys[n] = malloc(sizeof(struct Polygon));
        memcpy(ret->polys[n], i->polys[n], sizeof(struct Polygon));
    }
    ret->num_polys = i->num_polys;
    ret->br = i->br;
    ret->bg = i->bg;
    ret->bb = i->bb;
    return ret;
}

void mkimg (struct Image *image)
{
    SDL_Surface *ret = image->img;
    struct Polygon **polys = image->polys;
    int num_polys = image->num_polys;
    int i;
    SDL_FillRect(ret, NULL, SDL_MapRGB(ret->format, image->br, image->bg,  image->bb));
    for (i = 0; i < num_polys; ++ i)
        filledPolygonRGBA(ret,
                          &polys[i]->x1, &polys[i]->y1, 6,
                          polys[i]->r, polys[i]->g, polys[i]->b, polys[i]->a);
}

#define ABSOLUTIFY(a,b) \
    if (a > b)  \
    image->fitness += a - b;\
    else \
    image->fitness += b - a

#define JOIN(a,b) a ## b
#define COL_AT_(u,c) (Uint8)((u&fmt->JOIN(c,mask))>>fmt->JOIN(c,shift))<<fmt->JOIN(c,loss)
void fitness (struct Image *image)
{
    int i, end_data = orig_image->h * orig_image->pitch / 4;
    Uint32 *timg, *oimg, da, db;
    Uint8 c_r, c_g, c_b, o_r, o_g, o_b;
    SDL_PixelFormat *fmt = orig_image->format;
    //if (image->fitness != ~0) return;
    image->fitness = 0;
    mkimg(image);
    // need a fast pixel comparison
    SDL_LockSurface(image->img);
    timg = image->img->pixels;
    SDL_LockSurface(orig_image);
    oimg = orig_image->pixels;
    for (i = 0; i < end_data; ++ i)
    {
        da = timg[i];
        db = oimg[i];
        c_r = COL_AT_(da, R);
        c_g = COL_AT_(da, G);
        c_b = COL_AT_(da, B);
        o_r = COL_AT_(db, R);
        o_g = COL_AT_(db, G);
        o_b = COL_AT_(db, B);
        ABSOLUTIFY(c_r, o_r);
        ABSOLUTIFY(c_g, o_g);
        ABSOLUTIFY(c_b, o_b);
    }
    SDL_UnlockSurface(image->img);
    SDL_UnlockSurface(orig_image);
}

struct Image *thebest(struct Image **images, int num_images)
{
    int i;
    struct Image *ret = images[0];
    for (i = 0; i < num_images; ++ i)
        fitness(images[i]);
    for (i = 1; i < num_images; ++ i)
    {
        if (ret->fitness > images[i]->fitness)
        {
            free_image(ret);
            ret = images[i];
        }
        else free_image(images[i]);
    }
    return ret;
}

// Relies 32-bit and width=256px TODO
#define PIXEL_AT(x,y) ((y<<10) + (x<<2))
void new_polygon()
{
    struct Polygon *pl;
    int s = RR(10)+12;
    int x = RR(256-2*s)+s, y = RR(256-2*s)+s;
    Uint32 u;
    SDL_PixelFormat *fmt;
    pl = malloc(sizeof(struct Polygon));
    pl->x1 = x-s;
    pl->y1 = y-s;
    pl->x2 = x+s;
    pl->y2 = y-s;
    pl->x3 = x+s+s;
    pl->y3 = y;
    pl->x4 = x+s;
    pl->y4 = y+s;
    pl->x5 = x-s;
    pl->y5 = y+s;
    pl->x6 = x-s-s;
    pl->y6 = y;
    SDL_LockSurface (orig_image);
    fmt = orig_image->format;
    int wh = PIXEL_AT(x, y);
    //printf("Location: %d\n", wh);
    //SDL_Delay(1000);
    u = *(Uint32*)orig_image->pixels+wh;
    SDL_UnlockSurface (orig_image);
    pl->r = COL_AT_(u,R);
    pl->g = COL_AT_(u,G);
    pl->b = COL_AT_(u,B);
    pl->a = 35;
    add_poly(cur_image, pl);
}

int main_loop()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
            {
                return 1;
            }
            case SDL_KEYDOWN:
            {
                new_polygon();
                break;
            }
        }
    }
    struct Image *ptrs[] = {cur_image,
                            random_change(duplicate_image(cur_image)),
                            random_change(duplicate_image(cur_image))};
    cur_image = thebest(ptrs, 3);
    float cur = ((float)cur_image->fitness)/65536;
    if (cur < cur_best)
        printf("Current best: %f\n", cur);
    cur_best = cur;
    return 0;
}

int main (int argc, char *argv[])
{
    SDL_Surface *screen;
    SDL_Rect dest = {0, 30, 256, 256};
    char *file;
    srand(time(NULL));
    if (argc < 2)
    {
        printf("Usage:\n     polygon <file to polygonize>\n\n");
        exit (0);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf (stderr, "Unable to init SDL: %s\n", SDL_GetError ());
        exit (1);
    }
    atexit (SDL_Quit);

    screen = SDL_SetVideoMode (520, 550, 32, SDL_SWSURFACE);
    if (screen == NULL)
    {
        fprintf (stderr, "Unable to set video mode: %s\n", SDL_GetError ());
        exit (1);
    }
    file = argv[1];
    orig_image = SDL_LoadBMP (file);
    if (orig_image == NULL)
    {
        fprintf (stderr, "Unable to load %s: %s\n", file, SDL_GetError ());
        exit (1);
    }
    cur_image = malloc(sizeof(*cur_image));
    memset(cur_image, 0, sizeof(*cur_image));
    cur_image->fitness = ~0;

    orig_image = crop_surface(orig_image, 128, 128, 256, 256);
    cur_image->img = surface_from(orig_image, 256, 256);
    SDL_BlitSurface(orig_image, NULL, screen, &dest);
    SDL_UpdateRects(screen, 1, &dest);
    dest.x = 264;
    mkimg(cur_image);
    do
    {
        SDL_BlitSurface(cur_image->img, NULL, screen, &dest);
        SDL_UpdateRects(screen, 1, &dest);
        SDL_Delay(1);
    }
    while (main_loop() == 0);
    printf("Exiting...\n");
} 

