#!/usr/bin/env python3
# 本文件从 Noto Serif CJK SC 裁出 UI 字体子集，支持各字重共用运行时字符表。
"""需要 fonttools；参数依次为官方原字体、src 目录与 assets/runtime/common 目录。"""
import argparse
from pathlib import Path
from fontTools import subset
from fontTools.ttLib import TTFont


def main() -> None:
    """扫描源码中 CJK 字符并输出 OTF 子集；增加中文 UI 后须重新运行，保留原字体许可证。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--filename", default="ui_font.otf", help="输出字体文件名")
    parser.add_argument("--family", default="KoumakyouUISerif", help="修改后字体的独立家族名")
    parser.add_argument("--charset", type=Path, help="复用已有字符表，保证不同字重覆盖相同字符")
    args = parser.parse_args()
    if args.charset:
        text = args.charset.read_text()
    else:
        characters = set(chr(code) for code in range(32, 127))
        characters.update("·～「」：，。！？（）★…←、 / —")
        for path in args.source.rglob("*"):
            if path.suffix in {".cpp", ".hpp"}:
                characters.update(char for char in path.read_text() if '\u4e00' <= char <= '\u9fff')
        text = "".join(sorted(characters))
    font = TTFont(args.font)
    options = subset.Options()
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]
    crop = subset.Subsetter(options=options)
    crop.populate(text=text)
    crop.subset(font)
    # 子集使用新家族名，避免将经过修改的字体冒充原字体。
    for record in font['name'].names:
        if record.nameID in {1, 3, 4, 6}:
            record.string = args.family.encode(record.getEncoding())
    args.output.mkdir(parents=True, exist_ok=True)
    font.save(args.output / args.filename)
    (args.output / "ui_charset.txt").write_text(text)


if __name__ == "__main__":
    main()
