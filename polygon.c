#include <stdlib.h>
#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>

SDL_Surface *orig_image;

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
    Uint16 x1, x2, x3, x4, y1, y2, y3, y4;
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
        int p = RR(i->num_polys);
        if (!RR(3))
        {
            i->polys[p]->x1 += RR(30) - 15;
            i->polys[p]->x1 %= 256;
        }
        if (!RR(3))
        {
            i->polys[p]->y1 += RR(30) - 15;
            i->polys[p]->y1 %= 256;
        }
        if (!RR(3))
        {
            i->polys[p]->x2 += RR(30) - 15;
            i->polys[p]->x2 %= 256;
        }
        if (!RR(3))
        {
            i->polys[p]->y2 += RR(30) - 15;
            i->polys[p]->y2 %= 256;
        }
        if (!RR(3))
        {
            i->polys[p]->x3 += RR(30) - 15;
            i->polys[p]->x3 %= 256;
        }
        if (!RR(3))
        {
            i->polys[p]->y3 += RR(30) - 15;
            i->polys[p]->y3 %= 256;
        }
        if (!RR(3))
        {
            i->polys[p]->x4 += RR(30) - 15;
            i->polys[p]->x4 %= 256;
        }
        if (!RR(3))
        {
            i->polys[p]->y4 += RR(30) - 15;
            i->polys[p]->y4 %= 256;
        }
        if (!RR(3))
        {
            i->polys[p]->r += RR(30) - 15;
        }
        if (!RR(3))
        {
            i->polys[p]->g += RR(30) - 15;
        }
        if (!RR(3))
        {
            i->polys[p]->b += RR(30) - 15;
        }
        if (!RR(3))
        {
            i->polys[p]->a += RR(30) - 15;
        }
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
                          &polys[i]->x1, &polys[i]->y1, 4,
                          polys[i]->r, polys[i]->g, polys[i]->b, polys[i]->a);
}

void fitness (struct Image *image)
{
    int i, end_data = orig_image->h * orig_image->pitch;
    char *timg, *oimg;
    //if (image->fitness != ~0) return;
    image->fitness = 0;
    mkimg(image);
    // need a fast pixel comparison
    SDL_LockSurface(image->img);
    timg = image->img->pixels;
    SDL_LockSurface(orig_image);
    oimg = orig_image->pixels;
    for (i = 0; i < end_data; ++ i)
        /* Seems to be faster than fabs() */
        if (timg[i] > oimg[i])
            image->fitness += timg[i] - oimg[i];
        else
            image->fitness += oimg[i] - timg[i];
    printf("Fitness: %d\n", image->fitness);
    SDL_UnlockSurface(image->img);
    SDL_UnlockSurface(orig_image);
}

struct Image *thebest(struct Image **images, int num_images)
{
    int i;
    struct Image *ret = images[0];
    printf("FITNESS\n");
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
int main_loop()
{
    SDL_Event event;
    struct Polygon *pl;
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
                int x = RR(166)+45, y = RR(166)+45, s = RR(30)+15;
                pl = malloc(sizeof(struct Polygon));
                pl->x1 = x-s;
                pl->y1 = y-s;
                pl->x2 = x+s;
                pl->y2 = y-s;
                pl->x3 = x+s;
                pl->y3 = y+s;
                pl->x4 = x-s;
                pl->y4 = y+s;
                SDL_LockSurface (orig_image);
                int wh = PIXEL_AT(x, y);
                //printf("Location: %d\n", wh);
                //SDL_Delay(1000);
                pl->r = ((char*)orig_image->pixels)[wh+0];
                pl->g = ((char*)orig_image->pixels)[wh+1];
                pl->b = ((char*)orig_image->pixels)[wh+2];
                pl->a = RR(128)+64;
                SDL_UnlockSurface (orig_image);
                add_poly(cur_image, pl);
                break;
            }
        }
    }
    struct Image *ptrs[] = {cur_image,
                            random_change(duplicate_image(cur_image)),
                            random_change(duplicate_image(cur_image))};
    cur_image = thebest(ptrs, 3);
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

    orig_image = crop_surface(orig_image, 0, 0, 256, 256);
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

