"""Build the subtitle fallback fonts in assets/subtitle-fonts/.

JellyFin [PS] for PS4 - Copyright (C) 2026 jw-notes <https://github.com/jw-notes>
GPL-3.0-or-later, with the attribution terms in NOTICE. The fonts it makes
keep their own licence (SIL OFL 1.1, Apache 2.0 for Open Sans).

The console's libass (0.14, no fontconfig) only takes a glyph the chosen face
lacks from a font whose *family name* matches the face and that has the glyph
(ass_fontselect.c, select_font). So each fallback here carries, besides its own
name, family-name records for the subtitle faces it backs up ("Open Sans",
"Noto Sans", "Roboto Condensed"), in other language slots of the name table,
which libass reads as well.

libass scores fonts by weight only as bold / not bold, so a tie cannot be
steered. Instead every fallback holds only characters that neither the faces it
backs up nor the other fallbacks have: there is never a choice to get wrong.
(One exception: the Arabic cut keeps the invisible U+FEFF, see below.)

  JFSub-Extra.ttf   Open Sans Bold's Greek, Cyrillic and punctuation, for
                    Noto Sans and Roboto Condensed (subset to Latin)
  JFSub-CJK.otf     Noto Sans SC Bold: Han (GB2312, Big5 level 1, JIS X 0208
                    level 1), kana, CJK punctuation, full-width forms
  JFSub-Hangul.otf  Noto Sans KR Bold: all 11,172 modern syllables, jamo
  JFSub-Hebrew.ttf  Noto Sans Hebrew Bold
  JFSub-Arabic.ttf  Noto Sans Arabic Bold, with the presentation forms that
                    FriBidi shapes Arabic and Persian into (libass here has no
                    HarfBuzz, so the forms must be in the font's cmap)

All Noto fonts are SIL OFL 1.1 with no Reserved Font Name; Open Sans is
Apache 2.0.

Usage (needs fontTools):
  python tools/make_subtitle_fallbacks.py <folder with the Noto sources>
Sources: notofonts/noto-cjk Sans/SubsetOTF/{SC,KR}/NotoSans{SC,KR}-Bold.otf,
notofonts.github.io fonts/NotoSans{Hebrew,Arabic}/hinted/ttf/*-Bold.ttf
"""
import io
import re
import sys
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parent.parent
GFX = ROOT / "vendor/ps3/source/gfx"
OUT = ROOT / "assets/subtitle-fonts"


def font_from_header(name):
    text = (GFX / name).read_text(encoding="latin-1")
    body = text[text.index("{") + 1:text.index("}")]
    return TTFont(io.BytesIO(bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{1,2})", body))))


def codec_range(codec, lead, trail):
    out = set()
    for a in lead:
        for b in trail:
            try:
                ch = bytes([a, b]).decode(codec)
            except UnicodeDecodeError:
                continue
            if len(ch) == 1:
                out.add(ord(ch))
    return out


def han():
    gb2312 = codec_range("gb2312", range(0xB0, 0xF8), range(0xA1, 0xFF))
    big5_l1 = {c for c in codec_range("big5", range(0xA4, 0xC7), list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF)))}
    jis_l1 = codec_range("euc_jp", range(0xB0, 0xD0), range(0xA1, 0xFF))
    return {c for c in gb2312 | big5_l1 | jis_l1 if 0x4E00 <= c <= 0x9FFF}


def ranges(*pairs):
    return {c for lo, hi in pairs for c in range(lo, hi + 1)}


