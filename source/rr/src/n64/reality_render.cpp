// Copyright 2020 Nuke.YKT, EDuke32 developers
// Polymost code: Copyright Ken Silverman, Copyright (c) 2018, Alex Dawson
#include "build.h"
#include "colmatch.h"
#include "reality.h"

tileinfo_t rt_tileinfo[RT_TILENUM];
int32_t rt_tilemap[MAXTILES];
intptr_t rt_waloff[RT_TILENUM];
char rt_walock[RT_TILENUM];

float rt_viewhorizang;

extern void (*gloadtile_n64)(int32_t dapic, int32_t dapal, int32_t tintpalnum, int32_t dashade, int32_t dameth, pthtyp* pth, int32_t doalloc);

static bool RT_TileLoad(int16_t tilenum);
static void rt_gloadtile_n64(int32_t dapic, int32_t dapal, int32_t tintpalnum, int32_t dashade, int32_t dameth, pthtyp* pth, int32_t doalloc);

void RT_LoadTiles(void)
{
    const int tileinfoOffset = 0x90bf0;
    Blseek(rt_group, tileinfoOffset, SEEK_SET);
    if (Bread(rt_group, rt_tileinfo, sizeof(rt_tileinfo)) != sizeof(rt_tileinfo))
    {
        initprintf("RT_LoadTiles: file read error");
        return;
    }

    Bmemset(rt_tilemap, -1, sizeof(rt_tilemap));
    Bmemset(rt_waloff, 0, sizeof(rt_waloff));
    Bmemset(rt_walock, 0, sizeof(rt_walock));

    for (int i = 0; i < RT_TILENUM; i++)
    {
        auto &t = rt_tileinfo[i];
        t.fileoff = B_BIG32(t.fileoff);
        t.waloff = B_BIG32(t.waloff);
        t.picanm = B_BIG32(t.picanm);
        t.sizx = B_BIG16(t.sizx);
        t.sizy = B_BIG16(t.sizy);
        t.filesiz = B_BIG16(t.filesiz);
        t.dimx = B_BIG16(t.dimx);
        t.dimy = B_BIG16(t.dimy);
        t.flags = B_BIG16(t.flags);
        t.tile = B_BIG16(t.tile);

        rt_tilemap[t.tile] = i;
        tilesiz[t.tile].x = t.sizx;
        tilesiz[t.tile].y = t.sizy;
        tileConvertAnimFormat(t.tile, t.picanm);
        tileUpdatePicSiz(t.tile);
    }

    rt_tileload_callback = RT_TileLoad;
    gloadtile_n64 = rt_gloadtile_n64;
#if 0
    for (auto& t : rt_tileinfo)
    {
        char *data = (char*)tileCreate(t.tile, t.sizx, t.sizy);
        int bufsize = 0;
        if (t.flags & RT_TILE8BIT)
        {
            bufsize = t.dimx * t.dimy;
        }
        else
        {
            bufsize = (t.dimx * t.dimy) / 2 + 32;
        }
        tileConvertAnimFormat(t.tile, t.picanm);
        char *inbuf = (char*)Xmalloc(t.filesiz);
        char *outbuf = (char*)Xmalloc(bufsize);
        Blseek(rt_group, dataOffset+t.fileoff, SEEK_SET);
        Bread(rt_group, inbuf, t.filesiz);
        if (RNCDecompress(inbuf, outbuf) == -1)
        {
            Bmemcpy(outbuf, inbuf, bufsize);
        }
        Xfree(inbuf);
        if (t.flags & RT_TILE8BIT)
        {
            for (int i = 0; i < t.sizx; i++)
            {
                for (int j = 0; j < t.sizy; j++)
                {
                    int ii = t.dimx - 1 - ((t.sizx - i - 1) * t.dimx) / t.sizx;
                    int jj = t.dimy - 1 - ((t.sizy - j - 1) * t.dimy) / t.sizy;
                    data[i*t.sizy+j] = outbuf[j*t.dimx+i];
                }
            }
        }
        else
        {
            int palremap[16];
            char *pix = outbuf+32;
            for (int i = 0; i < 16; i++)
            {
                int t = (outbuf[i*2+1] << 8) + outbuf[i*2];
                int r = (t >> 11) & 31;
                int g = (t >> 6) & 31;
                int b = (t >> 1) & 31;
                int a = (t >> 0) & 1;
                r = (r << 3) + (r >> 2);
                g = (g << 3) + (g >> 2);
                b = (b << 3) + (b >> 2);
                if (a == 0)
                    palremap[i] = 255;
                else
                {
                    palremap[i] = paletteGetClosestColor(r, g, b);
                }
            }
            for (int i = 0; i < t.sizx; i++)
            {
                for (int j = 0; j < t.sizy; j++)
                {
                    int ii = t.dimx - 1 - ((t.sizx - i - 1) * t.dimx) / t.sizx;
                    int jj = t.dimy - 1 - ((t.sizy - j - 1) * t.dimy) / t.sizy;
                    int ix = jj * t.dimx + ii;
                    char b = pix[ix>>1];
                    if (ix&1)
                        b &= 15;
                    else
                        b = (b >> 4) & 15;
                    data[i*t.sizy+j] = palremap[b];
                }
            }
        }
        Xfree(outbuf);
    }
#endif
}

