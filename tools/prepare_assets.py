#!/usr/bin/env python3
"""Normalise the design assets in `image assets/` into `res/`.

The source artwork comes from three different drawing programs and is authored
at three different scales (mm, 72dpi px, and raw Serif px). VCV Rack measures
everything in "rack px" (75 dpi, 1 HP = 15 px, panel height = 380 px), so every
control is rewritten here with an explicit rack-px width/height plus a viewBox
covering its original coordinate space. That keeps the C++ free of per-asset
scale factors: a widget is simply as big as its SVG says it is.

Panels are 179x380 px = 12 HP and are copied through untouched (they are PNGs
and are drawn by FmdPngPanel).

The panel exports arrived with their type layer switched off, so the printed
labels are rebuilt separately into res/labels/ from the `*-panel-type.svg`
files and drawn over the raster panel as vector -- see "Panel typography".

Run from anywhere:  python tools/prepare_assets.py
"""

import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
SRC = os.path.join(os.path.dirname(PROJECT), "image assets")
RES = os.path.join(PROJECT, "res")

# ---------------------------------------------------------------------------
# Target sizes, in rack px. Diameter for knobs, bounding box for everything
# else. Sizes are taken from the two mockups; the notes say pixel accuracy is
# not required, so these are "reads correctly at 1x zoom" values.
# ---------------------------------------------------------------------------
# (source, dest, target_width, rotate_degrees)  -- height follows the aspect
CONTROLS = [
    # -- shared -------------------------------------------------------------
    ("common/jack.svg",                     "common/Jack.svg",            24.0, 0),
    ("common/KnobTrimmer-bright-03-base.svg",    "common/Trimmer-1-base.svg",    20.0, 0),
    ("common/KnobTrimmer-bright-02-turn.svg",    "common/Trimmer-2-turn.svg",    20.0, 0),
    ("common/KnobTrimmer-bright-01-overlay.svg", "common/Trimmer-3-overlay.svg", 20.0, 0),
    # dark variant of the trimmer, kept for reference / easy swapping
    ("common/KnobTrimmer-04-base.svg",      "common/TrimmerDark-1-base.svg",    20.0, 0),
    ("common/KnobTrimmer-03-TURN.svg",      "common/TrimmerDark-2-turn.svg",    18.9, 0),
    ("common/KnobTrimmer-02-overlay.svg",   "common/TrimmerDark-3-overlay.svg", 18.0, 0),
    ("common/KnobTrimmer-01-pointer.svg",   "common/TrimmerDark-4-pointer.svg", 18.9, 0),

    # -- Flower Child -------------------------------------------------------
    ("Flower Child/Controls/KnobBigMoog-06-base.svg",       "FlowerChild/KnobBig-1-base.svg",      62.0, 0),
    ("Flower Child/Controls/KnobBigMoog-05-top-TURN.svg",   "FlowerChild/KnobBig-2-topturn.svg",   50.2, 0),
    ("Flower Child/Controls/KnobBigMoog-04-highlight.svg",  "FlowerChild/KnobBig-3-highlight.svg", 49.3, 0),
    ("Flower Child/Controls/KnobBigMoog-03-skirt-TURN.svg", "FlowerChild/KnobBig-4-skirtturn.svg", 62.0, 0),
    ("Flower Child/Controls/KnobBigMoog-02-top-light.svg",  "FlowerChild/KnobBig-5-toplight.svg",  57.2, 0),
    ("Flower Child/Controls/KnobBigMoog-01-top.svg",        "FlowerChild/KnobBig-6-top.svg",       61.8, 0),

    ("Flower Child/Controls/KnobSmallMoog-03-base.svg",     "FlowerChild/KnobSmall-1-base.svg",    31.3, 0),
    ("Flower Child/Controls/KnobSmallMoog-02-TURN.svg",     "FlowerChild/KnobSmall-2-turn.svg",    32.0, 0),
    ("Flower Child/Controls/KnobSmallMoog-01-top.svg",      "FlowerChild/KnobSmall-3-top.svg",     31.6, 0),

    ("Flower Child/Controls/ButtonLED-OFF.svg",             "FlowerChild/ButtonLED-0-off.svg",     26.0, 0),
    ("Flower Child/Controls/ButtonLED-ON.svg",              "FlowerChild/ButtonLED-1-on.svg",      33.8, 0),

    # -- Shaped Resonator ---------------------------------------------------
    ("Shaped Resonator/Controls/KnobPolivoks-03-base.svg",    "ShapedResonator/KnobBig-1-base.svg",    57.0, 0),
    ("Shaped Resonator/Controls/KnobPolivoks-02-turn.svg",    "ShapedResonator/KnobBig-2-turn.svg",    57.0, 0),
    ("Shaped Resonator/Controls/KnobPolivoks-01-overlay.svg", "ShapedResonator/KnobBig-3-overlay.svg", 57.0, 0),

    # The fader handle is drawn portrait but sits on a vertical fader, where the
    # handle must be wide and short -- rotate it a quarter turn.
    ("Shaped Resonator/Controls/FaderPolivoks.svg",           "ShapedResonator/FaderHandle.svg",       17.0, 90),

    ("Shaped Resonator/Controls/ButtonSquare-OFF.svg",        "ShapedResonator/Button-0-off.svg",      19.0, 0),
    ("Shaped Resonator/Controls/ButtonSquare-ON.svg",         "ShapedResonator/Button-1-on.svg",       21.4, 0),
    ("Shaped Resonator/Controls/IconShape1.svg",              "ShapedResonator/IconShape1.svg",        12.0, 0),
    ("Shaped Resonator/Controls/IconShape2.svg",              "ShapedResonator/IconShape2.svg",        12.0, 0),
    ("Shaped Resonator/Controls/IconShape3.svg",              "ShapedResonator/IconShape3.svg",        12.0, 0),

    # -- Super Love ---------------------------------------------------------
    ("Super Love/Controls/KnobSLLarge-03-base.svg",    "SuperLove/KnobLarge-1-base.svg",    60.0, 0),
    ("Super Love/Controls/KnobSLLarge-02-turn.svg",    "SuperLove/KnobLarge-2-turn.svg",    60.0, 0),
    ("Super Love/Controls/KnobSLLarge-01-overlay.svg", "SuperLove/KnobLarge-3-overlay.svg", 60.0, 0),

    ("Super Love/Controls/KnobSLMed-03-base.svg",      "SuperLove/KnobMed-1-base.svg",      44.0, 0),
    ("Super Love/Controls/KnobSLMed-02-turn.svg",      "SuperLove/KnobMed-2-turn.svg",      44.0, 0),
    ("Super Love/Controls/KnobSLMed-01-overlay.svg",   "SuperLove/KnobMed-3-overlay.svg",   44.0, 0),

    ("Super Love/Controls/KnobSLSmall-03-base.svg",    "SuperLove/KnobSmall-1-base.svg",    35.0, 0),
    ("Super Love/Controls/KnobSLSmall-02-turn.svg",    "SuperLove/KnobSmall-2-turn.svg",    35.0, 0),
    ("Super Love/Controls/KnobSLSmall-01-overlay.svg", "SuperLove/KnobSmall-3-overlay.svg", 35.0, 0),

    ("Super Love/Controls/KnobSLslider.svg",           "SuperLove/SliderHandle.svg",         7.0, 0),
]

