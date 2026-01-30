#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

// Usage:
//   pack_assets <repo_source_root> <out_dir>
// Generates:
//   <out_dir>/messages_packed.h
//   <out_dir>/fuse_nca_packed.h

static void die(const char *msg)
{
	fprintf(stderr, "pack_assets: %s\n", msg);
	exit(1);
}

static char *xxstrdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = (char*)malloc(n);
	if (!p) return NULL;
	memcpy(p, s, n);
	return p;
}

static unsigned char *read_file(const char *path, size_t *out_sz)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "pack_assets: cannot open %s\n", path);
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long szl = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (szl < 0) { fclose(f); return NULL; }
	size_t sz = (size_t)szl;
	unsigned char *buf = (unsigned char*)malloc(sz + 1);
	if (!buf) { fclose(f); return NULL; }
	if (fread(buf, 1, sz, f) != sz) { fclose(f); free(buf); return NULL; }
	fclose(f);
	buf[sz] = 0;
	if (out_sz) *out_sz = sz;
	return buf;
}

static int c_unescape(const char *s, size_t len, unsigned char *out, size_t out_cap, size_t *out_len)
{
	// Basic C string unescape for \n \r \t \\ \" \xHH and octal \ooo.
	size_t oi = 0;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		if (c != '\\') {
			if (oi >= out_cap) return 0;
			out[oi++] = c;
			continue;
		}
		if (i + 1 >= len) return 0;
		unsigned char n = (unsigned char)s[++i];
		switch (n) {
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case '\\': c = '\\'; break;
			case '"': c = '"'; break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7': {
				// octal: up to 3 digits (we already consumed first)
				int val = (n - '0');
				for (int k = 0; k < 2 && i + 1 < len; k++) {
					unsigned char d = (unsigned char)s[i + 1];
					if (d < '0' || d > '7') break;
					i++;
					val = (val * 8) + (d - '0');
				}
				c = (unsigned char)val;
				break;
			}
			case 'x': {
				// \xHH (1-2 hex digits)
				int val = 0;
				int got = 0;
				for (int k = 0; k < 2 && i + 1 < len; k++) {
					unsigned char h = (unsigned char)s[i + 1];
					int v;
					if (h >= '0' && h <= '9') v = h - '0';
					else if ((h | 32) >= 'a' && (h | 32) <= 'f') v = ((h | 32) - 'a') + 10;
					else break;
					i++;
					val = (val * 16) + v;
					got = 1;
				}
				if (!got) return 0;
				c = (unsigned char)val;
				break;
			}
			default:
				// Unknown escape: keep literal
				c = n;
				break;
		}
		if (oi >= out_cap) return 0;
		out[oi++] = c;
	}
	if (out_len) *out_len = oi;
	return 1;
}

static int parse_messages_enum(const char *hdr, size_t hdr_len, char ***out_names, size_t *out_count)
{
	// Parse enum log_msg_id_t list to map name->index.
	const char *p = strstr(hdr, "typedef enum");
	if (!p) return 0;
	p = strstr(p, "log_msg_id_t");
	if (!p) return 0;
	// Find opening brace before log_msg_id_t.
	const char *b = hdr;
	const char *brace = NULL;
	for (const char *q = p; q >= hdr; q--) {
		if (*q == '{') { brace = q; break; }
	}
	if (!brace) return 0;
	const char *q = brace + 1;

	size_t cap = 256;
	size_t cnt = 0;
	char **names = (char**)calloc(cap, sizeof(char*));
	if (!names) return 0;

	while ((size_t)(q - hdr) < hdr_len) {
		while (*q && (isspace((unsigned char)*q) || *q == ',')) q++;
		if (!*q) break;
		if (*q == '}') break;

		// read token
		const char *t0 = q;
		while (*q && (isalnum((unsigned char)*q) || *q == '_')) q++;
		if (q == t0) { q++; continue; }
		size_t tl = (size_t)(q - t0);

		char tmp[256];
		if (tl >= sizeof(tmp)) { free(names); return 0; }
		memcpy(tmp, t0, tl);
		tmp[tl] = 0;

		if (strcmp(tmp, "LOG_MSG_COUNT") == 0) {
			break;
		}

		if (cnt >= cap) {
			cap *= 2;
			char **nn = (char**)realloc(names, cap * sizeof(char*));
			if (!nn) return 0;
			names = nn;
		}
		names[cnt++] = xxstrdup(tmp);

		// skip until comma or end
		while (*q && *q != ',' && *q != '}') q++;
		if (*q == '}') break;
	}

	*out_names = names;
	*out_count = cnt;
	return 1;
}

static int find_name_index(char **names, size_t count, const char *name)
{
	for (size_t i = 0; i < count; i++) {
		if (names[i] && strcmp(names[i], name) == 0)
			return (int)i;
	}
	return -1;
}