bool RT_TileLoad(int16_t tilenum)
{
    const int dataOffset = 0xc2270;
    int32_t const tileid = rt_tilemap[tilenum];
    if (tileid < 0)
        return false;
    auto &t = rt_tileinfo[tileid];
    int bufsize = 0;
    if (t.flags & RT_TILE8BIT)
    {
        bufsize = t.dimx * t.dimy;
    }
    else
    {
        bufsize = (t.dimx * t.dimy) / 2 + 32;
    }
    if (rt_waloff[tileid] == 0)
    {
        rt_walock[tileid] = CACHE1D_UNLOCKED;
        g_cache.allocateBlock(&rt_waloff[tileid], bufsize, &rt_walock[tileid]);
    }
    if (!rt_waloff[tileid])
        return false;
    char *inbuf = (char*)Xmalloc(t.filesiz);
    Blseek(rt_group, dataOffset+t.fileoff, SEEK_SET);
    Bread(rt_group, inbuf, t.filesiz);
    if (RNCDecompress(inbuf, (char*)rt_waloff[tileid]) == -1)
    {
        Bmemcpy((char*)rt_waloff[tileid], inbuf, bufsize);
    }
    Xfree(inbuf);

    if (waloff[tilenum])
    {
        char *data = (char*)waloff[tilenum];
        char *src = (char*)rt_waloff[tileid];
        if (t.flags & RT_TILE8BIT)
        {
            for (int i = 0; i < t.sizx; i++)
            {
                for (int j = 0; j < t.sizy; j++)
                {
                    int ii = t.dimx - 1 - ((t.sizx - i - 1) * t.dimx) / t.sizx;
                    int jj = t.dimy - 1 - ((t.sizy - j - 1) * t.dimy) / t.sizy;
                    data[i*t.sizy+j] = src[j*t.dimx+i];
                }
            }
        }
        else
        {
            int palremap[16];
            char *pix = src+32;
            for (int i = 0; i < 16; i++)
            {
                int t = (src[i*2+1] << 8) + src[i*2];
                int r = (t >> 11) & 31;
                int g = (t >> 6) & 31;
                int b = (t >> 1) & 31;
                int a = (t >> 0) & 1;
                r = (r << 3) + (r >> 2);
                g = (g << 3) + (g >> 2);
                b = (b << 3) + (b >> 2);
                if (a == 0)
                    palremap[i] = 255;
                else
                {
                    palremap[i] = paletteGetClosestColor(r, g, b);
                }
            }
            for (int i = 0; i < t.sizx; i++)
            {
                for (int j = 0; j < t.sizy; j++)
                {
                    int ii = t.dimx - 1 - ((t.sizx - i - 1) * t.dimx) / t.sizx;
                    int jj = t.dimy - 1 - ((t.sizy - j - 1) * t.dimy) / t.sizy;
                    int ix = jj * t.dimx + ii;
                    char b = pix[ix>>1];
                    if (ix&1)
                        b &= 15;
                    else
                        b = (b >> 4) & 15;
                    data[i*t.sizy+j] = palremap[b];
                }
            }
        }
    }

    return true;
}
void rt_gloadtile_n64(int32_t dapic, int32_t dapal, int32_t tintpalnum, int32_t dashade, int32_t dameth, pthtyp *pth, int32_t doalloc)
{
    int tileid = rt_tilemap[dapic];
    static int32_t fullbrightloadingpass = 0;
    vec2_16_t const & tsizart = tilesiz[dapic];
    vec2_t siz = { 0, 0 }, tsiz = { 0, 0 };
    int const picdim = tsiz.x*tsiz.y;
    char hasalpha = 0;
    tileinfo_t *tinfo = nullptr;

    if (tileid >= 0)
    {
        tinfo = &rt_tileinfo[tileid];
        tsiz.x = tinfo->dimx;
        tsiz.y = tinfo->dimy;
    }

    if (!glinfo.texnpot)
    {
        for (siz.x = 1; siz.x < tsiz.x; siz.x += siz.x) { }
        for (siz.y = 1; siz.y < tsiz.y; siz.y += siz.y) { }
    }
    else
    {
        if ((tsiz.x|tsiz.y) == 0)
            siz.x = siz.y = 1;
        else
            siz = tsiz;
    }

    coltype *pic = (coltype *)Xmalloc(siz.x*siz.y*sizeof(coltype));

    if (tileid < 0 || !rt_waloff[tileid])
    {
        //Force invalid textures to draw something - an almost purely transparency texture
        //This allows the Z-buffer to be updated for mirrors (which are invalidated textures)
        pic[0].r = pic[0].g = pic[0].b = 0; pic[0].a = 1;
        tsiz.x = tsiz.y = 1; hasalpha = 1;
    }
    else
    {
        int is8bit = (tinfo->flags & RT_TILE8BIT) != 0;
        for (bssize_t y = 0; y < siz.y; y++)
        {
            coltype *wpptr = &pic[y * siz.x];
            int32_t y2 = (y < tsiz.y) ? y : y - tsiz.y;

            for (bssize_t x = 0; x < siz.x; x++, wpptr++)
            {
                int32_t dacol;
                int32_t x2 = (x < tsiz.x) ? x : x-tsiz.x;

                if ((dameth & DAMETH_CLAMPED) && (x >= tsiz.x || y >= tsiz.y)) //Clamp texture
                {
                    wpptr->r = wpptr->g = wpptr->b = wpptr->a = 0;
                    continue;
                }

                if (is8bit)
                {
                    dacol = *(char *)(rt_waloff[tileid]+y2*tsiz.x+x2);
                    dacol = rt_palette[dapal][dacol];
                }
                else
                {
                    int o = y2 * tsiz.x + x2;
                    dacol = *(char *)(rt_waloff[tileid]+32+o/2);
                    if (o&1)
                        dacol &= 15;
                    else
                        dacol >>= 4;
                    if (!(dameth & DAMETH_N64_INTENSIVITY))
                    {
                        dacol = *(uint16_t*)(rt_waloff[tileid]+2*dacol);
                        dacol = B_LITTLE16(dacol);
                    }
                }

                if (dameth & DAMETH_N64_INTENSIVITY)
                {
                    int32_t i = (dacol << 4) | dacol;
                    wpptr->r = wpptr->g = wpptr->b = wpptr->a = i;
                    hasalpha = 1;
                }
                else
                {
                    int32_t r = (dacol >> 11) & 31;
                    int32_t g = (dacol >> 6) & 31;
                    int32_t b = (dacol >> 1) & 31;
                    int32_t a = (dacol >> 0) & 1;

                    wpptr->r = (r << 3) + (r >> 2);
                    wpptr->g = (g << 3) + (g >> 2);
                    wpptr->b = (b << 3) + (b >> 2);

                    if (a == 0)
                    {
                        wpptr->a = 0;
                        hasalpha = 1;
                    }
                    else
                        wpptr->a = 255;
                }

#if 0
                bricolor((palette_t *)wpptr, dacol);

                if (tintpalnum >= 0)
                {
                    polytint_t const & tint = hictinting[tintpalnum];
                    polytintflags_t const effect = tint.f;
                    uint8_t const r = tint.r;
                    uint8_t const g = tint.g;
                    uint8_t const b = tint.b;

                    if (effect & HICTINT_GRAYSCALE)
                    {
                        wpptr->g = wpptr->r = wpptr->b = (uint8_t) ((wpptr->r * GRAYSCALE_COEFF_RED) +
                                                                (wpptr->g * GRAYSCALE_COEFF_GREEN) +
                                                                (wpptr->b * GRAYSCALE_COEFF_BLUE));
                    }

                    if (effect & HICTINT_INVERT)
                    {
                        wpptr->b = 255 - wpptr->b;
                        wpptr->g = 255 - wpptr->g;
                        wpptr->r = 255 - wpptr->r;
                    }

                    if (effect & HICTINT_COLORIZE)
                    {
                        wpptr->b = min((int32_t)((wpptr->b) * b) >> 6, 255);
                        wpptr->g = min((int32_t)((wpptr->g) * g) >> 6, 255);
                        wpptr->r = min((int32_t)((wpptr->r) * r) >> 6, 255);
                    }

                    switch (effect & HICTINT_BLENDMASK)
                    {
                        case HICTINT_BLEND_SCREEN:
                            wpptr->b = 255 - (((255 - wpptr->b) * (255 - b)) >> 8);
                            wpptr->g = 255 - (((255 - wpptr->g) * (255 - g)) >> 8);
                            wpptr->r = 255 - (((255 - wpptr->r) * (255 - r)) >> 8);
                            break;
                        case HICTINT_BLEND_OVERLAY:
                            wpptr->b = wpptr->b < 128 ? (wpptr->b * b) >> 7 : 255 - (((255 - wpptr->b) * (255 - b)) >> 7);
                            wpptr->g = wpptr->g < 128 ? (wpptr->g * g) >> 7 : 255 - (((255 - wpptr->g) * (255 - g)) >> 7);
                            wpptr->r = wpptr->r < 128 ? (wpptr->r * r) >> 7 : 255 - (((255 - wpptr->r) * (255 - r)) >> 7);
                            break;
                        case HICTINT_BLEND_HARDLIGHT:
                            wpptr->b = b < 128 ? (wpptr->b * b) >> 7 : 255 - (((255 - wpptr->b) * (255 - b)) >> 7);
                            wpptr->g = g < 128 ? (wpptr->g * g) >> 7 : 255 - (((255 - wpptr->g) * (255 - g)) >> 7);
                            wpptr->r = r < 128 ? (wpptr->r * r) >> 7 : 255 - (((255 - wpptr->r) * (255 - r)) >> 7);
                            break;
                    }
                }
#endif

                //swap r & b so that we deal with the data as BGRA
                uint8_t tmpR = wpptr->r;
                wpptr->r = wpptr->b;
                wpptr->b = tmpR;
            }
        }
    }

    if (doalloc) glGenTextures(1,(GLuint *)&pth->glpic); //# of textures (make OpenGL allocate structure)
    glBindTexture(GL_TEXTURE_2D, pth->glpic);

    // fixtransparency(pic,tsiz,siz,dameth);

#if 0
    if (polymost_want_npotytex(dameth, siz.y) && tsiz.x == siz.x && tsiz.y == siz.y)  // XXX
    {
        const int32_t nextpoty = 1 << ((picsiz[dapic] >> 4) + 1);
        const int32_t ydif = nextpoty - siz.y;
        coltype *paddedpic;

        Bassert(ydif < siz.y);

        paddedpic = (coltype *)Xrealloc(pic, siz.x * nextpoty * sizeof(coltype));

        pic = paddedpic;
        Bmemcpy(&pic[siz.x * siz.y], pic, siz.x * ydif * sizeof(coltype));
        siz.y = tsiz.y = nextpoty;

        npoty = 1;
    }
#endif

    if (!doalloc)
    {
        vec2_t pthSiz2 = pth->siz;
        if (!glinfo.texnpot)
        {
            for (pthSiz2.x = 1; pthSiz2.x < pth->siz.x; pthSiz2.x += pthSiz2.x) { }
            for (pthSiz2.y = 1; pthSiz2.y < pth->siz.y; pthSiz2.y += pthSiz2.y) { }
        }
        else
        {
            if ((pthSiz2.x|pthSiz2.y) == 0)
                pthSiz2.x = pthSiz2.y = 1;
            else
                pthSiz2 = pth->siz;
        }
        if (siz.x > pthSiz2.x ||
            siz.y > pthSiz2.y)
        {
            //POGO: grow our texture to hold the tile data
            doalloc = true;
        }
    }
    uploadtexture(doalloc, siz, GL_BGRA, pic, tsiz,
                    dameth | DAMETH_ARTIMMUNITY |
                    (dapic >= MAXUSERTILES ? (DAMETH_NOTEXCOMPRESS|DAMETH_NODOWNSIZE) : 0) | /* never process these short-lived tiles */
                    (hasalpha ? (DAMETH_HASALPHA|DAMETH_ONEBITALPHA) : 0));

    Xfree(pic);

    polymost_setuptexture(dameth, -1);

    pth->picnum = dapic;
    pth->palnum = dapal;
    pth->shade = dashade;
    pth->effects = 0;
    pth->flags = PTH_N64 | TO_PTH_CLAMPED(dameth) | TO_PTH_NOTRANSFIX(dameth) | (hasalpha*(PTH_HASALPHA|PTH_ONEBITALPHA)) | TO_PTH_N64_INTENSIVITY(dameth);
    pth->hicr = NULL;
    pth->siz = tsiz;
}

GLuint rt_shaderprogram;
GLuint rt_stexsamplerloc = -1;
GLuint rt_stexcombloc = -1;
GLuint rt_stexcomb = 0;
GLuint rt_scolor1loc = 0;
GLfloat rt_scolor1[4] = {
    0.f, 0.f, 0.f, 0.f
};
GLuint rt_scolor2loc = 0;
GLfloat rt_scolor2[4] = {
    0.f, 0.f, 0.f, 0.f
};

void RT_SetShader(void)
{
    glUseProgram(rt_shaderprogram);
    rt_stexsamplerloc = glGetUniformLocation(rt_shaderprogram, "s_texture");
    rt_stexcombloc = glGetUniformLocation(rt_shaderprogram, "u_texcomb");
    rt_scolor1loc = glGetUniformLocation(rt_shaderprogram, "u_color1");
    rt_scolor2loc = glGetUniformLocation(rt_shaderprogram, "u_color2");
    glUniform1i(rt_stexsamplerloc, 0);
    glUniform1f(rt_stexcombloc, rt_stexcomb);
    glUniform4fv(rt_scolor1loc, 1, rt_scolor1);
    glUniform4fv(rt_scolor2loc, 1, rt_scolor2);
}

void RT_SetColor1(int r, int g, int b, int a)
{
    rt_scolor1[0] = r / 255.f;
    rt_scolor1[1] = g / 255.f;
    rt_scolor1[2] = b / 255.f;
    rt_scolor1[3] = a / 255.f;
    glUniform4fv(rt_scolor1loc, 1, rt_scolor1);
}