def build(src, out_name, wanted, exclude, own_family, aliases, force=()):
    font = TTFont(src)
    cmap = font.getBestCmap()
    keep = sorted({c for c in wanted if c in cmap and c not in exclude} | {c for c in force if c in cmap})
    opts = subset.Options()
    opts.layout_features = []
    opts.hinting = False
    opts.drop_tables += ["GSUB", "GPOS", "GDEF", "DSIG", "LTSH", "VDMX", "hdmx", "kern", "vhea", "vmtx", "VORG", "BASE", "JSTF", "MATH", "STAT"]
    opts.name_IDs = [0, 13, 14]  # copyright and licence; the names are set below
    opts.notdef_outline = True
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=keep)
    sub.subset(font)
    name = font["name"]
    full = own_family + " Bold"
    ps = own_family.replace(" ", "") + "-Bold"
    for nid, value in ((1, own_family), (2, "Bold"), (4, full), (6, ps)):
        name.setName(value, nid, 3, 1, 0x409)
    # The faces this one backs up, as further family names (other English locales).
    for lang, alias in zip((0x809, 0xC09, 0x1009, 0x1409), aliases):
        name.setName(alias, 1, 3, 1, lang)
    path = OUT / out_name
    font.save(path)
    return path, len(keep)


def main():
    src = Path(sys.argv[1])
    OUT.mkdir(parents=True, exist_ok=True)
    faces = {n: font_from_header(h).getBestCmap() for n, h in (
        ("Open Sans", "opensans_bold.h"), ("Noto Sans", "notosans_bold.h"), ("Roboto Condensed", "robotocondensed_bold.h"))}
    all_faces = set(faces["Open Sans"]) | set(faces["Noto Sans"]) | set(faces["Roboto Condensed"])
    latin_only = set(faces["Noto Sans"]) | set(faces["Roboto Condensed"])
    three = ["Open Sans", "Noto Sans", "Roboto Condensed"]

    # Open Sans Bold, from the same embedded face, as a file for the Extra cut.
    opensans = OUT / "_opensans.ttf"
    font_from_header("opensans_bold.h").save(opensans)
    jobs = [
        (opensans, "JFSub-Extra.ttf", set(faces["Open Sans"]) - set(range(0x20)), latin_only,
         "JF Subtitle Extra", ["Noto Sans", "Roboto Condensed"]),
        (src / "NotoSansSC-Bold.otf", "JFSub-CJK.otf",
         han() | ranges((0x3000, 0x303F), (0x3040, 0x30FF), (0x31F0, 0x31FF), (0xFF01, 0xFF60), (0xFFE0, 0xFFE6),
                        (0x2015, 0x2015), (0x2025, 0x2026), (0x2E80, 0x2EFF)),
         all_faces, "JF Subtitle CJK", three),
        (src / "NotoSansKR-Bold.otf", "JFSub-Hangul.otf",
         ranges((0xAC00, 0xD7A3), (0x3131, 0x318E), (0x1100, 0x11FF)),
         all_faces, "JF Subtitle Hangul", three),
        (src / "NotoSansHebrew-Bold.ttf", "JFSub-Hebrew.ttf", ranges((0x0590, 0x05FF), (0xFB1D, 0xFB4F)),
         all_faces, "JF Subtitle Hebrew", three),
        (src / "NotoSansArabic-Bold.ttf", "JFSub-Arabic.ttf",
         ranges((0x0600, 0x06FF), (0x0750, 0x077F), (0x08A0, 0x08FF), (0xFB50, 0xFDFF), (0xFE70, 0xFEFF)),
         all_faces, "JF Subtitle Arabic", three,
         # FriBidi turns lam + alef into one ligature and leaves U+FEFF in the
         # alef's place; libass looks that up in the Arabic face only, so it
         # must be here (zero width) although Open Sans has it too.
         {0xFEFF}),
    ]
    taken = set()
    for src_path, out_name, wanted, exclude, family, aliases, *force in jobs:
        path, n = build(src_path, out_name, wanted - taken, exclude, family, aliases, *force)
        taken |= {c for c in TTFont(path).getBestCmap()}
        print(f"{out_name:20s} {n:6d} characters {path.stat().st_size / 1024:8.1f} KB  aliases: {', '.join(aliases)}")
    opensans.unlink()


if __name__ == "__main__":
    main()