# The 950 px exports are 2.5x the 380 px panel height. Rack loads images without
# mipmaps, so a 4x source aliases badly when drawn at 1x zoom; 2.5x is the best
# trade between sharpness when zoomed in and shimmer when zoomed out.
PANELS = [
    ("Flower Child/Panel/FMD-FC-panel-950.png",      "panels/FlowerChild.png"),
    ("Flower Child/Panel/FMD-FC-panel-AGGR-950.png", "panels/FlowerChildAggr.png"),
    ("Shaped Resonator/Panel/FMD-SR-panel-950.png",  "panels/ShapedResonator.png"),
    ("Super Love/Panel/FMD-SL-panel-1520.png",       "panels/SuperLove.png"),
]

# ---------------------------------------------------------------------------
# Panel typography
# ---------------------------------------------------------------------------
# The panel PNGs were exported without their type layer -- there is no FREQ,
# RES, NOISE, DRIVE, SPREAD, IN, CLIP or OUT anywhere on them, only the title
# art, the fader slots and the "- +" attenuverter marks. The missing labels sit
# unused in the `*-panel-type.svg` files beside the panel exports, and are
# rebuilt here into a vector overlay that the module widgets draw on top of the
# raster panel.
#
# The three files are two different exports of the same idea:
#
#   Flower Child  plain <path> glyphs, already fill="#d3d3d3", every one of them
#                 carrying the same translate() that crops the page down to the
#                 type bounding box. LABEL_SHEETS undoes that with a wrapper
#                 group, so the paths land back in panel space.
#
#   SR / SL       Illustrator's "text filled with an image": each label is a
#                 <clipPath> full of glyph outlines with a full-page raster
#                 painted through it. Every one of those rasters is a blank
#                 white page with just the label region at #d3d3d3, so they
#                 carry no information at all -- the clip path outlines *are*
#                 the labels, and the colour is the same #d3d3d3 that Flower
#                 Child states outright.
#
# Rack's nanosvg supports neither clipPath nor <image> nor <style> (see the
# README), so nothing from the source survives except the path geometry: the
# output is a flat list of <path d= fill=> in the source artboard's coordinate
# space, one <g id> per printed label.