void RT_SetColor2(int r, int g, int b, int a)
{
    rt_scolor2[0] = r / 255.f;
    rt_scolor2[1] = g / 255.f;
    rt_scolor2[2] = b / 255.f;
    rt_scolor2[3] = a / 255.f;
    glUniform4fv(rt_scolor2loc, 1, rt_scolor2);
}

void RT_SetTexComb(int comb)
{
    if (rt_stexcomb != comb)
    {
        rt_stexcomb = comb;
        glUniform1f(rt_stexcombloc, rt_stexcomb);
    }
}

void RT_GLInit(void)
{
    if (videoGetRenderMode() == REND_CLASSIC)
        return;
    const char* const RT_VERTEX_SHADER =
        "#version 110\n\
         \n\
         varying vec4 v_color;\n\
         varying float v_distance;\n\
         \n\
         const float c_zero = 0.0;\n\
         const float c_one  = 1.0;\n\
         \n\
         void main()\n\
         {\n\
            vec4 eyeCoordPosition = gl_ModelViewMatrix * gl_Vertex;\n\
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n\
            \n\
            eyeCoordPosition.xyz /= eyeCoordPosition.w;\n\
            gl_TexCoord[0] = gl_MultiTexCoord0;\n\
            \n\
            gl_FogFragCoord = abs(eyeCoordPosition.z);\n\
            //gl_FogFragCoord = clamp((gl_Fog.end-abs(eyeCoordPosition.z))*gl_Fog.scale, c_zero, c_one);\n\
            \n\
            v_color = gl_Color;\n\
         }\n\
         ";
    const char* const RT_FRAGMENT_SHADER =
        "#version 110\n\
         #extension GL_ARB_shader_texture_lod : enable\n\
         \n\
         uniform sampler2D s_texture;\n\
         uniform vec4 u_color1;\n\
         uniform vec4 u_color2;\n\
         uniform float u_texcomb;\n\
         \n\
         varying vec4 v_color;\n\
         varying float v_distance;\n\
         \n\
         const float c_zero = 0.0;\n\
         const float c_one  = 1.0;\n\
         const float c_two  = 2.0;\n\
         \n\
         void main()\n\
         {\n\
         #ifdef GL_ARB_shader_texture_lod\n\
            //vec4 color = texture2DGradARB(s_texture, gl_TexCoord[0].xy, dFdx(gl_TexCoord[0].xy), dFdy(gl_TexCoord[0].xy));\n\
            vec4 color = texture2D(s_texture, gl_TexCoord[0].xy);\n\
         #else\n\
            vec2 transitionBlend = fwidth(floor(gl_TexCoord[0].xy));\n\
            transitionBlend = fwidth(transitionBlend)+transitionBlend;\n\
            vec2 texCoord = mix(fract(gl_TexCoord[0].xy), abs(c_one-mod(gl_TexCoord[0].xy+c_one, c_two)), transitionBlend);\n\
            vec4 color = texture2D(s_texture, u_texturePosSize.xy+texCoord);\n\
         #endif\n\
            \n\
            vec4 colorcomb;\n\
            colorcomb.rgb = mix(u_color1.rgb, u_color2.rgb, color.r);\n\
            colorcomb.a = color.a * v_color.a;\n\
            color.rgb = v_color.rgb * color.rgb;\n\
            \n\
            color.a *= v_color.a;\n\
            \n\
            color = mix(color, colorcomb, u_texcomb);\n\
            \n\
            gl_FragData[0] = color;\n\
         }\n\
         ";

    rt_shaderprogram = glCreateProgram();
    GLuint vertexshaderid = polymost2_compileShader(GL_VERTEX_SHADER, RT_VERTEX_SHADER);
    GLuint fragmentshaderid = polymost2_compileShader(GL_FRAGMENT_SHADER, RT_FRAGMENT_SHADER);
    glAttachShader(rt_shaderprogram, vertexshaderid);
    glAttachShader(rt_shaderprogram, fragmentshaderid);
    glLinkProgram(rt_shaderprogram);
}

static float x_vs = 160.f;
static float y_vs = 120.f;
static float x_vt = 160.f;
static float y_vt = 120.f;
static float vp_scale = 1.f;
static float rt_globaldepth;
static int rt_fxtile = 0;

void RT_DisplayTileWorld(float x, float y, float sx, float sy, int16_t picnum, int flags)
{
    int xflip = (flags & 4) != 0;
    int yflip = (flags & 4) != 0;

    sx *= vp_scale;
    sy *= vp_scale;

    float xoff = picanm[picnum].xofs * sx / 6.f;
    if (xflip)
        xoff = -xoff;

    x -= xoff * 2.f;

    float sizx = tilesiz[picnum].x * sx / 6.f;
    float sizy = tilesiz[picnum].y * sy / 6.f;

    if (sizx < 1.f && sizy < 1.f)
        return;

    float x1 = x - sizx;
    float x2 = x + sizx;
    float y1 = y - sizy;
    float y2 = y + sizy;

    if (!waloff[picnum])
        tileLoad(picnum);
    
    int method = DAMETH_CLAMPED | DAMETH_N64 | (rt_fxtile ? DAMETH_N64_INTENSIVITY : 0);
    pthtyp *pth = texcache_fetch(picnum, 0, 0, method);

    if (!pth)
        return;

    glBindTexture(GL_TEXTURE_2D, pth->glpic);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 320.f, 240.f, 0, -1.f, 1.f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(x1, y1, -rt_globaldepth);
    glTexCoord2f(1, 0); glVertex3f(x2, y1, -rt_globaldepth);
    glTexCoord2f(1, 1); glVertex3f(x2, y2, -rt_globaldepth);
    glTexCoord2f(0, 1); glVertex3f(x1, y2, -rt_globaldepth);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

float rt_sky_color[2][3];

static float rt_globalhoriz;
static float rt_globalposx, rt_globalposy, rt_globalposz;
static float rt_globalang;

void setfxcolor(int a1, int a2, int a3, int a4, int a5, int a6)
{
    rt_fxtile = 1;
    glEnable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    RT_SetColor1(a1, a2, a3, 255);
    RT_SetColor2(a4, a5, a6, 255);
    RT_SetTexComb(1);
}

void unsetfxcolor(void)
{
    rt_fxtile = 0;
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    RT_SetTexComb(0);
}

void RT_DisplaySky(void)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    rt_globaldepth = 0.f;
    setfxcolor(rt_sky_color[0][0], rt_sky_color[0][1], rt_sky_color[0][2], rt_sky_color[1][0], rt_sky_color[1][1], rt_sky_color[1][2]);
    glDisable(GL_BLEND);
    RT_DisplayTileWorld(x_vt, y_vt + rt_globalhoriz - 100.f, 52.f, 103.f, 3976, 0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void RT_DisablePolymost()
{
    RT_SetShader();
    RT_SetTexComb(0);
}

void RT_EnablePolymost()
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glDisable(GL_CULL_FACE);
    polymost_resetVertexPointers();
    polymost_setFogEnabled(true);
    polymost_usePaletteIndexing(true);
}

static GLfloat rt_projmatrix[16];


void RT_SetupMatrix(void)
{
    float dx = 512.f * cosf(rt_globalang / (1024.f / fPI));
    float dy = 512.f * sinf(rt_globalang / (1024.f / fPI));
    float dz = -(rt_globalhoriz - 100.f) * 4.f;
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(0.5f, 0.5f, 0.5f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    bgluPerspective(60.f, 4.f/3.f, 5.f, 16384.f);
    bgluLookAt(rt_globalposx * 0.5f, rt_globalposy * 0.5f, rt_globalposz * 0.5f, (rt_globalposx * 0.5f + dx), (rt_globalposy * 0.5f + dy), (rt_globalposz * 0.5f + dz), 0.f, 0.f, -1.f);
    glGetFloatv(GL_PROJECTION_MATRIX, rt_projmatrix);
}

static int rt_globalpicnum = -1;
static vec2f_t rt_uvscale;

static inline int RT_PicSizLog(int siz)
{
    int lg = 0;
    while (siz > (1 << lg))
        lg++;
    return lg;
}

void RT_SetTexture(int tilenum)
{
    if (rt_globalpicnum == tilenum)
        return;
    
    rt_globalpicnum = tilenum;

    tilenum += animateoffs(tilenum, 0);

    int tileid = rt_tilemap[tilenum];

    int method = DAMETH_N64;
    pthtyp *pth = texcache_fetch(tilenum, 0, 0, method);
    if (pth)
        glBindTexture(GL_TEXTURE_2D, pth->glpic);

    if (tileid >= 0)
    {
        auto& tinfo = rt_tileinfo[tileid];
        int logx = RT_PicSizLog(tinfo.dimx);
        int logy = RT_PicSizLog(tinfo.dimy);
        rt_uvscale.x = 1.f/float(32 << logx);
        rt_uvscale.y = 1.f/float(32 << logy);
    }
    else
    {
        rt_uvscale.x = 1.f;
        rt_uvscale.y = 1.f;
    }
}

void RT_DrawCeiling(int sectnum)
{
    auto sect = &rt_sector[sectnum];
    RT_SetTexComb(0);
    RT_SetTexture(sector[sectnum].ceilingpicnum);
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < sect->ceilingvertexnum * 3; i++)
    {
        auto vtx = rt_sectvtx[sect->ceilingvertexptr+i];
        float x = vtx.x;
        float y = vtx.y;
        float z = getceilzofslope(sectnum, vtx.x * 2, vtx.y * 2) / 32.f;
        glTexCoord2f(vtx.u * rt_uvscale.x, vtx.v * rt_uvscale.y); glVertex3f(x, y, z);
    }
    glEnd();
}

void RT_DrawFloor(int sectnum)
{
    auto sect = &rt_sector[sectnum];
    RT_SetTexComb(0);
    int method = DAMETH_N64;
    RT_SetTexture(sector[sectnum].floorpicnum);
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < sect->floorvertexnum * 3; i++)
    {
        auto vtx = rt_sectvtx[sect->floorvertexptr+i];
        float x = vtx.x;
        float y = vtx.y;
        float z = getflorzofslope(sectnum, vtx.x * 2, vtx.y * 2) / 32.f;
        glTexCoord2f(vtx.u * rt_uvscale.x, vtx.v * rt_uvscale.y); glVertex3f(x, y, z);
    }
    glEnd();
}

