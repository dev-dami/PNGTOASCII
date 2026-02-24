#include <limits.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>
#include <png.h>

#define d_ow 80
#define d_oh 40
#define mx_dim 16384

static const char d_trp[] =
    "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft()1{}[]?+~<>i!lI;:,\"^`'. ";

typedef struct {
    int w;
    int h;
    unsigned char *px;
} img_t;

typedef struct {
    struct jpeg_error_mgr jb;
    jmp_buf jmp;
} jpg_err_t;

static void img_clr(img_t *im) {
    free(im->px);
    im->px = NULL;
    im->w = 0;
    im->h = 0;
}

static int smul(size_t a, size_t b, size_t *o) {
    if (a != 0 && b > SIZE_MAX / a) return 0;
    *o = a * b;
    return 1;
}

static int img_alc(img_t *im, int w, int h) {
    size_t n = 0;
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "error: invalid image dimensions\n");
        return 0;
    }
    if (w > mx_dim || h > mx_dim) {
        fprintf(stderr, "error: image too large (max %dx%d)\n", mx_dim, mx_dim);
        return 0;
    }
    if (!smul((size_t)w, (size_t)h, &n)) {
        fprintf(stderr, "error: image size overflow\n");
        return 0;
    }

    im->px = (unsigned char *)malloc(n);
    if (!im->px) {
        fprintf(stderr, "error: not enough memory for image (%dx%d)\n", w, h);
        return 0;
    }

    im->w = w;
    im->h = h;
    return 1;
}

static unsigned char a2w(unsigned char c, unsigned char a) {
    return (unsigned char)(((unsigned int)c * a + 255U * (255U - a)) / 255U);
}