# Rack's module rect for the 12 HP panels these labels sit on.
PANEL_W = 179.0
PANEL_H = 380.0

LABEL_FILL = "#d3d3d3"

# The Illustrator exports name their clip paths `clippath-N`, so the label text
# is not recoverable from those two files. These are the words printed on the
# panel, read off the mockups and keyed by clip path id. Flower Child needs no
# table -- it names its own groups.
SR_LABEL_NAMES = {
    "clippath-1": "RES", "clippath-3": "CRNCH", "clippath-5": "DRV",
    "clippath-7": "SPRD", "clippath-9": "FREQ", "clippath-11": "IN",
    "clippath-13": "CLIP", "clippath-15": "OUT",
}
SL_LABEL_NAMES = {
    "clippath-1": "FREQ", "clippath-3": "RES",
    "clippath-5": "LP6", "clippath-7": "LP12", "clippath-9": "BP", "clippath-11": "HP",
    "clippath-13": "NOISE", "clippath-15": "DRIVE", "clippath-17": "SPREAD",
    "clippath-19": "IN", "clippath-21": "CLIP", "clippath-23": "OUT",
    # The row printed above the CV jacks repeats three of the knob names, so
    # these carry a suffix to keep the group ids unique.
    "clippath-25": "NOISE-CV", "clippath-27": "FREQ-CV", "clippath-29": "DRIVE-CV",
}
FC_LABEL_NAMES = None  # the source already names every group

# (source, dest, artboard w, artboard h, wrapper transform, id -> name)
LABEL_SHEETS = [
    ("Flower Child/Panel/FMD-FC-panel-type.svg", "labels/FlowerChild.svg",
     171.97, 364.45, "translate(14.15,34.89)", FC_LABEL_NAMES),
    ("Shaped Resonator/Panel/FMD-SR-panel-type.svg", "labels/ShapedResonator.svg",
     171.72, 364.2, None, SR_LABEL_NAMES),
    ("Super Love/Panel/FMD-SL-panel-type.svg", "labels/SuperLove.svg",
     171.72, 364.2, None, SL_LABEL_NAMES),
]

# What each panel is meant to print, as a cross-check on the extraction: if a
# design file gains or loses a label, the run says so instead of quietly
# shipping a panel with a word missing.
LABEL_INVENTORY = {
    "labels/FlowerChild.svg": [
        "FREQ", "AGGR", "RES", "NOISE", "DRIVE", "SPREAD", "IN", "CLIP", "OUT"],
    "labels/ShapedResonator.svg": [
        "RES", "CRNCH", "DRV", "SPRD", "FREQ", "IN", "CLIP", "OUT"],
    "labels/SuperLove.svg": [
        "NOISE", "DRIVE", "FREQ", "RES", "SPREAD",
        # Printed sheet is post-edited to LP18/LP24/HP/BP by
        # tools/_relabel_superlove_modes.py (do not re-extract over that).
        "LP6", "LP12", "BP", "HP",
        "NOISE-CV", "FREQ-CV", "DRIVE-CV",
        "IN", "CLIP", "OUT"],
}

