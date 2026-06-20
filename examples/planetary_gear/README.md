# 예시: 유성기어 구동모듈 측정 보고서 (Planetary‑gear drive report)

Argos의 **STEP 자동 측정 보고서** 기능 예시입니다. 실제 **2단 유성기어 감속기**
(JVL **HSHG17** — NEMA 17 스텝모터용)를 받아 `argos-cli` + `argos_report.py`로
파워포인트 보고서를 자동 생성했습니다.

> 휴머노이드 개발 관점: 보고서에는 **질량·무게중심·관성텐서(COM 기준)**와
> 바로 붙여넣을 수 있는 **URDF `<inertial>`** 블록이 포함됩니다.

## 측정 요약 (강철 7850 kg/m³ 가정)

| 항목 | 값 |
|---|---|
| 전체 치수 (L×W×H) | 49.2 × 49.2 × 93.0 mm |
| 부피 / 질량 | 103.5 cm³ / **0.812 kg** |
| 표면적 | 20,554 mm² |
| 솔리드 / 면 / 모서리 | 1 / 198 / 483 |
| 주요 지름 | ⌀58, 54, 42, 28, 22, 18, 10, 8, 6.8, 6, 5, 3.2 mm |

## 폴더 내용

- `report/HSHG17_2stage_planetary_gear_report.pptx` — 보고서(슬라이드 6장, **3D 이미지 포함**)
- `report/view_iso.png` · `view_front.png` · `view_top.png` · `view_right.png` —
  Argos가 **헤드리스(offscreen)로 렌더링**한 3D 뷰. 어디를 측정했는지 한눈에 보이며,
  창이 뜨지 않아 작업 중에도 생성됩니다.
- `report/digest.json` — 측정 원본 데이터(JSON)
- `report/inertial.urdf.xml` — URDF `<inertial>` 스니펫
- 원본 STEP는 **저작권상 저장소에 포함하지 않습니다**(`.gitignore`). 아래로 받으세요.

## 재생성 방법

```powershell
# 0) 의존성: python-pptx + Pillow,  그리고 mayo-conv.exe(오프스크린 렌더러) 빌드 필요
#    pip install python-pptx pillow   (mayo-conv는 argos-build.ps1이 함께 빌드)

# 1) 원본 STEP 다운로드 (JVL, 무료 CAD)
#    https://www.jvl.dk/files/Downloads-1/CAD%20drawings/step/HSHG17NxxxMHN17008M_2stage.STEP
#    -> examples/planetary_gear/HSHG17_2stage_planetary_gear.step 로 저장

# 2) 보고서 생성 (이미지 없이 빠르게: --no-images)
py -3.12 scripts/argos_report.py `
    examples/planetary_gear/HSHG17_2stage_planetary_gear.step `
    --material "강철(steel)" --out examples/planetary_gear/report

# 알루미늄으로 다시: --density 2700 --material "알루미늄"
```

출처: JVL Industri Elektronik — Planetary gears CAD (https://www.jvl.dk/723/cad-files-planetary-gears)
