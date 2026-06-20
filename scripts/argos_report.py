#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Argos STEP measurement report generator.

Given a STEP/IGES/BREP file, this creates an output folder and a PowerPoint
report (.pptx) with the key measured values: overall dimensions, mass / inertia
(for robot-link dynamics), notable cylinder diameters, and a ready-to-paste
URDF <inertial> block. It drives the Qt-free `argos-cli` (digest / props).

Usage:
  py -3.12 scripts/argos_report.py <file.step> [--density 7850] [--out DIR] [--cli PATH]

Requires: python-pptx  (pip install python-pptx)
"""
import argparse
import json
import os
import subprocess
import sys

KO_FONT = "Malgun Gothic"  # Korean-capable font for PowerPoint rendering


def find_cli(explicit):
    if explicit and os.path.isfile(explicit):
        return os.path.abspath(explicit)
    here = os.path.dirname(os.path.abspath(__file__))
    for c in (
        os.path.join(here, "..", "build", "Release", "argos-cli.exe"),
        os.path.join(here, "..", "dist", "Argos", "argos-cli.exe"),
        "argos-cli.exe",
    ):
        if os.path.isfile(c):
            return os.path.abspath(c)
    return "argos-cli.exe"


def run_cli(cli, args):
    try:
        r = subprocess.run([cli, *args], capture_output=True, text=True,
                           encoding="utf-8", timeout=600)
        return r.stdout or "", r.returncode
    except Exception as e:
        return f"(argos-cli error: {e})", 1


def fnum(v, n=2):
    try:
        return f"{float(v):,.{n}f}"
    except Exception:
        return str(v)


def fsci(v, n=4):
    try:
        return f"{float(v):.{n}e}"
    except Exception:
        return str(v)


def main():
    ap = argparse.ArgumentParser(description="Argos STEP -> PPTX measurement report")
    ap.add_argument("step", help="input CAD file (STEP/IGES/BREP)")
    ap.add_argument("--density", type=float, default=7850.0, help="kg/m^3 (default steel)")
    ap.add_argument("--material", default="", help="material name shown in the report")
    ap.add_argument("--out", default=None, help="output folder (default: <file>_Argos_report)")
    ap.add_argument("--cli", default=None, help="path to argos-cli.exe")
    a = ap.parse_args()

    step = os.path.abspath(a.step)
    if not os.path.isfile(step):
        print("[argos_report] file not found:", step)
        sys.exit(1)

    cli = find_cli(a.cli)
    stem = os.path.splitext(os.path.basename(step))[0]
    out_dir = a.out or os.path.join(os.path.dirname(step), stem + "_Argos_report")
    os.makedirs(out_dir, exist_ok=True)

    print(f"[argos_report] cli   = {cli}")
    print(f"[argos_report] file  = {step}")
    print(f"[argos_report] out   = {out_dir}")

    dg_txt, rc = run_cli(cli, ["digest", step, "--density", str(a.density), "--pretty"])
    try:
        dg = json.loads(dg_txt)
    except Exception as e:
        print("[argos_report] failed to parse digest:", e)
        print(dg_txt[:400])
        sys.exit(1)
    if not dg.get("ok"):
        print("[argos_report] digest failed:", dg.get("error"))
        sys.exit(1)

    urdf_txt, _ = run_cli(cli, ["props", step, "--density", str(a.density), "--urdf"])

    with open(os.path.join(out_dir, "digest.json"), "w", encoding="utf-8") as f:
        f.write(dg_txt)
    with open(os.path.join(out_dir, "inertial.urdf.xml"), "w", encoding="utf-8") as f:
        f.write(urdf_txt)

    pptx_path = os.path.join(out_dir, stem + "_report.pptx")
    build_pptx(dg, urdf_txt, step, a.density, a.material, pptx_path)
    print(f"[argos_report] PPTX  = {pptx_path}")
    print("[argos_report] done.")


def build_pptx(dg, urdf, step, density, material, path):
    from pptx import Presentation
    from pptx.util import Inches, Pt, Emu
    from pptx.dml.color import RGBColor
    from pptx.enum.text import PP_ALIGN, MSO_ANCHOR

    ACCENT = RGBColor(0x1F, 0x6F, 0xC2)
    INK = RGBColor(0x22, 0x24, 0x26)
    DIM = RGBColor(0x6A, 0x6E, 0x73)
    WHITE = RGBColor(0xFF, 0xFF, 0xFF)

    prs = Presentation()
    prs.slide_width = Inches(13.333)
    prs.slide_height = Inches(7.5)
    blank = prs.slide_layouts[6]
    SW, SH = prs.slide_width, prs.slide_height

    def textbox(slide, l, t, w, h, text, size=14, bold=False, color=INK,
                align=PP_ALIGN.LEFT, font=KO_FONT, mono=False):
        tb = slide.shapes.add_textbox(l, t, w, h)
        tf = tb.text_frame
        tf.word_wrap = True
        lines = text.split("\n")
        for i, ln in enumerate(lines):
            p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
            p.alignment = align
            r = p.add_run()
            r.text = ln
            r.font.size = Pt(size)
            r.font.bold = bold
            r.font.color.rgb = color
            r.font.name = "Consolas" if mono else font
        return tb

    def title_bar(slide, title, subtitle=""):
        bar = slide.shapes.add_shape(1, 0, 0, SW, Inches(1.05))
        bar.fill.solid(); bar.fill.fore_color.rgb = ACCENT
        bar.line.fill.background()
        tf = bar.text_frame
        tf.margin_left = Inches(0.4); tf.margin_top = Inches(0.12)
        p = tf.paragraphs[0]
        r = p.add_run(); r.text = title
        r.font.size = Pt(26); r.font.bold = True; r.font.color.rgb = WHITE; r.font.name = KO_FONT
        if subtitle:
            p2 = tf.add_paragraph()
            r2 = p2.add_run(); r2.text = subtitle
            r2.font.size = Pt(12); r2.font.color.rgb = RGBColor(0xDD, 0xEA, 0xF8); r2.font.name = KO_FONT

    def add_table(slide, rows, l, t, w, col_fracs, header=True, fontsize=13):
        nrows, ncols = len(rows), len(rows[0])
        gt = slide.shapes.add_table(nrows, ncols, l, t, w, Inches(0.4 * nrows)).table
        for ci, frac in enumerate(col_fracs):
            gt.columns[ci].width = Emu(int(w * frac))
        for ri, row in enumerate(rows):
            for ci, val in enumerate(row):
                cell = gt.cell(ri, ci)
                cell.vertical_anchor = MSO_ANCHOR.MIDDLE
                cell.margin_left = Inches(0.08); cell.margin_top = Inches(0.02)
                cell.margin_bottom = Inches(0.02)
                p = cell.text_frame.paragraphs[0]
                run = p.add_run(); run.text = str(val)
                run.font.size = Pt(fontsize); run.font.name = KO_FONT
                if header and ri == 0:
                    run.font.bold = True; run.font.color.rgb = WHITE
                    cell.fill.solid(); cell.fill.fore_color.rgb = ACCENT
                else:
                    run.font.color.rgb = INK
                    cell.fill.solid()
                    cell.fill.fore_color.rgb = RGBColor(0xF4, 0xF6, 0xF8) if (ri % 2) else WHITE
        return gt

    bb = dg.get("bboxSize") or {}
    counts = dg.get("counts") or {}
    mass = dg.get("mass") or {}
    diam = dg.get("cylinderDiametersMm") or []
    vol_mm3 = mass.get("volume_mm3")
    fname = os.path.basename(step)
    mat_label = material or ("강철(steel)" if abs(density - 7850.0) < 1 else f"{density:g} kg/m³")

    # ---- Slide 1: title ----
    s = prs.slides.add_slide(blank)
    bg = s.shapes.add_shape(1, 0, 0, SW, SH)
    bg.fill.solid(); bg.fill.fore_color.rgb = RGBColor(0x15, 0x31, 0x52); bg.line.fill.background()
    textbox(s, Inches(0.8), Inches(2.4), Inches(11.7), Inches(1.2),
            "Argos 측정 보고서", size=40, bold=True, color=WHITE)
    textbox(s, Inches(0.85), Inches(3.7), Inches(11.7), Inches(2.0),
            f"파일: {fname}\n재료/밀도: {mat_label}\n생성: Argos (argos-cli digest/props)",
            size=16, color=RGBColor(0xCF, 0xDD, 0xEE))

    # ---- Slide 2: overview / dimensions ----
    s = prs.slides.add_slide(blank)
    title_bar(s, "개요 · 주요 치수", fname)
    rows = [["항목", "값"]]
    if bb:
        rows.append(["전체 치수 (L×W×H)",
                     f"{fnum(bb.get('x'))} × {fnum(bb.get('y'))} × {fnum(bb.get('z'))} mm"])
    if vol_mm3 is not None:
        rows.append(["부피", f"{fnum(vol_mm3)} mm³  ({fnum(float(vol_mm3)/1000.0)} cm³)"])
    if mass.get("area_mm2") is not None:
        rows.append(["표면적", f"{fnum(mass.get('area_mm2'))} mm²"])
    rows.append(["솔리드 / 면 / 모서리",
                 f"{counts.get('solids','-')} / {counts.get('faces','-')} / {counts.get('edges','-')}"])
    add_table(s, rows, Inches(0.7), Inches(1.4), Inches(11.9), [0.42, 0.58])

    # ---- Slide 3: mass / inertia ----
    s = prs.slides.add_slide(blank)
    title_bar(s, "질량 특성 (휴머노이드 링크 동역학)", f"{fname}  ·  {mat_label}")
    if mass.get("ok"):
        com = mass.get("com_mm") or {}
        it = mass.get("inertia_com_kg_m2") or {}
        pm = mass.get("principal_moments_kg_m2") or []
        m_rows = [["항목", "값"],
                  ["질량", f"{fnum(mass.get('mass_kg'), 4)} kg  ({fnum(float(mass.get('mass_kg',0))*1000.0)} g)"],
                  ["무게중심 COM (mm)",
                   f"({fnum(com.get('x'),3)}, {fnum(com.get('y'),3)}, {fnum(com.get('z'),3)})"],
                  ["주관성모멘트 (kg·m²)",
                   f"{fsci(pm[0]) if len(pm)>0 else '-'},  {fsci(pm[1]) if len(pm)>1 else '-'},  {fsci(pm[2]) if len(pm)>2 else '-'}"]]
        add_table(s, m_rows, Inches(0.7), Inches(1.35), Inches(11.9), [0.34, 0.66])
        textbox(s, Inches(0.7), Inches(3.55), Inches(6), Inches(0.4),
                "관성텐서 (COM 기준, kg·m²)", size=14, bold=True, color=ACCENT)
        I = [["", "x", "y", "z"],
             ["x", fsci(it.get("ixx")), fsci(it.get("ixy")), fsci(it.get("ixz"))],
             ["y", fsci(it.get("ixy")), fsci(it.get("iyy")), fsci(it.get("iyz"))],
             ["z", fsci(it.get("ixz")), fsci(it.get("iyz")), fsci(it.get("izz"))]]
        add_table(s, I, Inches(0.7), Inches(4.0), Inches(8.2), [0.16, 0.28, 0.28, 0.28], fontsize=12)
    else:
        textbox(s, Inches(0.7), Inches(1.6), Inches(11), Inches(1),
                "질량 특성을 계산할 수 없습니다 (솔리드가 아님): " + str(mass.get("error", "")),
                size=16, color=DIM)

    # ---- Slide 4: diameters ----
    s = prs.slides.add_slide(blank)
    title_bar(s, "주요 지름 (원통면)", fname)
    if diam:
        rows = [["#", "지름 (mm)", "반경 (mm)"]]
        for i, d in enumerate(diam, 1):
            rows.append([i, fnum(d, 3), fnum(float(d) / 2.0, 3)])
        add_table(s, rows, Inches(0.7), Inches(1.4), Inches(8.0), [0.16, 0.42, 0.42])
    else:
        textbox(s, Inches(0.7), Inches(1.6), Inches(11), Inches(1),
                "원통면이 감지되지 않았습니다.", size=16, color=DIM)

    # ---- Slide 5: URDF ----
    s = prs.slides.add_slide(blank)
    title_bar(s, "URDF <inertial> (로봇 링크)", fname)
    textbox(s, Inches(0.6), Inches(1.35), Inches(12.1), Inches(5.5), urdf.strip(),
            size=14, mono=True, color=INK)

    prs.save(path)


if __name__ == "__main__":
    main()