static int rd_png(const char *fn, img_t *im) {
    FILE *fp = fopen(fn, "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot open file %s\n", fn);
        return 0;
    }

    unsigned char sg[8];
    if (fread(sg, 1, sizeof(sg), fp) != sizeof(sg) || png_sig_cmp(sg, 0, sizeof(sg)) != 0) {
        fprintf(stderr, "error: invalid png signature: %s\n", fn);
        fclose(fp);
        return 0;
    }

    png_structp ps = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!ps) {
        fprintf(stderr, "error: cannot initialize png reader\n");
        fclose(fp);
        return 0;
    }

    png_infop pi = png_create_info_struct(ps);
    if (!pi) {
        fprintf(stderr, "error: cannot create png info\n");
        png_destroy_read_struct(&ps, NULL, NULL);
        fclose(fp);
        return 0;
    }

    if (setjmp(png_jmpbuf(ps))) {
        fprintf(stderr, "error: png decode failed\n");
        png_destroy_read_struct(&ps, &pi, NULL);
        img_clr(im);
        fclose(fp);
        return 0;
    }

    png_init_io(ps, fp);
    png_set_sig_bytes(ps, sizeof(sg));
    png_read_info(ps, pi);

    png_uint_32 w = 0;
    png_uint_32 h = 0;
    int bd = 0;
    int ct = 0;

    png_get_IHDR(ps, pi, &w, &h, &bd, &ct, NULL, NULL, NULL);
    if (w == 0 || h == 0 || w > mx_dim || h > mx_dim) {
        fprintf(stderr, "error: png dimensions out of range\n");
        png_destroy_read_struct(&ps, &pi, NULL);
        fclose(fp);
        return 0;
    }

    if (bd == 16) png_set_strip_16(ps);
    if (ct == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(ps);
    if (ct == PNG_COLOR_TYPE_GRAY && bd < 8) png_set_expand_gray_1_2_4_to_8(ps);
    if (png_get_valid(ps, pi, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(ps);
    if (ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(ps);
    if ((ct & PNG_COLOR_MASK_ALPHA) == 0 && !png_get_valid(ps, pi, PNG_INFO_tRNS)) {
        png_set_filler(ps, 0xFF, PNG_FILLER_AFTER);
    }

    png_read_update_info(ps, pi);
    if (png_get_channels(ps, pi) != 4) {
        fprintf(stderr, "error: png channel transform failed\n");
        png_destroy_read_struct(&ps, &pi, NULL);
        fclose(fp);
        return 0;
    }

    png_size_t rb = png_get_rowbytes(ps, pi);
    size_t buf_sz = 0;
    size_t rows_sz = 0;
    if (!smul((size_t)rb, (size_t)h, &buf_sz) || !smul(sizeof(png_bytep), (size_t)h, &rows_sz)) {
        fprintf(stderr, "error: png buffer size overflow\n");
        png_destroy_read_struct(&ps, &pi, NULL);
        fclose(fp);
        return 0;
    }

    unsigned char *buf = (unsigned char *)malloc(buf_sz);
    png_bytep *rows = (png_bytep *)malloc(rows_sz);
    if (!buf || !rows) {
        fprintf(stderr, "error: not enough memory for png decode\n");
        free(buf);
        free(rows);
        png_destroy_read_struct(&ps, &pi, NULL);
        fclose(fp);
        return 0;
    }

    for (png_uint_32 y = 0; y < h; y++) rows[y] = buf + (size_t)y * (size_t)rb;

    png_read_image(ps, rows);
    png_read_end(ps, NULL);

    if (!img_alc(im, (int)w, (int)h)) {
        free(rows);
        free(buf);
        png_destroy_read_struct(&ps, &pi, NULL);
        fclose(fp);
        return 0;
    }

    for (png_uint_32 y = 0; y < h; y++) {
        unsigned char *r = rows[y];
        for (png_uint_32 x = 0; x < w; x++) {
            unsigned char c0 = r[x * 4 + 0];
            unsigned char c1 = r[x * 4 + 1];
            unsigned char c2 = r[x * 4 + 2];
            unsigned char ca = r[x * 4 + 3];
            c0 = a2w(c0, ca);
            c1 = a2w(c1, ca);
            c2 = a2w(c2, ca);
            im->px[(size_t)y * (size_t)w + (size_t)x] = (unsigned char)((299U * c0 + 587U * c1 + 114U * c2) / 1000U);
        }
    }

    free(rows);
    free(buf);
    png_destroy_read_struct(&ps, &pi, NULL);
    fclose(fp);
    return 1;
}

static void jpg_die(j_common_ptr ci) {
    jpg_err_t *je = (jpg_err_t *)ci->err;
    longjmp(je->jmp, 1);
}

static int rd_jpg(const char *fn, img_t *im) {
    FILE *fp = fopen(fn, "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot open file %s\n", fn);
        return 0;
    }

    struct jpeg_decompress_struct jd;
    jpg_err_t je;
    jd.err = jpeg_std_error(&je.jb);
    je.jb.error_exit = jpg_die;

    if (setjmp(je.jmp)) {
        jpeg_destroy_decompress(&jd);
        img_clr(im);
        fclose(fp);
        fprintf(stderr, "error: jpeg decode failed\n");
        return 0;
    }

    jpeg_create_decompress(&jd);
    jpeg_stdio_src(&jd, fp);
    jpeg_read_header(&jd, TRUE);
    jpeg_start_decompress(&jd);

    if (jd.output_width == 0 || jd.output_height == 0 || jd.output_width > mx_dim || jd.output_height > mx_dim) {
        jpeg_finish_decompress(&jd);
        jpeg_destroy_decompress(&jd);
        fclose(fp);
        fprintf(stderr, "error: jpeg dimensions out of range\n");
        return 0;
    }

    if (!img_alc(im, (int)jd.output_width, (int)jd.output_height)) {
        jpeg_finish_decompress(&jd);
        jpeg_destroy_decompress(&jd);
        fclose(fp);
        return 0;
    }

    int ch = jd.output_components;
    size_t row_bytes = 0;
    if (ch <= 0 || !smul((size_t)jd.output_width, (size_t)ch, &row_bytes) || row_bytes > (size_t)UINT_MAX) {
        jpeg_finish_decompress(&jd);
        jpeg_destroy_decompress(&jd);
        img_clr(im);
        fclose(fp);
        fprintf(stderr, "error: jpeg row size overflow\n");
        return 0;
    }

    JSAMPARRAY row = (*jd.mem->alloc_sarray)((j_common_ptr)&jd, JPOOL_IMAGE, (JDIMENSION)row_bytes, 1);

    size_t y = 0;
    while (jd.output_scanline < jd.output_height) {
        jpeg_read_scanlines(&jd, row, 1);
        unsigned char *src = row[0];
        for (size_t x = 0; x < jd.output_width; x++) {
            unsigned int lum;
            if (ch >= 3) {
                unsigned char r = src[x * (size_t)ch + 0];
                unsigned char g = src[x * (size_t)ch + 1];
                unsigned char b = src[x * (size_t)ch + 2];
                lum = (299U * r + 587U * g + 114U * b) / 1000U;
            } else {
                lum = src[x * (size_t)ch + 0];
            }
            im->px[y * jd.output_width + x] = (unsigned char)lum;
        }
        y++;
    }

    jpeg_finish_decompress(&jd);
    jpeg_destroy_decompress(&jd);
    fclose(fp);
    return 1;
}

static int rd_img(const char *fn, img_t *im) {
    FILE *fp = fopen(fn, "rb");
    if (!fp) {
        fprintf(stderr, "error: cannot open file %s\n", fn);
        return 0;
    }

    unsigned char hd[8] = {0};
    size_t n = fread(hd, 1, sizeof(hd), fp);
    fclose(fp);

    if (n >= 8 && png_sig_cmp(hd, 0, 8) == 0) return rd_png(fn, im);
    if (n >= 3 && hd[0] == 0xFF && hd[1] == 0xD8 && hd[2] == 0xFF) return rd_jpg(fn, im);

    fprintf(stderr, "error: unsupported format, use png/jpg/jpeg\n");
    return 0;
}

static void mk_tlut(const img_t *im, unsigned char tlut[256]) {
    uint32_t hst[256] = {0};
    size_t n = (size_t)im->w * (size_t)im->h;
    for (size_t i = 0; i < n; i++) hst[im->px[i]]++;

    uint64_t lo_t = (uint64_t)n / 100U;
    uint64_t hi_t = ((uint64_t)n * 99U) / 100U;
    uint64_t acc = 0;
    int lo = 0;
    int hi = 255;

    for (int i = 0; i < 256; i++) {
        acc += hst[i];
        if (acc >= lo_t) {
            lo = i;
            break;
        }
    }

    acc = 0;
    for (int i = 0; i < 256; i++) {
        acc += hst[i];
        if (acc >= hi_t) {
            hi = i;
            break;
        }
    }

    if (hi <= lo) {
        lo = 0;
        hi = 255;
    }

    int rg = hi - lo;
    for (int i = 0; i < 256; i++) {
        int v;
        if (i <= lo) v = 0;
        else if (i >= hi) v = 255;
        else v = ((i - lo) * 255) / rg;

        int b = (v * 180 + ((v * v) / 255) * 75) / 255;
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        tlut[i] = (unsigned char)b;
    }
}

static int mk_sat2(const img_t *im, uint64_t **sa_o, uint64_t **sq_o, size_t *st_o) {
    size_t st = (size_t)im->w + 1U;
    size_t rs = (size_t)im->h + 1U;
    size_t n = 0;
    uint64_t *sa = NULL;
    uint64_t *sq = NULL;

    if (!smul(st, rs, &n) || n > SIZE_MAX / sizeof(uint64_t)) return 0;

    sa = (uint64_t *)calloc(n, sizeof(uint64_t));
    sq = (uint64_t *)calloc(n, sizeof(uint64_t));
    if (!sa || !sq) {
        free(sa);
        free(sq);
        return 0;
    }

    for (int y = 1; y <= im->h; y++) {
        uint64_t rsum = 0;
        uint64_t qsum = 0;
        const unsigned char *src = im->px + (size_t)(y - 1) * (size_t)im->w;
        uint64_t *cur_s = sa + (size_t)y * st;
        uint64_t *prv_s = sa + (size_t)(y - 1) * st;
        uint64_t *cur_q = sq + (size_t)y * st;
        uint64_t *prv_q = sq + (size_t)(y - 1) * st;

        for (int x = 1; x <= im->w; x++) {
            uint64_t v = src[x - 1];
            rsum += v;
            qsum += v * v;
            cur_s[x] = prv_s[x] + rsum;
            cur_q[x] = prv_q[x] + qsum;
        }
    }

    *sa_o = sa;
    *sq_o = sq;
    *st_o = st;
    return 1;
}

static uint64_t sat_blk(const uint64_t *sa, size_t st, int x0, int y0, int x1, int y1) {
    size_t sx0 = (size_t)x0;
    size_t sx1 = (size_t)x1;
    size_t sy0 = (size_t)y0;
    size_t sy1 = (size_t)y1;
    return sa[sy1 * st + sx1] - sa[sy0 * st + sx1] - sa[sy1 * st + sx0] + sa[sy0 * st + sx0];
}

static unsigned char pxc(const img_t *im, int x, int y) {
    if (x < 0) x = 0;
    if (x >= im->w) x = im->w - 1;
    if (y < 0) y = 0;
    if (y >= im->h) y = im->h - 1;
    return im->px[(size_t)y * (size_t)im->w + (size_t)x];
}

static char ed_ch(int gx, int gy) {
    int ax = gx < 0 ? -gx : gx;
    int ay = gy < 0 ? -gy : gy;
    if (ax > ay * 2) return '|';
    if (ay > ax * 2) return '-';
    return (gx ^ gy) < 0 ? '/' : '\\';
}

static unsigned char qv_to_u8(int qi, int qn) {
    if (qi < 0) qi = 0;
    if (qi >= qn) qi = qn - 1;
    return (unsigned char)((qi * 255) / (qn - 1));
}

static void wr_asc(const img_t *im, int ow, int oh, FILE *out) {
    float sx = (float)im->w / (float)ow;
    float sy = (float)im->h / (float)oh;
    size_t rn = strlen(d_trp);
    unsigned char tlut[256];
    uint64_t *sa = NULL;
    uint64_t *sq = NULL;
    size_t st = 0;
    int ok_sa = mk_sat2(im, &sa, &sq, &st);

    char *ln = (char *)malloc((size_t)ow);
    size_t esz = 0;
    float *e0 = NULL;
    float *e1 = NULL;

    mk_tlut(im, tlut);

    if (smul((size_t)(ow + 2), sizeof(float), &esz)) {
        e0 = (float *)calloc((size_t)(ow + 2), sizeof(float));
        e1 = (float *)calloc((size_t)(ow + 2), sizeof(float));
    }

    int ok_edf = (e0 != NULL && e1 != NULL);
    int qn = (int)rn;
    if (qn < 2) qn = 2;

    for (int y = 0; y < oh; y++) {
        int y0 = (int)(y * sy);
        int y1 = (int)((y + 1) * sy);

        if (y0 < 0) y0 = 0;
        if (y1 <= y0) y1 = y0 + 1;
        if (y1 > im->h) y1 = im->h;
        if (y0 >= im->h) {
            y0 = im->h - 1;
            y1 = im->h;
        }

        if (ok_edf) memset(e1, 0, esz);

        int ltr = ((y & 1) == 0);
        int xs = ltr ? 0 : ow - 1;
        int xe = ltr ? ow : -1;
        int xd = ltr ? 1 : -1;

        for (int x = xs; x != xe; x += xd) {
            int x0 = (int)(x * sx);
            int x1 = (int)((x + 1) * sx);

            if (x0 < 0) x0 = 0;
            if (x1 <= x0) x1 = x0 + 1;
            if (x1 > im->w) x1 = im->w;
            if (x0 >= im->w) {
                x0 = im->w - 1;
                x1 = im->w;
            }

            uint64_t ct = (uint64_t)(x1 - x0) * (uint64_t)(y1 - y0);
            uint64_t sm = 0;

            if (ok_sa) {
                sm = sat_blk(sa, st, x0, y0, x1, y1);
            } else {
                for (int yy = y0; yy < y1; yy++) {
                    for (int xx = x0; xx < x1; xx++) {
                        sm += im->px[(size_t)yy * (size_t)im->w + (size_t)xx];
                    }
                }
            }

            unsigned char av = ct ? (unsigned char)(sm / ct) : 0;
            int cx = (x0 + x1) >> 1;
            int cy = (y0 + y1) >> 1;

            int gxs =
                -(int)pxc(im, cx - 1, cy - 1) - 2 * (int)pxc(im, cx - 1, cy) - (int)pxc(im, cx - 1, cy + 1) +
                (int)pxc(im, cx + 1, cy - 1) + 2 * (int)pxc(im, cx + 1, cy) + (int)pxc(im, cx + 1, cy + 1);
            int gys =
                -(int)pxc(im, cx - 1, cy - 1) - 2 * (int)pxc(im, cx, cy - 1) - (int)pxc(im, cx + 1, cy - 1) +
                (int)pxc(im, cx - 1, cy + 1) + 2 * (int)pxc(im, cx, cy + 1) + (int)pxc(im, cx + 1, cy + 1);
            int gm = (gxs < 0 ? -gxs : gxs) + (gys < 0 ? -gys : gys);

            int rx = (x1 - x0) * 2;
            int ry = (y1 - y0) * 2;
            int nx0, nx1, ny0, ny1;
            uint64_t nct, nsm = 0, nss = 0;

            if (rx < 1) rx = 1;
            if (ry < 1) ry = 1;

            nx0 = cx - rx;
            nx1 = cx + rx + 1;
            ny0 = cy - ry;
            ny1 = cy + ry + 1;
            if (nx0 < 0) nx0 = 0;
            if (ny0 < 0) ny0 = 0;
            if (nx1 > im->w) nx1 = im->w;
            if (ny1 > im->h) ny1 = im->h;

            nct = (uint64_t)(nx1 - nx0) * (uint64_t)(ny1 - ny0);

            if (ok_sa && nct > 0) {
                nsm = sat_blk(sa, st, nx0, ny0, nx1, ny1);
                nss = sat_blk(sq, st, nx0, ny0, nx1, ny1);
            } else {
                for (int yy = ny0; yy < ny1; yy++) {
                    for (int xx = nx0; xx < nx1; xx++) {
                        uint64_t pv = im->px[(size_t)yy * (size_t)im->w + (size_t)xx];
                        nsm += pv;
                        nss += pv * pv;
                    }
                }
            }

            int nav = av;
            long double var = 0.0L;
            if (nct > 0) {
                long double mu = (long double)nsm / (long double)nct;
                nav = (int)(mu + 0.5L);
                var = (long double)nss / (long double)nct - mu * mu;
                if (var < 0.0L) var = 0.0L;
            }

            long double gain = 1.55L;
            if (var < 180.0L) gain = 2.35L;
            else if (var < 800.0L) gain = 1.95L;

            int lv = (int)((long double)av + gain * ((long double)av - (long double)nav));
            if (lv < 0) lv = 0;
            if (lv > 255) lv = 255;

            int idx = x + 1;
            float tv = (float)tlut[lv];
            if (ok_edf) {
                tv += e0[idx];
                if (tv < 0.0f) tv = 0.0f;
                if (tv > 255.0f) tv = 255.0f;
            }

            int gm_thr = (var < 220.0L) ? 76 : 116;
            char ch;
            if (gm > gm_thr) {
                ch = ed_ch(gxs, gys);
                if (ok_edf) e0[idx] = 0.0f;
            } else {
                int qi = (int)((tv * (float)(qn - 1)) / 255.0f + 0.5f);
                if (qi < 0) qi = 0;
                if (qi >= qn) qi = qn - 1;
                ch = d_trp[qi];

                if (ok_edf) {
                    float er = tv - (float)qv_to_u8(qi, qn);
                    if (ltr) {
                        e0[idx + 1] += er * (7.0f / 16.0f);
                        e1[idx - 1] += er * (3.0f / 16.0f);
                        e1[idx] += er * (5.0f / 16.0f);
                        e1[idx + 1] += er * (1.0f / 16.0f);
                    } else {
                        e0[idx - 1] += er * (7.0f / 16.0f);
                        e1[idx + 1] += er * (3.0f / 16.0f);
                        e1[idx] += er * (5.0f / 16.0f);
                        e1[idx - 1] += er * (1.0f / 16.0f);
                    }
                }
            }

            if (ln) ln[x] = ch;
            else fputc(ch, out);
        }

        if (ln) {
            fwrite(ln, 1, (size_t)ow, out);
        }
        fputc('\n', out);

        if (ok_edf) {
            float *tmp = e0;
            e0 = e1;
            e1 = tmp;
        }
    }

    free(sa);
    free(sq);
    free(e0);
    free(e1);
    free(ln);
}

static void prt_use(const char *pn) {
    printf("usage: %s <input.(png|jpg|jpeg)> [output_width] [output_height] [output.txt]\n", pn);
    printf("  input         : input image file (png/jpg/jpeg)\n");
    printf("  output_width  : output width in chars (default: %d)\n", d_ow);
    printf("  output_height : output height in lines (default: %d)\n", d_oh);
    printf("  output.txt    : output file (default: stdout)\n");
}

int main(int ac, char *av[]) {
    if (ac < 2 || strcmp(av[1], "--help") == 0 || strcmp(av[1], "-h") == 0) {
        prt_use(av[0]);
        return (ac < 2) ? 1 : 0;
    }

    const char *in = av[1];
    int ow = (ac > 2) ? atoi(av[2]) : d_ow;
    int oh = (ac > 3) ? atoi(av[3]) : d_oh;
    const char *ofn = (ac > 4) ? av[4] : NULL;

    if (ow <= 0 || ow > 1000) {
        fprintf(stderr, "warn: invalid width, using %d\n", d_ow);
        ow = d_ow;
    }
    if (oh <= 0 || oh > 1000) {
        fprintf(stderr, "warn: invalid height, using %d\n", d_oh);
        oh = d_oh;
    }

    img_t im = {0};
    if (!rd_img(in, &im)) return 1;

    FILE *out = stdout;
    if (ofn) {
        out = fopen(ofn, "w");
        if (!out) {
            fprintf(stderr, "error: cannot create output file %s\n", ofn);
            img_clr(&im);
            return 1;
        }
    }

    wr_asc(&im, ow, oh, out);

    if (ofn) {
        fclose(out);
        printf("ascii art saved to: %s\n", ofn);
    }

    img_clr(&im);
    return 0;
}