static rt_vertex_t wallvtx[12];

static int rt_wallcalcres, rt_haswhitewall, rt_hastopwall, rt_hasbottomwall, rt_hasoneway;
static int rt_wallpolycount;

static int globaltileid;
static vec2f_t globaltilescale, globaltilesiz, globaltiledim;

void RT_SetTileGlobals(int tilenum)
{
    int tileid = rt_tilemap[tilenum];
    if (tileid < 0)
        return;
    globaltileid = tileid;
    globaltilescale = { float(rt_tileinfo[tileid].sizx) / float(rt_tileinfo[tileid].dimx),
                        float(rt_tileinfo[tileid].sizy) / float(rt_tileinfo[tileid].dimy) };

    globaltilesiz = { float(rt_tileinfo[tileid].sizx), float(rt_tileinfo[tileid].sizy) };
    globaltiledim = { float(rt_tileinfo[tileid].dimx), float(rt_tileinfo[tileid].dimy) };
}

static float globalxrepeat, globalyrepeat, globalxpanning, globalypanning;
static float globalwallvoffset, globalwallu1, globalwallu2, globalwallv1, globalwallv2, globalwallv3, globalwallv4;

void RT_SetWallGlobals(int wallnum, int cstat)
{
    globalxrepeat = wall[wallnum].xrepeat / globaltilescale.x;
    globalxpanning = wall[wallnum].xpanning / globaltilescale.x;
    globalyrepeat = wall[wallnum].yrepeat / (4.f * globaltilescale.y);
    if (!(cstat & 256))
    {
        globalyrepeat = -globalyrepeat;
        globalypanning = (globaltiledim.y / 256.f) * wall[wallnum].ypanning;
    }
    else
        globalypanning = (globaltiledim.y / 256.f) * (255 - wall[wallnum].ypanning);

    globalwallvoffset = globalypanning * 32.f;
    if (cstat & 4)
        globalwallvoffset += globaltiledim.y * 32.f;

    globalwallu1 = globalxpanning * 32.f;
    globalwallu2 = globalwallu1 + (globalxrepeat * 8.f) * 32.f;
}

void RT_SetWallGlobals2(int wallnum, int cstat)
{
    int nextwall = wall[wallnum].nextwall;
    globalxrepeat = wall[wallnum].xrepeat / globaltilescale.x;
    globalxpanning = wall[nextwall].xpanning / globaltilescale.x;
    globalyrepeat = wall[wallnum].yrepeat / (4.f * globaltilescale.y);
    if (!(cstat & 256))
    {
        globalyrepeat = -globalyrepeat;
        globalypanning = (globaltiledim.y / 256.f) * wall[nextwall].ypanning;
    }
    else
        globalypanning = (globaltiledim.y / 256.f) * (255 - wall[nextwall].ypanning);

    globalwallvoffset = globalypanning * 32.f;
    if (cstat & 4)
        globalwallvoffset += globaltiledim.y * 32.f;

    globalwallu1 = globalxpanning * 32.f;
    globalwallu2 = globalwallu1 + (globalxrepeat * 8.f) * 32.f;
}

void RT_HandleWallCstat(int cstat)
{
    if (cstat & 8)
    {
        float t = globalwallu2;
        globalwallu2 = globalwallu1;
        globalwallu1 = t;
    }
    float v3 = min(globalwallv1, globalwallv2);
    if (v3 < -32760.f)
    {
        float adjust = ((int(fabs(v3) - 32760.f) + 4095) & ~4095);
        globalwallv1 += adjust;
        globalwallv2 += adjust;
        globalwallv3 += adjust;
        globalwallv4 += adjust;
    }
    v3 = max(globalwallv1, globalwallv2);
    if (v3 > 32760.f)
    {
        float adjust = ((int(v3 - 32760.f) + 4095) & ~4095);
        globalwallv1 -= adjust;
        globalwallv2 -= adjust;
        globalwallv3 -= adjust;
        globalwallv4 -= adjust;
    }

}

void RT_HandleWallCstatSlope(int cstat)
{
    if (cstat & 8)
    {
        float t = globalwallu2;
        globalwallu2 = globalwallu1;
        globalwallu1 = t;
    }
    float v3 = min({ globalwallv1, globalwallv2, globalwallv3, globalwallv4 });
    if (v3 < -32760.f)
    {
        float adjust = ((int(v3 - -32760.f) + 4095) & ~4095);
        globalwallv1 += adjust;
        globalwallv2 += adjust;
        globalwallv3 += adjust;
        globalwallv4 += adjust;
    }
    v3 = max({ globalwallv1, globalwallv2, globalwallv3, globalwallv4 });
    if (v3 > 32760.f)
    {
        float adjust = ((int(v3 - 32760.f) + 4095) & ~4095);
        globalwallv1 -= adjust;
        globalwallv2 -= adjust;
        globalwallv3 -= adjust;
        globalwallv4 -= adjust;
    }

}

struct maskdraw_t {
    int dist;
    uint16_t index;
};

maskdraw_t maskdrawlist[10240];
static int sortspritescnt = 0;

static int globalposx, globalposy, globalposz;