# A label is a <g id="NAME"> (Flower Child) or a <clipPath id="clippath-N">
# (Illustrator); every <path> below a marker belongs to it. Neither `type` nor
# `FMD-FC-panel` nor `Layer_1` matches, and in all three files the only paths
# present are label glyphs, so nothing else is picked up.
LABEL_MARKER = re.compile(r'<(?:g|clipPath)\s+id="([A-Z][A-Z0-9]*|clippath-\d+)"')
LABEL_PATH = re.compile(r"<path\b([^>]*?)/?>", re.I)
LABEL_ATTR = re.compile(r'([\w:.-]+)\s*=\s*"([^"]*)"')


def read_labels(src_path, names):
    """Return [(label, [(d, transform), ...]), ...] in document order."""
    with open(src_path, encoding="utf-8", errors="replace") as f:
        raw = f.read()

    marks = [(m.start(), m.group(1)) for m in LABEL_MARKER.finditer(raw)]
    if not marks:
        raise ValueError("no label groups and no clip paths in the file")

    found = []
    index = {}
    for m in LABEL_PATH.finditer(raw):
        attrs = dict(LABEL_ATTR.findall(m.group(1)))
        if "d" not in attrs:
            continue

        owner = None
        for pos, name in marks:
            if pos > m.start():
                break
            owner = name
        if owner is None:
            raise ValueError("a <path> sits above every label marker")

        label = names[owner] if names else owner
        if label not in index:
            index[label] = len(found)
            found.append((label, []))
        found[index[label]][1].append((attrs["d"], attrs.get("transform")))

    return found