static void write_byte_array(FILE *o, const char *sym, const unsigned char *buf, size_t sz)
{
	fprintf(o, "static const unsigned char %s[%zu] = {\n", sym, sz);
	for (size_t i = 0; i < sz; i++) {
		if ((i % 12) == 0) fprintf(o, "    ");
		fprintf(o, "0x%02X", buf[i]);
		if (i + 1 != sz) fprintf(o, ",");
		if ((i % 12) == 11 || i + 1 == sz) fprintf(o, "\n");
		else fprintf(o, " ");
	}
	fprintf(o, "};\n");
}

static void gen_messages(const char *src_root, const char *out_dir)
{
	char path_h[1024];
	char path_c[1024];
	snprintf(path_h, sizeof(path_h), "%s/gfx/messages.h", src_root);
	snprintf(path_c, sizeof(path_c), "%s/gfx/messages.c", src_root);

	size_t hdr_len = 0, src_len = 0;
	unsigned char *hdr = read_file(path_h, &hdr_len);
	unsigned char *src = read_file(path_c, &src_len);
	if (!hdr || !src) die("cannot read messages sources");

	char **names = NULL;
	size_t msg_count = 0;
	if (!parse_messages_enum((const char*)hdr, hdr_len, &names, &msg_count) || msg_count == 0) {
		die("failed to parse log_msg_id_t enum from messages.h");
	}

	// Offsets (u32 during build), pool as bytes.
	uint32_t *offs = (uint32_t*)calloc(msg_count, sizeof(uint32_t));
	uint8_t *present = (uint8_t*)calloc(msg_count, 1);
	unsigned char *pool = (unsigned char*)malloc(1);
	size_t pool_sz = 0;
	if (!offs || !present || !pool) die("oom");

	// Parse lines like: [LOG_MSG_xxx] = "...",
	const char *p = (const char*)src;
	while ((p = strstr(p, "[LOG_MSG_")) != NULL) {
		const char *name0 = p + 1;
		const char *name1 = name0;
		while (*name1 && (isalnum((unsigned char)*name1) || *name1 == '_')) name1++;
		if (*name1 != ']') { p = name1; continue; }

		char name[256];
		size_t nl = (size_t)(name1 - name0);
		if (nl >= sizeof(name)) { p = name1; continue; }
		memcpy(name, name0, nl);
		name[nl] = 0;

		int idx = find_name_index(names, msg_count, name);
		if (idx < 0) { p = name1; continue; }

		const char *eq = strstr(name1, "=");
		if (!eq) { p = name1; continue; }
		const char *q = eq;
		while (*q && *q != '"') q++;
		if (*q != '"') { p = name1; continue; }
		q++; // after opening quote

		// Find closing quote not escaped.
		const char *s0 = q;
		const char *s1 = q;
		int esc = 0;
		while (*s1) {
			if (!esc && *s1 == '"') break;
			if (!esc && *s1 == '\\') esc = 1;
			else esc = 0;
			s1++;
		}
		if (*s1 != '"') { p = name1; continue; }

		// Unescape into temp.
		size_t lit_len = (size_t)(s1 - s0);
		unsigned char tmp[2048];
		size_t out_len = 0;
		if (lit_len >= sizeof(tmp)) { p = s1; continue; }
		if (!c_unescape(s0, lit_len, tmp, sizeof(tmp), &out_len)) { p = s1; continue; }

		offs[idx] = (uint32_t)pool_sz;
		present[idx] = 1;
		pool = (unsigned char*)realloc(pool, pool_sz + out_len + 1);
		if (!pool) die("oom");
		memcpy(pool + pool_sz, tmp, out_len);
		pool[pool_sz + out_len] = 0;
		pool_sz += out_len + 1;

		p = s1;
	}

	// Fill missing entries with empty strings.
	for (size_t i = 0; i < msg_count; i++) {
		if (!present[i]) {
			offs[i] = (uint32_t)pool_sz;
			pool = (unsigned char*)realloc(pool, pool_sz + 1);
			if (!pool) die("oom");
			pool[pool_sz++] = 0;
			present[i] = 1;
		}
	}

	// Emit messages pool uncompressed + adaptive offsets.
	char out_path[1024];
	snprintf(out_path, sizeof(out_path), "%s/messages_packed.h", out_dir);
	FILE *o = fopen(out_path, "wb");
	if (!o) die("cannot write messages_packed.h");

	fprintf(o, "#ifndef _MESSAGES_PACKED_H_\n");
	fprintf(o, "#define _MESSAGES_PACKED_H_\n\n");
	fprintf(o, "// Auto-generated. DO NOT EDIT.\n");
	fprintf(o, "#include <utils/types.h>\n\n");

	fprintf(o, "static const uint32_t g_logmsg_pool_size = %zuU;\n", pool_sz);

	if (pool_sz <= 0xFFFFu) {
		fprintf(o, "typedef uint16_t logmsg_off_t;\n");
		fprintf(o, "static const logmsg_off_t g_logmsg_offsets[%zu] = {\n", msg_count);
		for (size_t i = 0; i < msg_count; i++) {
			if ((i % 12) == 0) fprintf(o, "    ");
			fprintf(o, "%u", (unsigned)offs[i]);
			if (i + 1 != msg_count) fprintf(o, ",");
			if ((i % 12) == 11 || i + 1 == msg_count) fprintf(o, "\n");
			else fprintf(o, " ");
		}
		fprintf(o, "};\n\n");
	} else {
		fprintf(o, "typedef uint32_t logmsg_off_t;\n");
		fprintf(o, "static const logmsg_off_t g_logmsg_offsets[%zu] = {\n", msg_count);
		for (size_t i = 0; i < msg_count; i++) {
			if ((i % 8) == 0) fprintf(o, "    ");
			fprintf(o, "%u", (unsigned)offs[i]);
			if (i + 1 != msg_count) fprintf(o, ",");
			if ((i % 8) == 7 || i + 1 == msg_count) fprintf(o, "\n");
			else fprintf(o, " ");
		}
		fprintf(o, "};\n\n");
	}

	write_byte_array(o, "g_logmsg_pool", pool, pool_sz);

fprintf(o, "\n#endif\n");

	fclose(o);

	// cleanup
	free(hdr); free(src);
	for (size_t i = 0; i < msg_count; i++) free(names[i]);
	free(names);
	free(present);
	free(offs); free(pool);
}