int RT_WallCalc_NoSlope(int sectnum, int wallnum)
{
    auto &w = wall[wallnum];
    int nextsectnum = w.nextsector;

    int ret = 0;
    rt_wallpolycount = 0;
    if (nextsectnum == -1)
    {
        int z1 = sector[sectnum].ceilingz;
        int z2 = sector[sectnum].floorz;
        int z1s = z1 >> 4;
        int z2s = z2 >> 4;
        if (z1s == z2s)
            return ret;

        int wx1 = w.x;
        int wy1 = w.y;
        int wx2 = wall[w.point2].x;
        int wy2 = wall[w.point2].y;
        int v10 = z1s;
        if (w.cstat & 4)
            v10 = z2s;

        RT_SetTileGlobals(w.picnum);
        RT_SetWallGlobals(wallnum, w.cstat);
        globalwallv1 = (v10 - z1s) * globalyrepeat + globalwallvoffset;
        globalwallv2 = (v10 - z2s) * globalyrepeat + globalwallvoffset;
        RT_HandleWallCstat(w.cstat);
        wallvtx[rt_wallpolycount*4+0].x = wx1 >> 1;
        wallvtx[rt_wallpolycount*4+0].y = wy1 >> 1;
        wallvtx[rt_wallpolycount*4+0].z = z1 >> 5;
        wallvtx[rt_wallpolycount*4+0].u = globalwallu1;
        wallvtx[rt_wallpolycount*4+0].v = globalwallv1;
        wallvtx[rt_wallpolycount*4+1].x = wx1 >> 1;
        wallvtx[rt_wallpolycount*4+1].y = wy1 >> 1;
        wallvtx[rt_wallpolycount*4+1].z = z2 >> 5;
        wallvtx[rt_wallpolycount*4+1].u = globalwallu1;
        wallvtx[rt_wallpolycount*4+1].v = globalwallv2;
        wallvtx[rt_wallpolycount*4+2].x = wx2 >> 1;
        wallvtx[rt_wallpolycount*4+2].y = wy2 >> 1;
        wallvtx[rt_wallpolycount*4+2].z = z2 >> 5;
        wallvtx[rt_wallpolycount*4+2].u = globalwallu2;
        wallvtx[rt_wallpolycount*4+2].v = globalwallv2;
        wallvtx[rt_wallpolycount*4+3].x = wx2 >> 1;
        wallvtx[rt_wallpolycount*4+3].y = wy2 >> 1;
        wallvtx[rt_wallpolycount*4+3].z = z1 >> 5;
        wallvtx[rt_wallpolycount*4+3].u = globalwallu2;
        wallvtx[rt_wallpolycount*4+3].v = globalwallv1;
        rt_wallpolycount++;
        ret |= 8;
        return ret;
    }
    if ((sector[sectnum].ceilingstat&1) == 0 || (sector[nextsectnum].ceilingstat&1) == 0)
    {
        int z1 = sector[sectnum].ceilingz;
        int z2 = sector[nextsectnum].ceilingz;
        int z1s = z1 >> 4;
        int z2s = z2 >> 4;
        if (z1s < z2s)
        {
            int wx1 = w.x;
            int wy1 = w.y;
            int wx2 = wall[w.point2].x;
            int wy2 = wall[w.point2].y;
            int v10;
            int cstat = w.cstat;
            if ((cstat & 4) == 0)
            {
                cstat |= 4;
                v10 = z2s;
            }
            else
            {
                cstat &= ~4;
                v10 = z1s;
            }
            RT_SetTileGlobals(w.picnum);
            RT_SetWallGlobals(wallnum, cstat);
            globalwallv1 = (v10 - z1s) * globalyrepeat + globalwallvoffset;
            globalwallv2 = (v10 - z2s) * globalyrepeat + globalwallvoffset;
            RT_HandleWallCstat(cstat);
            wallvtx[rt_wallpolycount*4+0].x = wx1 >> 1;
            wallvtx[rt_wallpolycount*4+0].y = wy1 >> 1;
            wallvtx[rt_wallpolycount*4+0].z = z1 >> 5;
            wallvtx[rt_wallpolycount*4+0].u = globalwallu1;
            wallvtx[rt_wallpolycount*4+0].v = globalwallv1;
            wallvtx[rt_wallpolycount*4+1].x = wx1 >> 1;
            wallvtx[rt_wallpolycount*4+1].y = wy1 >> 1;
            wallvtx[rt_wallpolycount*4+1].z = z2 >> 5;
            wallvtx[rt_wallpolycount*4+1].u = globalwallu1;
            wallvtx[rt_wallpolycount*4+1].v = globalwallv2;
            wallvtx[rt_wallpolycount*4+2].x = wx2 >> 1;
            wallvtx[rt_wallpolycount*4+2].y = wy2 >> 1;
            wallvtx[rt_wallpolycount*4+2].z = z2 >> 5;
            wallvtx[rt_wallpolycount*4+2].u = globalwallu2;
            wallvtx[rt_wallpolycount*4+2].v = globalwallv2;
            wallvtx[rt_wallpolycount*4+3].x = wx2 >> 1;
            wallvtx[rt_wallpolycount*4+3].y = wy2 >> 1;
            wallvtx[rt_wallpolycount*4+3].z = z1 >> 5;
            wallvtx[rt_wallpolycount*4+3].u = globalwallu2;
            wallvtx[rt_wallpolycount*4+3].v = globalwallv1;
            rt_wallpolycount++;
            ret |= 1;
        }
    }
    if ((sector[sectnum].floorstat&1) == 0 || (sector[nextsectnum].floorstat&1) == 0)
    {
        int z1 = sector[nextsectnum].floorz;
        int z2 = sector[sectnum].floorz;
        int z1s = z1 >> 4;
        int z2s = z2 >> 4;
        if (z1s < z2s)
        {
            int wx1 = w.x;
            int wy1 = w.y;
            int wx2 = wall[w.point2].x;
            int wy2 = wall[w.point2].y;
            int v10;
            int cstat = w.cstat;
            if ((cstat & 2) == 0)
            {
                if (cstat&4)
                    v10 = sector[sectnum].ceilingz>>4;
                else
                    v10 = z1s;
                cstat &= ~4;
                RT_SetTileGlobals(w.picnum);
                RT_SetWallGlobals(wallnum, cstat);
            }
            else
            {
                RT_SetTileGlobals(wall[w.nextwall].picnum);
                if (cstat & 4)
                    v10 = sector[sectnum].ceilingz>>4;
                else
                    v10 = z1s;
                cstat &= ~4;
                RT_SetWallGlobals2(wallnum, cstat);
            }
            globalwallv1 = (v10 - z1s) * globalyrepeat + globalwallvoffset;
            globalwallv2 = (v10 - z2s) * globalyrepeat + globalwallvoffset;
            RT_HandleWallCstat(cstat);
            wallvtx[rt_wallpolycount*4+0].x = wx1 >> 1;
            wallvtx[rt_wallpolycount*4+0].y = wy1 >> 1;
            wallvtx[rt_wallpolycount*4+0].z = z1 >> 5;
            wallvtx[rt_wallpolycount*4+0].u = globalwallu1;
            wallvtx[rt_wallpolycount*4+0].v = globalwallv1;
            wallvtx[rt_wallpolycount*4+1].x = wx1 >> 1;
            wallvtx[rt_wallpolycount*4+1].y = wy1 >> 1;
            wallvtx[rt_wallpolycount*4+1].z = z2 >> 5;
            wallvtx[rt_wallpolycount*4+1].u = globalwallu1;
            wallvtx[rt_wallpolycount*4+1].v = globalwallv2;
            wallvtx[rt_wallpolycount*4+2].x = wx2 >> 1;
            wallvtx[rt_wallpolycount*4+2].y = wy2 >> 1;
            wallvtx[rt_wallpolycount*4+2].z = z2 >> 5;
            wallvtx[rt_wallpolycount*4+2].u = globalwallu2;
            wallvtx[rt_wallpolycount*4+2].v = globalwallv2;
            wallvtx[rt_wallpolycount*4+3].x = wx2 >> 1;
            wallvtx[rt_wallpolycount*4+3].y = wy2 >> 1;
            wallvtx[rt_wallpolycount*4+3].z = z1 >> 5;
            wallvtx[rt_wallpolycount*4+3].u = globalwallu2;
            wallvtx[rt_wallpolycount*4+3].v = globalwallv1;
            rt_wallpolycount++;
            ret |= 2;
        }
    }
    if ((w.cstat & 32) == 0)
    {
        if (w.cstat & 16)
        {
            int wx = abs(globalposx - (w.x + wall[w.point2].x) / 2);
            int wy = abs(globalposy - (w.y + wall[w.point2].y) / 2);
            maskdrawlist[sortspritescnt].dist = (min(wx, wy) >> 3) + max(wx, wy) + (min(wx, wy) >> 2);
            maskdrawlist[sortspritescnt].index = wallnum | 32768;
            sortspritescnt++;
        }
    }
    else
    {
        int wx1 = w.x;
        int wy1 = w.y;
        int wx2 = wall[w.point2].x;
        int wy2 = wall[w.point2].y;
        int z1 = getceilzofslope(nextsectnum, wx1, wy1);
        int z2 = getflorzofslope(nextsectnum, wx1, wy1);
        int v9;
        if (w.cstat & 4)
            v9 = sector[sectnum].ceilingz;
        else
            v9 = sector[nextsectnum].floorz;
        RT_SetTileGlobals(w.overpicnum);
        RT_SetWallGlobals(wallnum, w.cstat & ~4);
        globalwallv1 = ((v9>>4) - (z1>>4)) * globalyrepeat + globalwallvoffset;
        globalwallv2 = ((v9>>4) - (z2>>4)) * globalyrepeat + globalwallvoffset;
        wallvtx[rt_wallpolycount*4+0].x = wx1 >> 1;
        wallvtx[rt_wallpolycount*4+0].y = wy1 >> 1;
        wallvtx[rt_wallpolycount*4+0].z = z1 >> 5;
        wallvtx[rt_wallpolycount*4+0].u = globalwallu1;
        wallvtx[rt_wallpolycount*4+0].v = globalwallv1;
        wallvtx[rt_wallpolycount*4+1].x = wx1 >> 1;
        wallvtx[rt_wallpolycount*4+1].y = wy1 >> 1;
        wallvtx[rt_wallpolycount*4+1].z = z2 >> 5;
        wallvtx[rt_wallpolycount*4+1].u = globalwallu1;
        wallvtx[rt_wallpolycount*4+1].v = globalwallv2;
        wallvtx[rt_wallpolycount*4+2].x = wx2 >> 1;
        wallvtx[rt_wallpolycount*4+2].y = wy2 >> 1;
        wallvtx[rt_wallpolycount*4+2].z = z2 >> 5;
        wallvtx[rt_wallpolycount*4+2].u = globalwallu2;
        wallvtx[rt_wallpolycount*4+2].v = globalwallv2;
        wallvtx[rt_wallpolycount*4+3].x = wx2 >> 1;
        wallvtx[rt_wallpolycount*4+3].y = wy2 >> 1;
        wallvtx[rt_wallpolycount*4+3].z = z1 >> 5;
        wallvtx[rt_wallpolycount*4+3].u = globalwallu2;
        wallvtx[rt_wallpolycount*4+3].v = globalwallv1;
        rt_wallpolycount++;
        ret |= 4;
    }
    return ret;
}

