#include <stdlib.h>
#include <assert.h>
#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_gfxPrimitives_font.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <omp.h>

SDL_Surface *orig_image;
int generation = 0;
int gen_at_last_sec_change = 0, last_sec = 0;
void draw_text(char *txt, int x, int y);

SDL_Surface *surface_from(SDL_Surface *sf, int w, int h)
{
    return SDL_CreateRGBSurface(sf->flags, w, h, 32, sf->format->Rmask, sf->format->Gmask, sf->format->Bmask, sf->format->Amask);
}

SDL_Surface *crop_surface(SDL_Surface* sprite_sheet, int x, int y, int width, int height)
{
    SDL_Surface *surface = surface_from(sprite_sheet, width, height);
    SDL_Rect rect = {x, y, width, height};
    SDL_BlitSurface(sprite_sheet, &rect, surface, 0);
    SDL_FreeSurface(sprite_sheet);
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
    SDL_Surface *err;
    Uint8 errors[64];
} *cur_image = NULL;
void mkimg(struct Image*);

#define NOSQABS(a,b,f) if (a>b)f+=a-b;else f+=b-a
#define ABSOLUTIFY(a,f) f += (a)*(a)
#define JOIN(a,b) a ## b
#define COL_AT_(u,c) (Uint8)((u&fmt->JOIN(c,mask))>>fmt->JOIN(c,shift))<<fmt->JOIN(c,loss)
void fitness (struct Image *image, int error)
{
    int i, end_data = orig_image->h * orig_image->pitch / 4;
    Uint32 da, db, areas[64]={0,};
    Uint32 *timg, *oimg;
    SDL_PixelFormat *fmt;
    struct Image *img;
    Uint8 c_r, c_g, c_b, o_r, o_g, o_b;
    //if (image->fitness != ~0) return;
    image->fitness = 0;
    mkimg(image);
    // need a fast pixel comparison
    fmt = orig_image->format;
    SDL_LockSurface(image->img);
    timg = image->img->pixels;
    SDL_LockSurface(orig_image);
    oimg = orig_image->pixels;
    if (error)
    {
        image->err = surface_from(orig_image, 256, 256);
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
            int wh = ((i>>5)&7)|(((i>>13)&7)<<3);
            assert(wh<64);
            NOSQABS(c_r, o_r, areas[wh]);
            NOSQABS(c_g, o_g, areas[wh]);
            NOSQABS(c_b, o_b, areas[wh]);
            ABSOLUTIFY(c_r - o_r, image->fitness);
            ABSOLUTIFY(c_g - o_g, image->fitness);
            ABSOLUTIFY(c_b - o_b, image->fitness);
        }
        for (i = 0; i < 64; ++ i)
        {
            boxRGBA(image->err, (i&7)<<5, ((i>>3)&7)<<5, ((i&7)<<5)+32, (((i>>3)&7)<<5)+32, areas[i]>>10, 0, 0, 255);
            image->errors[i] = areas[i]>>10;
        }
    }
    else
    {
        int ret;
        Uint8 c_r, c_g, c_b, o_r, o_g, o_b;
        Uint32 da, db;
#pragma omp parallel private(ret, c_r, c_g, c_b, o_r, o_g, o_b, da, db)
        {
            ret = 0;
#  pragma omp for schedule(dynamic, 65536)
            for (i = 0; i < (1<<16); ++ i)
            {
                da = timg[i];
                db = oimg[i];
                c_r = COL_AT_(da, R);
                c_g = COL_AT_(da, G);
                c_b = COL_AT_(da, B);
                o_r = COL_AT_(db, R);
                o_g = COL_AT_(db, G);
                o_b = COL_AT_(db, B);
                ABSOLUTIFY(c_r - o_r, ret);
                ABSOLUTIFY(c_g - o_g, ret);
                ABSOLUTIFY(c_b - o_b, ret);
            }
#  pragma omp critical
            image->fitness += ret;
        }
    }
    SDL_UnlockSurface(image->img);
    SDL_UnlockSurface(orig_image);
}

SDL_Surface *img_lines(struct Image *i)
{
    SDL_Surface *ret = surface_from(i->img, i->img->w, i->img->h);
    int n;
    for (n = 0; n < i->num_polys; ++ n)
        polygonRGBA(ret, &(i->polys[n]->x1), &(i->polys[n]->y1), 6, 90, 190, 255, 255);
    return ret;
}

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

        if (P8[n] < 40) P8[n] += RR(30);
        else if (P8[n] > 50) P8[n] -= RR(30);
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