def write_labels(src_path, dst_path, vw, vh, wrap, names, expected):
    labels = read_labels(src_path, names)

    got = [name for name, _ in labels]
    if sorted(got) != sorted(expected):
        raise ValueError(
            "label inventory changed: missing %s, unexpected %s"
            % (sorted(set(expected) - set(got)) or "-",
               sorted(set(got) - set(expected)) or "-"))

    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg"',
        # The panel raster is drawn stretched into Rack's module rect, so the
        # type has to stretch with it. Without preserveAspectRatio="none",
        # nanosvg fits the viewBox with a uniform scale and centres it, which
        # walks the labels off the controls by a fraction of a pixel.
        '     width="%g" height="%g" viewBox="0 0 %g %g" preserveAspectRatio="none" fill="none">'
        % (PANEL_W, PANEL_H, vw, vh),
    ]
    pad = "  "
    if wrap:
        lines.append('  <g transform="%s">' % wrap)
        pad = "    "
    for name, glyphs in labels:
        lines.append('%s<g id="%s">' % (pad, name))
        for d, transform in glyphs:
            extra = ' transform="%s"' % transform if transform else ""
            lines.append('%s  <path d="%s"%s fill="%s"/>' % (pad, d, extra, LABEL_FILL))
        lines.append("%s</g>" % pad)
    if wrap:
        lines.append("  </g>")
    lines.append("</svg>")

    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    with open(dst_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return got, sum(len(g) for _, g in labels)



SVG_TAG = re.compile(r"<svg\b[^>]*>", re.I)
NUMS = re.compile(r"[-+]?\d*\.?\d+(?:[eE][-+]?\d+)?")

# Rack's bundled nanosvg has no clipPath support at all. The Serif exports wrap
# their artwork in <clipPath id="_clip1"><rect id="export" .../></clipPath>, and
# that rect carries no fill -- so nanosvg falls back to its default of black and
# paints a solid square over the whole control. Strip the clip definitions and
# the references to them; in these assets the clip is only the page bounds, so
# nothing meaningful is lost.
CLIPPATH_EL = re.compile(r"<clipPath\b.*?</clipPath\s*>", re.S | re.I)
CLIPPATH_EMPTY = re.compile(r"<clipPath\b[^>]*/>", re.I)
CLIPPATH_ATTR = re.compile(r"\s*clip-path\s*=\s*\"[^\"]*\"", re.I)


def strip_clip_paths(body):
    body = CLIPPATH_EL.sub("", body)
    body = CLIPPATH_EMPTY.sub("", body)
    return CLIPPATH_ATTR.sub("", body)


# nanosvg understands the `style` ATTRIBUTE only -- it has no CSS engine and no
# notion of `class`. Illustrator's default SVG export puts every fill, gradient
# and opacity in a <style> block and references it by class, which leaves those
# shapes with no fill at all -- and nanosvg's default fill is black, so the
# control renders as a solid black rectangle. Flatten the stylesheet into inline
# style attributes so the declarations survive.
STYLE_EL = re.compile(r"<style\b[^>]*>(.*?)</style\s*>", re.S | re.I)
CSS_COMMENT = re.compile(r"/\*.*?\*/", re.S)
CSS_RULE = re.compile(r"([^{}]+)\{([^{}]*)\}", re.S)
ELEMENT = re.compile(r"<([\w:.-]+)((?:\s+[\w:.-]+\s*=\s*\"[^\"]*\")*)\s*(/?)>")
CLASS_ATTR = re.compile(r"\s+class\s*=\s*\"([^\"]*)\"")
STYLE_ATTR = re.compile(r"\s+style\s*=\s*\"([^\"]*)\"")


def _split_decls(text):
    out = []
    for decl in text.split(";"):
        key, sep, value = decl.partition(":")
        key = key.strip()
        value = value.strip()
        # clip-path is meaningless once the clipPath defs have been removed.
        if key and sep and key.lower() != "clip-path":
            out.append((key, value))
    return out


def inline_css(body):
    sheets = STYLE_EL.findall(body)
    if not sheets:
        return body

    rules = []
    for selector, decls in CSS_RULE.findall(CSS_COMMENT.sub("", "\n".join(sheets))):
        classes = set()
        for part in selector.split(","):
            part = part.strip()
            if part.startswith(".") and " " not in part:
                classes.add(part[1:])
        decls = _split_decls(decls)
        if classes and decls:
            rules.append((classes, decls))

    body = STYLE_EL.sub("", body)
    if not rules:
        return body

    def rewrite(match):
        tag, attrs, closing = match.groups()
        found = CLASS_ATTR.search(attrs)
        if not found:
            return match.group(0)

        names = set(found.group(1).split())
        attrs = attrs[:found.start()] + attrs[found.end():]

        # Applied in stylesheet order so later rules win, as the cascade does.
        props = []
        for classes, decls in rules:
            if classes & names:
                props.extend(decls)

        existing = STYLE_ATTR.search(attrs)
        if existing:
            # An inline style attribute outranks the stylesheet.
            props.extend(_split_decls(existing.group(1)))
            attrs = attrs[:existing.start()] + attrs[existing.end():]

        merged = {}
        for key, value in props:
            merged[key] = value
        if merged:
            ordered = []
            for key, _ in props:
                if key in merged:
                    ordered.append("%s:%s" % (key, merged.pop(key)))
            attrs += ' style="%s"' % ";".join(ordered)

        return "<%s%s%s>" % (tag, attrs, closing)

    return ELEMENT.sub(rewrite, body)


def view_box(tag):
    """Return (x, y, w, h) of the source coordinate space."""
    m = re.search(r'viewBox\s*=\s*"([^"]+)"', tag, re.I)
    if m:
        v = [float(n) for n in NUMS.findall(m.group(1))]
        if len(v) == 4:
            return tuple(v)
    w = re.search(r'\swidth\s*=\s*"([-+.\deE]+)', tag, re.I)
    h = re.search(r'\sheight\s*=\s*"([-+.\deE]+)', tag, re.I)
    if not (w and h):
        raise ValueError("no viewBox and no numeric width/height")
    return (0.0, 0.0, float(w.group(1)), float(h.group(1)))



def convert_pointer(src_path, dst_path, turn_vb=24.09, target_w=18.9):
    """Pad the cropped pointer onto the TURN square so centering hits 12 o'clock."""
    with open(src_path, encoding="utf-8", errors="replace") as f:
        raw = f.read()
    m = SVG_TAG.search(raw)
    if not m:
        raise ValueError("no <svg> element")
    x0, y0, vw, vh = view_box(m.group(0))
    body = inline_css(strip_clip_paths(raw[m.end():raw.rindex("</svg>")]))
    tx = (turn_vb - vw) / 2.0 - x0
    ty = 0.0 - y0
    ns = 'xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"'
    out = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg %s\n'
        '     width="%g" height="%g" viewBox="0 0 %g %g">'
        '<g transform="translate(%g,%g)">%s</g></svg>\n'
        % (ns, target_w, target_w, turn_vb, turn_vb, tx, ty, body)
    )
    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    with open(dst_path, "w", encoding="utf-8") as f:
        f.write(out)
    return turn_vb, turn_vb, target_w, target_w


