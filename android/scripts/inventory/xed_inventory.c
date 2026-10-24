// Guest code inventory, step 2: decodes the functions (ranges from .eh_frame) with XED and
// counts the instruction forms (iform: the same grid as Remill's DEF_ISEL).
// Build (WSL): see scripts/inventory/run.sh
// Run: xed_inventory <name> <seg.bin> <seg_vaddr> <ranges> <out_prefix>
//   <out_prefix>.iforms.tsv : iform  count  functions  isa_set  extension  sample_addr  sample_hex  disasm
//   <out_prefix>.funcs      : start size  iform_id...   (the function's unique forms; id = XED enum)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xed/xed-interface.h>

typedef struct {
    uint64_t count, funcs, sample_addr;
    uint8_t sample[15];
    unsigned sample_len, last_func;
    char disasm[96];
} stat_t;

static stat_t stats[XED_IFORM_LAST];

int main(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "usage: %s name seg.bin seg_vaddr ranges out_prefix\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[2], "rb");
    if (!f) { perror(argv[2]); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *seg = malloc((size_t)size);
    if (fread(seg, 1, (size_t)size, f) != (size_t)size) return 1;
    fclose(f);
    uint64_t base = strtoull(argv[3], NULL, 0);

    xed_tables_init();
    FILE *rf = fopen(argv[4], "r");
    char path[512];
    snprintf(path, sizeof path, "%s.funcs", argv[5]);
    FILE *ff = fopen(path, "w");
    if (!rf || !ff) { perror("ranges/funcs"); return 1; }

    uint64_t start, len, total = 0, errors = 0, nfunc = 0, outside = 0;
    unsigned func_id = 0;
    static unsigned seen[XED_IFORM_LAST];
    unsigned *list = malloc(sizeof(unsigned) * XED_IFORM_LAST);
    while (fscanf(rf, "%lx %lx", &start, &len) == 2) {
        func_id++;
        if (start < base || start + len > base + (uint64_t)size) { outside++; continue; }
        nfunc++;
        unsigned nlist = 0;
        uint64_t pc = start;
        while (pc < start + len) {
            xed_decoded_inst_t x;
            xed_decoded_inst_zero(&x);
            xed_decoded_inst_set_mode(&x, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
            unsigned avail = (unsigned)(start + len - pc);
            if (avail > 15) avail = 15;
            const uint8_t *p = seg + (pc - base);
            xed_error_enum_t e = xed_decode(&x, p, avail);
            if (e != XED_ERROR_NONE) { errors++; pc++; continue; }
            unsigned n = xed_decoded_inst_get_length(&x);
            xed_iform_enum_t form = xed_decoded_inst_get_iform_enum(&x);
            stat_t *s = &stats[form];
            if (!s->count) {
                s->sample_addr = pc;
                s->sample_len = n;
                memcpy(s->sample, p, n);
                xed_format_context(XED_SYNTAX_INTEL, &x, s->disasm, sizeof s->disasm, pc, 0, 0);
            }
            s->count++;
            total++;
            if (s->last_func != func_id) {
                s->last_func = func_id;
                s->funcs++;
            }
            if (seen[form] != func_id) { seen[form] = func_id; list[nlist++] = form; }
            pc += n;
        }
        fprintf(ff, "%lx %lx", start, len);
        for (unsigned i = 0; i < nlist; i++) fprintf(ff, " %u", list[i]);
        fputc('\n', ff);
    }
    fclose(ff);
    fclose(rf);

    snprintf(path, sizeof path, "%s.iforms.tsv", argv[5]);
    FILE *of = fopen(path, "w");
    unsigned forms = 0;
    for (unsigned i = 0; i < XED_IFORM_LAST; i++) {
        stat_t *s = &stats[i];
        if (!s->count) continue;
        forms++;
        xed_decoded_inst_t x;
        xed_decoded_inst_zero(&x);
        xed_decoded_inst_set_mode(&x, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
        xed_decode(&x, s->sample, s->sample_len);
        fprintf(of, "%s\t%lu\t%lu\t%s\t%s\t%lx\t", xed_iform_enum_t2str((xed_iform_enum_t)i), s->count,
                s->funcs, xed_isa_set_enum_t2str(xed_decoded_inst_get_isa_set(&x)),
                xed_extension_enum_t2str(xed_decoded_inst_get_extension(&x)), s->sample_addr);
        for (unsigned k = 0; k < s->sample_len; k++) fprintf(of, "%02x", s->sample[k]);
        fprintf(of, "\t%s\t%u\n", s->disasm, i);
    }
    fclose(of);
    printf("%s: %lu functions (%lu outside segment), %lu instructions, %u iforms, %lu decode errors\n",
           argv[1], nfunc, outside, total, forms, errors);
    return 0;
}