void free_image(struct Image *i, int error)
{
    int n;
    /* free the surfaces */
    SDL_FreeSurface(i->img);
    if (error)
        SDL_FreeSurface(i->err);
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
    ret->err = i->err;
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

struct Image *thebest(struct Image **images, int num_images, int error)
{
    int i;
    struct Image *ret = images[0];
    if (error)
        SDL_FreeSurface(ret->err);
    for (i = 0; i < num_images; ++ i)
        fitness(images[i], error);
    for (i = 1; i < num_images; ++ i)
    {
        if (ret->fitness > images[i]->fitness)
        {
            free_image(ret, error);
            ret = images[i];
        }
        else free_image(images[i], error);
    }
    return ret;
}

// Relies 32-bit and width=256px TODO
#define PIXEL_AT(x,y) ((y<<10) + (x<<2))
void new_polygon(struct Image *image)
{
    struct Polygon *pl;
    int s = RR(10)+12;
    int x, y;
    if (image->err == NULL)
    {
        x = RR(256-2*s)+s;
        y = RR(256-2*s)+s;
    }
    else
    {
        do
        {
            x = RR(256-2*s)+s;
            y = RR(256-2*s)+s;
        }
        while(RR(256) > image->errors[(x>>5)+((y>>5)<<3)]);
    }
    Uint32 u;
    SDL_PixelFormat *fmt;
    pl = malloc(sizeof(struct Polygon));
    pl->x1 = x-s;
    pl->y1 = y-s;
    pl->x2 = x+s;
    pl->y2 = y-s;
    pl->x3 = x+s;
    pl->y3 = y;
    pl->x4 = x+s;
    pl->y4 = y+s;
    pl->x5 = x-s;
    pl->y5 = y+s;
    pl->x6 = x-s;
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
    add_poly(image, pl);
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
                new_polygon(cur_image);
                break;
            }
        }
    }
    struct Image *ptrs[] = {cur_image,
                            random_change(duplicate_image(cur_image)),
                            random_change(duplicate_image(cur_image))};
    cur_image = thebest(ptrs, 3, !((++generation)%400));
    if (!((generation)%400)) new_polygon(cur_image);
    float cur = ((float)cur_image->fitness)/65536;
    char str[80];
    sprintf(str, "Generation %d, best: %f", generation, cur);
    draw_text(str, 0, 0);
    if (time(NULL) != last_sec)
    {
        sprintf(str, "Speed: %dG/s", generation-gen_at_last_sec_change);
        draw_text(str, 0, 15);
        gen_at_last_sec_change = generation;
        last_sec = time(NULL);
    }
    return 0;
}

TTF_Font *font;
SDL_Surface *screen;

void draw_text(char *txt, int x, int y)
{
    SDL_Color col = {255, 255, 255};
    SDL_Surface *s = TTF_RenderText_Solid(font, txt, col);
    boxRGBA(screen, x, y, x+s->w, y+s->h, 0, 0, 0, 255);
    SDL_Rect rect = {x, y, s->w, s->h};
    SDL_BlitSurface(s, NULL, screen, &rect);
    SDL_FreeSurface(s);
}

/* windows likes this :/ */
#ifdef main
#undef main
#endif

int main (int argc, char *argv[])
{
    SDL_Rect dest = {0, 30, 256, 256};
    char *file;
    //srand(time(NULL));
    if (argc < 2)
    {
        printf(
"Usage:\n\
    polygon <image to polygonize>\n\n");
        exit (0);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf (stderr, "Unable to init SDL: %s\n", SDL_GetError ());
        exit (1);
    }
    atexit (SDL_Quit);

    if (TTF_Init() == -1)
    {
        printf(stderr, "TTF_Init: %s\n", (const char*)TTF_GetError());
        exit (1);
    }
    font = TTF_OpenFont("cnew.ttf", 12);

    screen = SDL_SetVideoMode (520, 550, 32, SDL_SWSURFACE);
    if (screen == NULL)
    {
        fprintf (stderr, "Unable to set video mode: %s\n", SDL_GetError ());
        exit (1);
    }
    file = argv[1];
    orig_image = IMG_Load (file);
    if (orig_image == NULL)
    {
        fprintf (stderr, "Unable to load %s: %s\n", file, SDL_GetError ());
        exit (1);
    }
    cur_image = malloc(sizeof(*cur_image));
    memset(cur_image, 0, sizeof(*cur_image));
    cur_image->fitness = ~0;

    orig_image = crop_surface(orig_image, 20, 20, 256, 256);
    cur_image->img = surface_from(orig_image, 256, 256);
    SDL_BlitSurface(orig_image, NULL, screen, &dest);
    SDL_UpdateRects(screen, 1, &dest);
    dest.x = 264;
    mkimg(cur_image);
    do
    {
        dest.x = 264;
        dest.y = 30;
        SDL_BlitSurface(cur_image->img, NULL, screen, &dest);
        dest.y = 294;
        SDL_BlitSurface(cur_image->err, NULL, screen, &dest);
        dest.x = 0;
        SDL_Surface *s = img_lines(cur_image);
        SDL_BlitSurface(s, NULL, screen, &dest);
        SDL_FreeSurface(s);
        SDL_UpdateRect(screen, 0, 0, 0, 0);
        SDL_Delay(1);
    }
    while (main_loop() == 0);
    printf("Exiting...\n");
    SDL_FreeSurface(orig_image);
    SDL_FreeSurface(screen);
    free_image(cur_image, 1);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
} 

