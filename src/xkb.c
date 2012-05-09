/************************************************************
Copyright (c) 1993 by Silicon Graphics Computer Systems, Inc.

Permission to use, copy, modify, and distribute this
software and its documentation for any purpose and without
fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting
documentation, and that the name of Silicon Graphics not be
used in advertising or publicity pertaining to distribution
of the software without specific prior written permission.
Silicon Graphics makes no representation about the suitability
of this software for any purpose. It is provided "as is"
without any express or implied warranty.

SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

#include "xkb-priv.h"

static char *
XkbcCanonicaliseComponent(char *name, const char *old)
{
    char *tmp;
    int i;

    if (!name)
        return NULL;

    /* Treachery. */
    if (old && strchr(old, '%'))
        return NULL;

    if (name[0] == '+' || name[0] == '|') {
        if (old) {
            tmp = malloc(strlen(name) + strlen(old) + 1);
            if (!tmp)
                return NULL;
            sprintf(tmp, "%s%s", old, name);
            free(name);
            name = tmp;
        }
        else {
            memmove(name, &name[1], strlen(&name[1]) + 1);
        }
    }

    for (i = 0; name[i]; i++) {
        if (name[i] == '%') {
            if (old) {
                tmp = malloc(strlen(name) + strlen(old));
                if (!tmp)
                    return NULL;
                strncpy(tmp, name, i);
                strcat(tmp + i, old);
                strcat(tmp + i + strlen(old), &name[i + 1]);
                free(name);
                name = tmp;
                i--;
            }
            else {
                memmove(&name[i - 1], &name[i + 1], strlen(&name[i + 1]) + 1);
                i -= 2;
            }
        }
    }

    return name;
}

_X_EXPORT void
xkb_canonicalise_components(struct xkb_component_names * names,
                            const struct xkb_component_names * old)
{
    names->keycodes = XkbcCanonicaliseComponent(names->keycodes,
                                                old ? old->keycodes : NULL);
    names->compat = XkbcCanonicaliseComponent(names->compat,
                                              old ? old->compat : NULL);
    names->symbols = XkbcCanonicaliseComponent(names->symbols,
                                               old ? old->symbols : NULL);
    names->types = XkbcCanonicaliseComponent(names->types,
                                             old ? old->types : NULL);
}

static bool
VirtualModsToReal(struct xkb_keymap *keymap, unsigned virtual_mask,
                  unsigned *mask_rtrn)
{
    int i, bit;
    unsigned mask;

    if (!keymap)
        return false;
    if (virtual_mask == 0) {
        *mask_rtrn = 0;
        return true;
    }
    if (!keymap->server)
        return false;

    for (i = mask = 0, bit = 1; i < XkbNumVirtualMods; i++, bit <<= 1) {
        if (virtual_mask & bit)
            mask |= keymap->server->vmods[i];
    }

    *mask_rtrn = mask;
    return true;
}

bool
XkbcComputeEffectiveMap(struct xkb_keymap *keymap, struct xkb_key_type *type,
                        unsigned char *map_rtrn)
{
    int i;
    unsigned tmp;
    struct xkb_kt_map_entry * entry = NULL;

    if (!keymap || !type || !keymap->server)
        return false;

    if (type->mods.vmods != 0) {
        if (!VirtualModsToReal(keymap, type->mods.vmods, &tmp))
            return false;

        type->mods.mask = tmp | type->mods.real_mods;
        entry = type->map;
        for (i = 0; i < type->map_count; i++, entry++) {
            tmp = 0;
            if (entry->mods.vmods != 0) {
                if (!VirtualModsToReal(keymap, entry->mods.vmods, &tmp))
                    return false;
                if (tmp == 0) {
                    entry->active = false;
                    continue;
                }
            }
            entry->active = true;
            entry->mods.mask = (entry->mods.real_mods | tmp) & type->mods.mask;
        }
    }
    else
        type->mods.mask = type->mods.real_mods;

    if (map_rtrn) {
        memset(map_rtrn, 0, type->mods.mask + 1);
        if (entry && entry->active)
            for (i = 0; i < type->map_count; i++)
                map_rtrn[type->map[i].mods.mask] = type->map[i].level;
    }

    return true;
}