def convert(src_path, dst_path, target_w, rotate):
    with open(src_path, encoding="utf-8", errors="replace") as f:
        raw = f.read()

    m = SVG_TAG.search(raw)
    if not m:
        raise ValueError("no <svg> element")
    x0, y0, vw, vh = view_box(m.group(0))
    body = inline_css(strip_clip_paths(raw[m.end():raw.rindex("</svg>")]))

    if rotate == 90:
        # (x, y) -> (vh - y, x): swaps the page dimensions.
        nw, nh = vh, vw
        wrap_open = '<g transform="translate(%g,0) rotate(90) translate(%g,%g)">' % (nw, -x0, -y0)
        wrap_close = "</g>"
        vb = "0 0 %g %g" % (nw, nh)
    elif rotate == 0:
        nw, nh = vw, vh
        wrap_open = wrap_close = ""
        vb = "%g %g %g %g" % (x0, y0, vw, vh)
    else:
        raise ValueError("only 0 and 90 degree rotation are implemented")

    # Carry over every namespace the source declared. The Serif exports tag
    # shapes with serif:id, which becomes an unbound prefix -- and invalid XML
    # -- if xmlns:serif is dropped when the <svg> element is rewritten.
    ns = dict(re.findall(r'(xmlns(?::[\w.-]+)?)\s*=\s*"([^"]*)"', m.group(0)))
    ns.setdefault("xmlns", "http://www.w3.org/2000/svg")
    ns.setdefault("xmlns:xlink", "http://www.w3.org/1999/xlink")
    ns_attrs = " ".join('%s="%s"' % (k, v) for k, v in sorted(ns.items()))

    scale = target_w / nw
    out = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg %s\n'
        '     width="%g" height="%g" viewBox="%s">%s%s%s</svg>\n'
        % (ns_attrs, target_w, nh * scale, vb, wrap_open, body, wrap_close)
    )

    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    with open(dst_path, "w", encoding="utf-8") as f:
        f.write(out)
    return nw, nh, target_w, nh * scale


def main():
    if not os.path.isdir(SRC):
        sys.exit("cannot find source artwork at %s" % SRC)

    errors = 0
    for src, dst, tw, rot in CONTROLS:
        s = os.path.join(SRC, src.replace("/", os.sep))
        d = os.path.join(RES, dst.replace("/", os.sep))
        if not os.path.isfile(s):
            print("  MISSING  %s" % src)
            errors += 1
            continue
        try:
            if dst.replace("\\", "/").endswith("TrimmerDark-4-pointer.svg"):
                ow, oh, nwv, nhv = convert_pointer(s, d, 24.09, tw)
            else:
                ow, oh, nwv, nhv = convert(s, d, tw, rot)
            print("  %-34s %7.2fx%-7.2f -> %6.2fx%-6.2f%s" % (dst, ow, oh, nwv, nhv, "  (rot 90)" if rot else ""))
        except Exception as exc:  # noqa: BLE001 - report and keep going
            print("  FAILED   %s: %s" % (src, exc))
            errors += 1

    for src, dst in PANELS:
        s = os.path.join(SRC, src.replace("/", os.sep))
        d = os.path.join(RES, dst.replace("/", os.sep))
        if not os.path.isfile(s):
            print("  MISSING  %s" % src)
            errors += 1
            continue
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copyfile(s, d)
        print("  %-34s copied" % dst)

    for src, dst, vw, vh, wrap, names in LABEL_SHEETS:
        s = os.path.join(SRC, src.replace("/", os.sep))
        d = os.path.join(RES, dst.replace("/", os.sep))
        if not os.path.isfile(s):
            print("  MISSING  %s" % src)
            errors += 1
            continue
        try:
            got, glyphs = write_labels(s, d, vw, vh, wrap, names, LABEL_INVENTORY[dst])
            print("  %-34s %2d labels, %3d glyphs: %s"
                  % (dst, len(got), glyphs, " ".join(got)))
        except Exception as exc:  # noqa: BLE001 - report and keep going
            print("  FAILED   %s: %s" % (src, exc))
            errors += 1

    print("\n%s" % ("done" if not errors else "%d problem(s)" % errors))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