int RT_WallCalc_Slope(int sectnum, int wallnum)
{
    auto &w = wall[wallnum];
    int nextsectnum = w.nextsector;

    int ret = 0;
    rt_wallpolycount = 0;
    if (nextsectnum == -1)
    {
        int wx1 = w.x;
        int wy1 = w.y;
        int wx2 = wall[w.point2].x;
        int wy2 = wall[w.point2].y;
        int z1 = getceilzofslope(sectnum, wx1, wy1);
        int z2 = getflorzofslope(sectnum, wx1, wy1);
        int z3 = getflorzofslope(sectnum, wx2, wy2);
        int z4 = getceilzofslope(sectnum, wx2, wy2);
        if ((z1 >> 4) == (z2 >> 4) && (z3 >> 4) == (z4 >> 4))
            return ret;

        int v2;
        if (w.cstat & 4)
            v2 = sector[sectnum].floorz;
        else
            v2 = sector[sectnum].ceilingz;
        RT_SetTileGlobals(w.picnum);
        RT_SetWallGlobals(wallnum, w.cstat);

        globalwallv1 = ((v2>>4) - (z1>>4)) * globalyrepeat + globalwallvoffset;
        globalwallv2 = ((v2>>4) - (z2>>4)) * globalyrepeat + globalwallvoffset;
        globalwallv3 = ((v2>>4) - (z3>>4)) * globalyrepeat + globalwallvoffset;
        globalwallv4 = ((v2>>4) - (z4>>4)) * globalyrepeat + globalwallvoffset;
        RT_HandleWallCstatSlope(w.cstat);
        wallvtx[rt_wallpolycount*4+0].x = wx1 >> 1;
        wallvtx[rt_wallpolycount*4+0].y = wy1 >> 1;
        wallvtx[rt_wallpolycount*4+0].z = z1 >> 5;
        wallvtx[rt_wallpolycount*4+0].u = globalwallu1;
        wallvtx[rt_wallpolycount*4+0].v = globalwallv1;
        wallvtx[rt_wallpolycount*4+1].x = wx1 >> 1;
        wallvtx[rt_wallpolycount*4+1].y = wy1 >> 1;
        wallvtx[rt_wallpolycount*4+1].z = z2 >> 5;
        wallvtx[rt_wallpolycount*4+1].u = globalwallu1;
        wallvtx[rt_wallpolycount*4+1].v = globalwallv2;
        wallvtx[rt_wallpolycount*4+2].x = wx2 >> 1;
        wallvtx[rt_wallpolycount*4+2].y = wy2 >> 1;
        wallvtx[rt_wallpolycount*4+2].z = z3 >> 5;
        wallvtx[rt_wallpolycount*4+2].u = globalwallu2;
        wallvtx[rt_wallpolycount*4+2].v = globalwallv3;
        wallvtx[rt_wallpolycount*4+3].x = wx2 >> 1;
        wallvtx[rt_wallpolycount*4+3].y = wy2 >> 1;
        wallvtx[rt_wallpolycount*4+3].z = z4 >> 5;
        wallvtx[rt_wallpolycount*4+3].u = globalwallu2;
        wallvtx[rt_wallpolycount*4+3].v = globalwallv4;
        rt_wallpolycount++;
        ret |= 8;
        return ret;
    }
    if ((sector[sectnum].ceilingstat&1) == 0 || (sector[nextsectnum].ceilingstat&1) == 0)
    {
        int wx1 = w.x;
        int wy1 = w.y;
        int wx2 = wall[w.point2].x;
        int wy2 = wall[w.point2].y;
        int z1 = getceilzofslope(sectnum, wx1, wy1);
        int z2 = getceilzofslope(nextsectnum, wx1, wy1);
        int z3 = getceilzofslope(nextsectnum, wx2, wy2);
        int z4 = getceilzofslope(sectnum, wx2, wy2);
        if (((z2>>4) >= (z1>>4) || (z3>>4) >= (z4>>4))
          && (z2>>4) != (z1>>4) || (z3>>4) != (z4>>4))
        {
            int v14 = min(z1>>4, z2>>4);
            int vz4 = min(z3>>4, z4>>4);
            int vz;
            int cstat = w.cstat;
            if (w.cstat & 4)
            {
                cstat &= ~4;
                vz = sector[sectnum].ceilingz;
            }
            else
            {
                cstat |= 4;
                vz = sector[nextsectnum].ceilingz;
            }
            RT_SetTileGlobals(w.picnum);
            RT_SetWallGlobals(wallnum, cstat);
            globalwallv1 = ((vz>>4) - v14) * globalyrepeat + globalwallvoffset;
            globalwallv2 = ((vz>>4) - (z2>>4)) * globalyrepeat + globalwallvoffset;
            globalwallv3 = ((vz>>4) - (z3>>4)) * globalyrepeat + globalwallvoffset;
            globalwallv4 = ((vz>>4) - vz4) * globalyrepeat + globalwallvoffset;
            RT_HandleWallCstatSlope(cstat);
            wallvtx[rt_wallpolycount*4+0].x = wx1 >> 1;
            wallvtx[rt_wallpolycount*4+0].y = wy1 >> 1;
            wallvtx[rt_wallpolycount*4+0].z = v14 >> 1;
            wallvtx[rt_wallpolycount*4+0].u = globalwallu1;
            wallvtx[rt_wallpolycount*4+0].v = globalwallv1;
            wallvtx[rt_wallpolycount*4+1].x = wx1 >> 1;
            wallvtx[rt_wallpolycount*4+1].y = wy1 >> 1;
            wallvtx[rt_wallpolycount*4+1].z = z2 >> 5;
            wallvtx[rt_wallpolycount*4+1].u = globalwallu1;
            wallvtx[rt_wallpolycount*4+1].v = globalwallv2;
            wallvtx[rt_wallpolycount*4+2].x = wx2 >> 1;
            wallvtx[rt_wallpolycount*4+2].y = wy2 >> 1;
            wallvtx[rt_wallpolycount*4+2].z = z3 >> 5;
            wallvtx[rt_wallpolycount*4+2].u = globalwallu2;
            wallvtx[rt_wallpolycount*4+2].v = globalwallv3;
            wallvtx[rt_wallpolycount*4+3].x = wx2 >> 1;
            wallvtx[rt_wallpolycount*4+3].y = wy2 >> 1;
            wallvtx[rt_wallpolycount*4+3].z = vz4 >> 1;
            wallvtx[rt_wallpolycount*4+3].u = globalwallu2;
            wallvtx[rt_wallpolycount*4+3].v = globalwallv4;
            rt_wallpolycount++;
            ret |= 1;
        }
    }
    if ((sector[sectnum].floorstat&1) == 0 || (sector[nextsectnum].floorstat&1) == 0)
    {
        int wx1 = w.x;
        int wy1 = w.y;
        int wx2 = wall[w.point2].x;
        int wy2 = wall[w.point2].y;
        int z1 = getflorzofslope(nextsectnum, wx1, wy1);
        int z2 = getflorzofslope(sectnum, wx1, wy1);
        int z3 = getflorzofslope(sectnum, wx2, wy2);
        int z4 = getflorzofslope(nextsectnum, wx2, wy2);
        if (((z2>>4) >= (z1>>4) || (z3>>4) >= (z4>>4))
          && (z2>>4) != (z1>>4) || (z3>>4) != (z4>>4))
        {
            int v14 = max(z1>>4, z2>>4);
            int vz4 = max(z3>>4, z4>>4);
            int vz;
            int cstat;
            if (w.cstat & 2)
            {
                RT_SetTileGlobals(wall[w.nextwall].picnum);
                if (w.cstat & 4)
                {
                    cstat = w.cstat;
                    vz = sector[sectnum].ceilingz;
                    RT_SetWallGlobals2(wallnum, wall[w.nextwall].cstat & ~4);
                }
                else
                {
                    cstat = w.cstat;
                    vz = sector[nextsectnum].floorz;
                    RT_SetWallGlobals2(wallnum, wall[w.nextwall].cstat & ~4);
                }
            }
            else
            {
                if (w.cstat & 4)
                    vz = sector[sectnum].ceilingz;
                else
                    vz = sector[nextsectnum].floorz;
                cstat = w.cstat & ~4;
                RT_SetTileGlobals(w.picnum);
                RT_SetWallGlobals(wallnum, cstat);
            }
            globalwallv1 = ((vz>>4) - (z1>>4)) * globalyrepeat + globalwallvoffset;
            globalwallv2 = ((vz>>4) - v14) * globalyrepeat + globalwallvoffset;
            globalwallv3 = ((vz>>4) - vz4) * globalyrepeat + globalwallvoffset;
            globalwallv4 = ((vz>>4) - (z4>>4)) * globalyrepeat + globalwallvoffset;
            RT_HandleWallCstatSlope(cstat);
            wallvtx[rt_wallpolycount*4+0].x = wx1 >> 1;
            wallvtx[rt_wallpolycount*4+0].y = wy1 >> 1;
            wallvtx[rt_wallpolycount*4+0].z = z1 >> 5;
            wallvtx[rt_wallpolycount*4+0].u = globalwallu1;
            wallvtx[rt_wallpolycount*4+0].v = globalwallv1;
            wallvtx[rt_wallpolycount*4+1].x = wx1 >> 1;
            wallvtx[rt_wallpolycount*4+1].y = wy1 >> 1;
            wallvtx[rt_wallpolycount*4+1].z = v14 >> 1;
            wallvtx[rt_wallpolycount*4+1].u = globalwallu1;
            wallvtx[rt_wallpolycount*4+1].v = globalwallv2;
            wallvtx[rt_wallpolycount*4+2].x = wx2 >> 1;
            wallvtx[rt_wallpolycount*4+2].y = wy2 >> 1;
            wallvtx[rt_wallpolycount*4+2].z = vz4 >> 1;
            wallvtx[rt_wallpolycount*4+2].u = globalwallu2;
            wallvtx[rt_wallpolycount*4+2].v = globalwallv3;
            wallvtx[rt_wallpolycount*4+3].x = wx2 >> 1;
            wallvtx[rt_wallpolycount*4+3].y = wy2 >> 1;
            wallvtx[rt_wallpolycount*4+3].z = z4 >> 5;
            wallvtx[rt_wallpolycount*4+3].u = globalwallu2;
            wallvtx[rt_wallpolycount*4+3].v = globalwallv4;
            rt_wallpolycount++;
            ret |= 2;
        }
    }
    if ((w.cstat & 32) == 0)
    {
        if (w.cstat & 16)
        {
            int wx = abs(globalposx - (w.x + wall[w.point2].x) / 2);
            int wy = abs(globalposy - (w.y + wall[w.point2].y) / 2);
            maskdrawlist[sortspritescnt].dist = (min(wx, wy) >> 3) + max(wx, wy) + (min(wx, wy) >> 2);
            maskdrawlist[sortspritescnt].index = wallnum | 32768;
            sortspritescnt++;
        }
    }
    else
    {
        int wx1 = w.x;
        int wy1 = w.y;
        int wx2 = wall[w.point2].x;
        int wy2 = wall[w.point2].y;
        int z1 = getceilzofslope(nextsectnum, wx1, wy1);
        int z2 = getflorzofslope(nextsectnum, wx1, wy1);
        int z3 = getflorzofslope(nextsectnum, wx2, wy2);
        int z4 = getceilzofslope(nextsectnum, wx2, wy2);
        int v2;
        if (w.cstat & 4)
            v2 = sector[sectnum].ceilingz;
        else
            v2 = sector[nextsectnum].floorz;
        int cstat = w.cstat & ~4;
        RT_SetTileGlobals(w.overpicnum);
        RT_SetWallGlobals(wallnum, cstat);

        globalwallv1 = ((v2>>4) - (z1>>4)) * globalyrepeat + globalwallvoffset;
        globalwallv2 = ((v2>>4) - (z2>>4)) * globalyrepeat + globalwallvoffset;
        globalwallv3 = ((v2>>4) - (z3>>4)) * globalyrepeat + globalwallvoffset;
        globalwallv4 = ((v2>>4) - (z4>>4)) * globalyrepeat + globalwallvoffset;
        RT_HandleWallCstatSlope(cstat);
        wallvtx[rt_wallpolycount*4+0].x = wx1 >> 1;
        wallvtx[rt_wallpolycount*4+0].y = wy1 >> 1;
        wallvtx[rt_wallpolycount*4+0].z = z1 >> 5;
        wallvtx[rt_wallpolycount*4+0].u = globalwallu1;
        wallvtx[rt_wallpolycount*4+0].v = globalwallv1;
        wallvtx[rt_wallpolycount*4+1].x = wx1 >> 1;
        wallvtx[rt_wallpolycount*4+1].y = wy1 >> 1;
        wallvtx[rt_wallpolycount*4+1].z = z2 >> 5;
        wallvtx[rt_wallpolycount*4+1].u = globalwallu1;
        wallvtx[rt_wallpolycount*4+1].v = globalwallv2;
        wallvtx[rt_wallpolycount*4+2].x = wx2 >> 1;
        wallvtx[rt_wallpolycount*4+2].y = wy2 >> 1;
        wallvtx[rt_wallpolycount*4+2].z = z3 >> 5;
        wallvtx[rt_wallpolycount*4+2].u = globalwallu2;
        wallvtx[rt_wallpolycount*4+2].v = globalwallv3;
        wallvtx[rt_wallpolycount*4+3].x = wx2 >> 1;
        wallvtx[rt_wallpolycount*4+3].y = wy2 >> 1;
        wallvtx[rt_wallpolycount*4+3].z = z4 >> 5;
        wallvtx[rt_wallpolycount*4+3].u = globalwallu2;
        wallvtx[rt_wallpolycount*4+3].v = globalwallv4;
        rt_wallpolycount++;
        ret |= 4;
    }
    return ret;
}

