/*
 * Copyright (C) 2010 Francisco Jerez.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <subdev/fb.h>

struct nv20_fb_priv {
	struct nouveau_fb base;
};

int
nv20_fb_vram_init(struct nouveau_fb *pfb)
{
	u32 pbus1218 = nv_rd32(pfb, 0x001218);

	switch (pbus1218 & 0x00000300) {
	case 0x00000000: pfb->ram.type = NV_MEM_TYPE_SDRAM; break;
	case 0x00000100: pfb->ram.type = NV_MEM_TYPE_DDR1; break;
	case 0x00000200: pfb->ram.type = NV_MEM_TYPE_GDDR3; break;
	case 0x00000300: pfb->ram.type = NV_MEM_TYPE_GDDR2; break;
	}
	pfb->ram.size  = (nv_rd32(pfb, 0x10020c) & 0xff000000);
	pfb->ram.parts = (nv_rd32(pfb, 0x100200) & 0x00000003) + 1;

	return nv_rd32(pfb, 0x100320);
}

void
nv20_fb_tile_init(struct nouveau_fb *pfb, int i, u32 addr, u32 size, u32 pitch,
		  u32 flags, struct nouveau_fb_tile *tile)
{
	tile->addr  = 0x00000001 | addr;
	tile->limit = max(1u, addr + size) - 1;
	tile->pitch = pitch;
	if (flags & 4) {
		pfb->tile.comp(pfb, i, size, flags, tile);
		tile->addr |= 2;
	}
}

static void
nv20_fb_tile_comp(struct nouveau_fb *pfb, int i, u32 size, u32 flags,
		  struct nouveau_fb_tile *tile)
{
	u32 tiles = DIV_ROUND_UP(size, 0x40);
	u32 tags  = round_up(tiles / pfb->ram.parts, 0x40);
	if (!nouveau_mm_head(&pfb->tags, 1, tags, tags, 1, &tile->tag)) {
		if (!(flags & 2)) tile->zcomp = 0x00000000; /* Z16 */
		else              tile->zcomp = 0x04000000; /* Z24S8 */
		tile->zcomp |= tile->tag->offset;
		tile->zcomp |= 0x80000000; /* enable */
#ifdef __BIG_ENDIAN
		tile->zcomp |= 0x08000000;
#endif
	}
}

void
nv20_fb_tile_fini(struct nouveau_fb *pfb, int i, struct nouveau_fb_tile *tile)
{
	tile->addr  = 0;
	tile->limit = 0;
	tile->pitch = 0;
	tile->zcomp = 0;
	nouveau_mm_free(&pfb->tags, &tile->tag);
}

void
nv20_fb_tile_prog(struct nouveau_fb *pfb, int i, struct nouveau_fb_tile *tile)
{
	nv_wr32(pfb, 0x100244 + (i * 0x10), tile->limit);
	nv_wr32(pfb, 0x100248 + (i * 0x10), tile->pitch);
	nv_wr32(pfb, 0x100240 + (i * 0x10), tile->addr);
	nv_rd32(pfb, 0x100240 + (i * 0x10));
	nv_wr32(pfb, 0x100300 + (i * 0x04), tile->zcomp);
}

static int
nv20_fb_ctor(struct nouveau_object *parent, struct nouveau_object *engine,
	     struct nouveau_oclass *oclass, void *data, u32 size,
	     struct nouveau_object **pobject)
{
	struct nv20_fb_priv *priv;
	int ret;

	ret = nouveau_fb_create(parent, engine, oclass, &priv);
	*pobject = nv_object(priv);
	if (ret)
		return ret;

	priv->base.memtype_valid = nv04_fb_memtype_valid;
	priv->base.ram.init = nv20_fb_vram_init;
	priv->base.tile.regions = 8;
	priv->base.tile.init = nv20_fb_tile_init;
	priv->base.tile.comp = nv20_fb_tile_comp;
	priv->base.tile.fini = nv20_fb_tile_fini;
	priv->base.tile.prog = nv20_fb_tile_prog;
	return nouveau_fb_preinit(&priv->base);
}

struct nouveau_oclass
nv20_fb_oclass = {
	.handle = NV_SUBDEV(FB, 0x20),
	.ofuncs = &(struct nouveau_ofuncs) {
		.ctor = nv20_fb_ctor,
		.dtor = _nouveau_fb_dtor,
		.init = _nouveau_fb_init,
		.fini = _nouveau_fb_fini,
	},
};