static int parse_hex32_to_16(const char *hex, unsigned char out16[16])
{
	for (int i = 0; i < 16; i++) {
		int hi = hex[i*2];
		int lo = hex[i*2+1];
		hi = (hi >= '0' && hi <= '9') ? (hi - '0') : ((hi | 32) - 'a' + 10);
		lo = (lo >= '0' && lo <= '9') ? (lo - '0') : ((lo | 32) - 'a' + 10);
		if ((unsigned)hi > 15U || (unsigned)lo > 15U) return 0;
		out16[i] = (unsigned char)((hi<<4)|lo);
	}
	return 1;
}

static void gen_fuse_nca(const char *src_root, const char *out_dir)
{
	char path_c[1024];
	snprintf(path_c, sizeof(path_c), "%s/fuse_check/fuse_check.c", src_root);

	size_t src_len = 0;
	unsigned char *src = read_file(path_c, &src_len);
	if (!src) die("cannot read fuse_check.c");

	// Extract entries like: {21, 2, 0, "ac1e...507f.nca"},
	const char *p = (const char*)src;
	size_t cap = 256;
	size_t cnt = 0;
	struct rec { unsigned char major, minor, patch, id[16]; } *recs = (struct rec*)malloc(cap * sizeof(*recs));
	if (!recs) die("oom");

	while ((p = strchr(p, '{')) != NULL) {
		int major, minor, patch;
		char hex[64];
		// Very strict scan: {d, d, d, "<32hex>.nca"}
		int n = sscanf(p, "{%d, %d, %d, \"%32[0-9a-fA-F].nca\"}", &major, &minor, &patch, hex);
		if (n == 4) {
			unsigned char id[16];
			if (parse_hex32_to_16(hex, id)) {
				if (cnt >= cap) {
					cap *= 2;
					recs = (struct rec*)realloc(recs, cap * sizeof(*recs));
					if (!recs) die("oom");
				}
				recs[cnt].major = (unsigned char)major;
				recs[cnt].minor = (unsigned char)minor;
				recs[cnt].patch = (unsigned char)patch;
				memcpy(recs[cnt].id, id, 16);
				cnt++;
			}
		}
		p++;
	}

	if (cnt == 0) die("no NCA entries found in fuse_check.c");

	char out_path[1024];
	snprintf(out_path, sizeof(out_path), "%s/fuse_nca_packed.h", out_dir);
	FILE *o = fopen(out_path, "wb");
	if (!o) die("cannot write fuse_nca_packed.h");

	fprintf(o, "#ifndef _FUSE_NCA_PACKED_H_\n");
	fprintf(o, "#define _FUSE_NCA_PACKED_H_\n\n");
	fprintf(o, "// Auto-generated. DO NOT EDIT.\n");
	fprintf(o, "#include <utils/types.h>\n\n");
	fprintf(o, "static const nca_map_t nca_db[%zu] = {\n", cnt);
	for (size_t i = 0; i < cnt; i++) {
		fprintf(o, "    {%u, %u, %u, {", (unsigned)recs[i].major, (unsigned)recs[i].minor, (unsigned)recs[i].patch);
		for (int b = 0; b < 16; b++) {
			fprintf(o, "0x%02X", recs[i].id[b]);
			if (b != 15) fprintf(o, ",");
		}
		fprintf(o, "}},\n");
	}
	fprintf(o, "};\n\n#endif\n");

	fclose(o);
	free(src);
	free(recs);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <source_root> <out_dir>\n", argv[0]);
		return 2;
	}
	const char *src_root = argv[1];
	const char *out_dir = argv[2];

	gen_messages(src_root, out_dir);
	gen_fuse_nca(src_root, out_dir);

	return 0;
}