int RT_WallCalc(int sectnum, int wallnum)
{
    int cstat = sector[sectnum].floorstat | sector[sectnum].ceilingstat;
    int nextsectnum = wall[wallnum].nextsector;
    if (nextsectnum != -1)
        cstat |= sector[nextsectnum].floorstat | sector[nextsectnum].ceilingstat;
    if (cstat & 2)
        return RT_WallCalc_Slope(sectnum, wallnum);
    return RT_WallCalc_NoSlope(sectnum, wallnum);
}

static tspritetype rt_tsprite, *rt_tspriteptr;

static int rt_tspritepicnum;
static int rt_fxcolor;
static int rt_curfxcolor, rt_boss2, rt_lastpicnum, rt_spritezbufferhack;
static float viewangsin, viewangcos;
static vec2_t rt_spritedim;
static int rt_spritedimtotal;

void RT_SetupDrawMask(void)
{
    rt_lastpicnum = 0;
    rt_boss2 = 0;
    rt_spritezbufferhack = 0;
    rt_fxtile = 0;
    rt_curfxcolor = 0;
    viewangcos = cosf(rt_globalang * BANG2RAD);
    viewangsin = sinf(rt_globalang * BANG2RAD);
    RT_SetTexComb(0);

    glEnable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
}

static vec3_t colortable[14][2] = {
    { 0, 0, 0, 0, 0, 0 },
    { 255, 255, 0, 255, 0, 0 },
    { 255, 255, 255, 255, 255, 0 },
    { 128, 128, 255, 64, 64, 128 },
    { 15, 255, 255, 115, 0, 170 },
    { 128, 128, 128, 64, 64, 64 },
    { 0, 0, 255, 64, 64, 128 },
    { 255, 192, 192, 64, 64, 64 },
    { 0, 0, 0, 0, 0, 0 },
    { 255, 0, 0, 255, 0, 0 },
    { 0, 255, 0, 0, 255, 255 },
    { 255, 0, 0, 255, 127, 0 },
    { 255, 255, 255, 64, 64, 64 },
    { 255, 255, 255, 0, 127, 127 }
};

static int rt_tspritesect;
static uint8_t rt_globalalpha;

void RT_DrawSpriteFace(float x, float y, float z, int pn)
{
    if (rt_tspriteptr->xrepeat == 0 || rt_tspriteptr->yrepeat == 0)
        return;

    float sw = rt_projmatrix[15] + rt_projmatrix[3] * x + rt_projmatrix[7] * y + rt_projmatrix[11] * z;
    if (sw == 0.f)
        return;
    float sx = (rt_projmatrix[12] + rt_projmatrix[0] * x + rt_projmatrix[4] * y + rt_projmatrix[8] * z) / sw;
    float sy = (rt_projmatrix[13] + rt_projmatrix[1] * x + rt_projmatrix[5] * y + rt_projmatrix[9] * z) / sw;
    float sz = (rt_projmatrix[14] + rt_projmatrix[2] * x + rt_projmatrix[6] * y + rt_projmatrix[10] * z) / sw;
    if (sx < -2 || sx > 2.f || sy < -2.f || sy > 2.f || sz < 0.f || sz > 1.f)
        return;
    
    rt_globaldepth = sz;

    float tt = 1.f - sz;

    glColor4f(1.f, 1.f, 1.f, rt_globalalpha * (1.f / 255.f));
    RT_DisplayTileWorld(sx * x_vs + x_vt, -sy * y_vs + y_vt, rt_tspriteptr->xrepeat * tt * 4.f, rt_tspriteptr->yrepeat * tt * 4.f,
        rt_tspriteptr->picnum, rt_tspriteptr->cstat);
    //RT_DisplayTileWorld(sx * x_vs + x_vt, -sy * y_vs + y_vt, 4.f, 4.f,
    //    rt_tspriteptr->picnum, rt_tspriteptr->cstat);
}

void RT_DrawSpriteFlat(int spritenum, int sectnum, int distance)
{
    rt_lastpicnum = 0;
    globalpal = rt_tspriteptr->pal;
    // TODO: shade
    int xoff = int8_t((rt_tileinfo[rt_tspritepicnum].picanm>>8)&255);
    int yoff = int8_t((rt_tileinfo[rt_tspritepicnum].picanm>>16)&255);
    int v20 = (rt_tileinfo[rt_tspritepicnum].sizx * rt_tspriteptr->xrepeat) / 16;
    int v11 = (rt_tileinfo[rt_tspritepicnum].sizy * rt_tspriteptr->yrepeat) / 8;
    int v6 = ((xoff + rt_tspriteptr->xoffset) * rt_tspriteptr->xrepeat) / 8;
    if (rt_tspriteptr->cstat&128)
        rt_tspriteptr->z += (yoff >> 1) * 32;
    if (rt_tspriteptr->cstat&8) {
    }

    rt_tspriteptr->z += (yoff + rt_tspriteptr->yoffset) * rt_tspriteptr->yrepeat * -4;

    int sz = (rt_tspriteptr->z >> 5) - v11;
    float v1, v2;
    if (rt_tspriteptr->cstat&4)
    {
        v1 = 0.f;
        v2 = 1.f;
    }
    else
    {
        v1 = 1.f;
        v2 = 0.f;
    }

    rt_globalalpha = 255;
    if (rt_tspriteptr->cstat&2)
        rt_globalalpha = 192;
    if (rt_tspriteptr->cstat&512)
        rt_globalalpha = 128;

    int16_t v40 = (rt_tspriteptr->x / 2.f);
    int16_t v48 = (rt_tspriteptr->y / 2.f);
    int16_t v46 = v48;
    int16_t v3e = v40;

    if((rt_tspriteptr->cstat&48)==0)
    {
        // TODO: display face sprite
        if (1)
        {
            RT_DrawSpriteFace(v40/2, v48/2, (sz + (v11>>1))/2, rt_tspritepicnum);
        }
        else
        {
            float ds = viewangsin * v20;
            float dc = viewangcos * v20;
            v40 += ds;
            v3e -= ds;
            v46 += dc;
            v48 -= dc;
        }
    }
    else if((rt_tspriteptr->cstat&48)==16)
    {
        float ang = rt_tspriteptr->ang / (1024.f/180.f);
        if (rt_spritezbufferhack)
        {
            int zoff = distance / 120;
            if (zoff < 4)
                zoff = 4;
            float zs = sin((ang+90.f)/(180.f/fPI));
            float zc = cos((ang+90.f)/(180.f/fPI));
            v3e += zs * zoff;
            v46 -= zc * zoff;
        }
        int o1 = v3e, o2 = v46;
        float fs = sin((ang-180.f)/(180.f/fPI));
        float fc = cos((ang-180.f)/(180.f/fPI));
        v40 = o1 + fs * v20 + fs * v6;
        v3e = o1 - fs * v20 + fs * v6;
        v46 = o2 + fc * v20;
        v48 = o2 - fc * v20;
    }

    int sz2;
    if (rt_tspriteptr->cstat & 8)
    {
        sz2 = sz;
        sz += v11;
    }
    else
    {
        sz2 = sz + v11;
    }
    
    int method = DAMETH_CLAMPED | DAMETH_N64 | (rt_fxtile ? DAMETH_N64_INTENSIVITY : 0);
    pthtyp *pth = texcache_fetch(rt_tspriteptr->picnum, 0, 0, method);

    if (!pth)
        return;

    glBindTexture(GL_TEXTURE_2D, pth->glpic);
    glBegin(GL_QUADS);
    glTexCoord2f(v1, 0.f); glColor4f(1.f, 1.f, 1.f, rt_globalalpha*(1.f/255.f)); glVertex3f(v3e, v46, sz);
    glTexCoord2f(v2, 0.f); glColor4f(1.f, 1.f, 1.f, rt_globalalpha*(1.f/255.f)); glVertex3f(v40, v48, sz);
    glTexCoord2f(v2, 1.f); glColor4f(1.f, 1.f, 1.f, rt_globalalpha*(1.f/255.f)); glVertex3f(v40, v48, sz2);
    glTexCoord2f(v1, 1.f); glColor4f(1.f, 1.f, 1.f, rt_globalalpha*(1.f/255.f)); glVertex3f(v3e, v46, sz2);
    glEnd();
}

void RT_DrawSprite(int spritenum, int sectnum, int distance)
{
    int pn;
    if (sprite[spritenum].cstat & 32768)
        return;
    if (sprite[spritenum].picnum < 11)
        return;
    if (sprite[spritenum].xrepeat == 0)
        return;

    rt_tspriteptr = &rt_tsprite;

    Bmemcpy(&rt_tsprite, &sprite[spritenum], sizeof(spritetype));

    // TODO: animatesprites
    // HACK:
    if ((rt_tsprite.picnum == 1405))
        return;
    if (rt_tspriteptr->xrepeat == 0)
        return;

    rt_tspritepicnum = rt_tilemap[rt_tspriteptr->picnum];
    if (rt_tspritepicnum == 1)
        return;
    if (rt_tileinfo[rt_tspritepicnum].picanm & 192)
        rt_tspritepicnum += animateoffs(rt_tspriteptr->picnum, 0);

    pn = sprite[spritenum].picnum;

    // TODO: BOSS2 code

    rt_fxcolor = 0;
    if (pn == 1360 || pn == 1671)
    {
        rt_tspriteptr->cstat |= 512;
    }
    switch (pn)
    {
    case 1261:
        rt_fxcolor = 4;
        break;
    case 0x659:
        rt_fxcolor = 2;
        break;
    case 0x65e:
        rt_fxcolor = 7;
        break;
    case 0x66e:
        rt_fxcolor = 10;
        break;
    case 0x678:
        rt_fxcolor = 10;
        break;
    case 0x687:
        rt_fxcolor = 4;
        break;
    case 0x762:
        rt_fxcolor = 1;
        break;
    case 0x8de:
        rt_fxcolor = 1;
        break;
    case 0x8df:
        rt_fxcolor = 1;
        break;
    case 0x906:
        rt_fxcolor = 1;
        break;
    case 0x907:
        rt_fxcolor = 1;
        break;
    case 0x919:
        rt_fxcolor = 5;
        break;
    case 0x990:
        rt_fxcolor = 11;
        break;
    case 0xa07:
        rt_fxcolor = 9;
        break;
    case 0xa23:
        rt_fxcolor = 2;
        break;
    case 0xa24:
        rt_fxcolor = 2;
        break;
    case 0xa25:
        rt_fxcolor = 2;
        break;
    case 0xa26:
        rt_fxcolor = 2;
        break;
    case 0xa27:
        rt_fxcolor = 2;
        break;
    case 0xe8c:
        rt_fxcolor = 2;
        break;
    case 0xf01:
        rt_fxcolor = 4;
        break;
    case 0xf05:
        rt_fxcolor = 2;
        break;
    case 0xf71:
        rt_fxcolor = 13;
        break;
    }
    if (rt_fxcolor == 0)
    {
        if (rt_fxtile)
        {
            RT_SetTexComb(0);
            glEnable(GL_ALPHA_TEST);
            rt_fxtile = 0;
            glColor4f(1.f, 1.f, 1.f, 1.f);
        }
    }
    else
    {
        if (!rt_fxtile)
        {
            glDisable(GL_ALPHA_TEST);
            RT_SetTexComb(1);
            rt_fxtile = 1;
        }
        RT_SetColor1(colortable[rt_fxcolor][1].x, colortable[rt_fxcolor][1].y, colortable[rt_fxcolor][1].z, 255);
        RT_SetColor2(colortable[rt_fxcolor][0].x, colortable[rt_fxcolor][0].y, colortable[rt_fxcolor][0].z, 255);
    }
    rt_spritezbufferhack = (rt_tspriteptr->cstat & 16384) != 0;
    rt_spritedim = { rt_tileinfo[rt_tspritepicnum].dimx, rt_tileinfo[rt_tspritepicnum].dimy };
    rt_spritedimtotal = rt_spritedim.x * rt_spritedim.y;
    rt_tspritesect = sectnum;
    if ((rt_tspriteptr->cstat & 48) == 16 && (rt_tspriteptr->cstat&64))
    {
        int ang = getangle(rt_tspriteptr->x - globalposx, rt_tspriteptr->y - globalposy) - rt_tspriteptr->ang;
        if (ang > 1024)
            ang -= 2048;
        if (ang < -1024)
            ang += 2048;
        if (klabs(ang) < 512)
            return;
    }
    if ((rt_tspriteptr->cstat&48) == 32)
    {
        // TODO: floor aligned sprite
        return;
    }
    RT_DrawSpriteFlat(spritenum, rt_tspritesect, distance);
}

static int globalpal, globalshade;

void RT_DrawWall(int wallnum)
{
    rt_wallcalcres = RT_WallCalc(rt_wall[wallnum].sectnum, wallnum);
    globalpal = wall[wallnum].pal;
    globalshade = wall[wallnum].shade;
    rt_haswhitewall = (rt_wallcalcres & 8) != 0;
    rt_hastopwall = (rt_wallcalcres & 1) != 0;
    rt_hasbottomwall = (rt_wallcalcres & 2) != 0;
    rt_hasoneway = (rt_wallcalcres & 4) != 0;
    int j = 0;
    RT_SetTexture(wall[wallnum].picnum);
    glBegin(GL_QUADS);
    for (int i = 0; i < (rt_haswhitewall + rt_hastopwall) * 4; i++)
    {
        auto vtx = wallvtx[j++];
        glTexCoord2f(vtx.u * rt_uvscale.x, vtx.v * rt_uvscale.y); glVertex3f(vtx.x, vtx.y, vtx.z);
    }
    glEnd();
    if (wall[wallnum].cstat & 2)
        RT_SetTexture(wall[wall[wallnum].nextwall].picnum);
    glBegin(GL_QUADS);
    for (int i = 0; i < rt_hasbottomwall * 4; i++)
    {
        auto vtx = wallvtx[j++];
        glTexCoord2f(vtx.u * rt_uvscale.x, vtx.v * rt_uvscale.y); glVertex3f(vtx.x, vtx.y, vtx.z);
    }
    glEnd();
    if (rt_hasoneway)
    {
        RT_SetTexture(wall[wallnum].overpicnum);
        glBegin(GL_QUADS);
        for (int i = 0; i < rt_hasoneway * 4; i++)
        {
            auto vtx = wallvtx[j++];
            glTexCoord2f(vtx.u * rt_uvscale.x, vtx.v * rt_uvscale.y); glVertex3f(vtx.x, vtx.y, vtx.z);
        }
        glEnd();
    }
}

void RT_DrawRooms(int x, int y, int z, fix16_t ang, fix16_t horiz, int16_t sectnum)
{
    RT_DisablePolymost();
#if 0
    // Test code
    int32_t method = 0;
    pthtyp* testpth = texcache_fetch(26, 0, 0, method);
    glBindTexture(GL_TEXTURE_2D, testpth->glpic);
    glViewport(0, 0, xdim, ydim);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 320.f, 240.f, 0, -1.f, 1.f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glColor3f(1, 1, 1);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(96.f, 0);
    glTexCoord2f(1, 1); glVertex2f(96.f, 40.f);
    glTexCoord2f(0, 1); glVertex2f(0, 40.f);
    glEnd();
#endif
    
    glDisable(GL_ALPHA_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    globalposx = x;
    globalposy = y;
    globalposz = z;

    rt_globalpicnum = -1;
    rt_globalposx = x * 0.5f;
    rt_globalposy = y * 0.5f;
    rt_globalposz = z * (1.f/32.f);
    rt_globalhoriz = fix16_to_float(horiz);
    rt_globalang = fix16_to_float(ang);
    RT_SetupMatrix();
    RT_DisplaySky();
    rt_fxcolor = 0;
    sortspritescnt = 0;

    glColor4f(1.f, 1.f, 1.f, 1.f);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    for (int i = 0; i < numsectors; i++)
    {
        RT_DrawCeiling(i);
        RT_DrawFloor(i);
    }

    for (int i = 0; i < numwalls; i++)
    {
        //if (rt_wall[i].sectnum != sectnum)
        //    continue;
        RT_DrawWall(i);
    }

    RT_SetupDrawMask();

    for (int i = 0; i < MAXSPRITES; i++)
    {
        if (sprite[i].statnum != MAXSTATUS)
        {
            int wx = abs(globalposx - sprite[i].x);
            int wy = abs(globalposy - sprite[i].y);
            RT_DrawSprite(i, sprite[i].sectnum, (min(wx, wy) >> 3) + max(wx, wy) + (min(wx, wy) >> 2));
        }
    }

    RT_EnablePolymost();
}

