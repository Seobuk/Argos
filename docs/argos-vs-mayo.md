# Argos vs Mayo — 차이점 정리

> Argos는 [Mayo](https://github.com/fougue/mayo)(Qt 6 + OpenCASCADE 기반 STEP/IGES/STL 뷰어)를
> 포크해 만든 **Windows 네이티브 CAD 측정 뷰어**입니다. Mayo의 뷰잉 기능은 그대로 두고,
> 그 위에 **측정 / 단면 / 자동화 엔진 / 한국어 UI**를 더했습니다. 기능을 빼지 않는
> "비파괴(additive)" 원칙으로 작업합니다.

상태 표기: ✅ 구현·검증 완료 · 🟡 부분(엔진 또는 일부만) · ⏳ 예정

---

## 0. 한눈에 보기

| 구분 | Mayo | Argos |
|---|---|---|
| 성격 | 범용 CAD 뷰어 | 뷰어 + **측정 전문 도구** + 자동화 엔진 |
| 측정 | 측정 타입을 **먼저 고른 뒤** 클릭 | **클릭만 하면 종류 자동 판별**(SolidWorks식) |
| 측정 결과 | 한 줄 텍스트 | **카드 UI**(대표값 + 색깔 ΔXYZ + 보조표) |
| 엔진 | 측정/IO가 Qt에 결합 | **`argos_core` = Qt 0%**, OCCT만, JSON 반환 |
| 헤드리스 | 변환 CLI 중심 | **`argos-cli` 측정/단면 → JSON** |
| 언어 | 영어/프랑스어/중국어 | **한국어**(툴팁·ViewCube·좌표축 포함) |
| 라이선스 | BSD-2-Clause | BSD-2-Clause **유지** |

---

## 1. 아키텍처 — 3계층 분리 ✅

Mayo는 측정·IO 로직이 Qt 타입과 섞여 있어 GUI 밖에서 재사용하기 어렵습니다.
Argos는 측정/단면 엔진을 **Qt가 전혀 없는** 독립 계층으로 떼어냈습니다.

```
argos_core   (Qt 없음, OpenCASCADE만)   ← 측정/단면/로더 + JSON
   ▲                 ▲
   │                 │
Qt GUI (Argos)    argos-cli / (향후) MCP
```

- `src/argos_core/` 는 Qt를 include/link 하지 않음 — `dumpbin /dependents` 로
  `argos_core_test.exe`, `argos-cli.exe` 에 **Qt DLL 의존성 0** 확인.
- 같은 측정 로직을 GUI·CLI·(향후)MCP 가 공유 → AI/자동화가 Argos를 구동하는 통로.

| 모듈 | 위치 |
|---|---|
| 측정 디스패치 + JSON | `src/argos_core/measure.{h,cpp}` |
| 단면 상태 + 헤드리스 절단 | `src/argos_core/section.{h,cpp}` |
| STEP/IGES/BREP 로더 | `src/argos_core/io.{h,cpp}` |
| GUI 측정 패널 | `src/app/widget_measure.{h,cpp}` |
| CLI | `src/argos_cli/main.cpp` |
| GUI-free 단위테스트 | `tests/argos/test_measure.cpp` |

---

## 2. 측정 도구 — 가장 큰 차이

### 2.1 측정 방식 ✅
- **Mayo**: 콤보박스에서 측정 타입(거리/각도/지름…)을 **먼저 선택**한 뒤 형상을 클릭.
- **Argos**: 측정 타입 콤보를 숨기고, **점·모서리·면을 모드 전환 없이 섞어 클릭**하면
  선택 조합으로 **종류를 자동 판별**(`argos::dispatch`). SolidWorks/Fusion 방식.

| 선택 | 결과 |
|---|---|
| 꼭짓점 ×1 | X, Y, Z |
| 꼭짓점 ×2 | 거리 (+ ΔX/ΔY/ΔZ) |
| 모서리 ×1 (직선/원) | 길이 / 지름·반경·중심 |
| 면 ×1 (평면/원통) | 면적·둘레 / 지름·면적 |
| 면 ×2 (평행) | 거리 |
| 원 ×2 | 중심-중심 / 최소 / 최대 거리 (드롭다운 전환, SolidWorks식) |
| 모서리·면 ×2 (비평행) | 각도 |
| 다수 | 총 길이 / 총 면적 |

### 2.2 결과 표시 ✅
- **Mayo**: 고정폭 한 줄 텍스트.
- **Argos**: **카드 스택** —
  - 🟩 **선택 항목 칩**(면 1 / 모서리 2 …, 색상 구분),
  - **대표값 대형 표시**(강조색, 예: `100.00 mm²`),
  - **색깔 ΔX(빨강)/ΔY(초록)/ΔZ(파랑) 그리드**,
  - **보조표**(반경·중심·둘레·부피 등).

### 2.3 워크플로 편의 ✅
- **값 복사 / JSON 복사 / 선택 해제** 버튼, **측정 이력 + 모두 지우기**.
- 이력 항목 **더블클릭·우클릭 복사**.
- 단축키 **Esc**(선택 해제), **Ctrl+C**(값 복사), **M**(측정 토글).
- 도구를 껐다 켜도 **이력이 보존됨**(명시적 "모두 지우기" 로만 삭제).
- 결과창에서 **원시 JSON 제거** → 사람이 읽는 화면은 깔끔, JSON은 버튼/CLI로만.

### 2.4 남은 측정 항목 ⏳
- 호버 즉시측정(클릭 0번), 뷰포트 플로팅 태그(치수선+ΔXYZ),
  per-entity 3D 하이라이트 색상 동기화, 인라인 단위/정밀도 드롭다운,
  자동 관계 라벨("두 평행면").

---

## 3. 단면(Section) 도구

- **엔진** ✅ `argos_core/section.h` — 표준 평면(XY/YZ/ZX)·오프셋·플립·캡(capping),
  `computeSection()`이 단면 윤곽/둘레/경계상자를 계산, JSON 직렬화.
- **CLI** ✅ `argos-cli section <file> --plane xy --offset N ...`
- **GUI** ✅ 전용 단면 패널(`WidgetSection`) — 평면 버튼 + 오프셋 슬라이더 + 방향 반전
  + 캡 색상 + 단면 둘레/경계 표시.
- **온스크린 안내** ✅ 단면 도구를 켜면 3D 화면 하단에 사용법 안내 오버레이가
  떠서(평면 선택·오프셋·반전·캡·종료) 처음 쓰는 사람도 뷰를 벗어나지 않고 조작.

---

## 3.5 2D 도면(Drawing) 🟡

Mayo에는 없는 기능입니다. OpenCASCADE의 **은선 제거(HLR, `HLRBRep`)** 로 3D 형상을
2D 정투상 뷰로 투영해 **엔지니어링 도면**을 생성합니다.

- **엔진** ✅ `argos_core/drawing.h` — Qt 비의존. `computeDrawing()`이
  **정면·평면·우측면 3면도 + 등각도**를 만들고, 뷰마다 가시선(외곽선)·은선(점선)을
  폴리라인으로 추출, **전체 치수(가로·세로)**·도면 테두리·표제란을 배치.
- **투상법** ✅ **제1각법(ISO/한국)** / **제3각법(ASME)** 배치 규칙 지원 (`--projection`).
- **출력** ✅ **SVG**(열람용, 벡터) + **DXF R12**(AutoCAD 등 CAD 편집용). 은선은
  DASHED 레이어, 치수/텍스트는 별도 레이어.
- **CLI** ✅ `argos-cli drawing <file> -o out.svg|out.dxf [옵션]`
- **GUI** ✅ **도구 → 2D 도면 내보내기…** — 형식(SVG/DXF/둘 다)·투상법·뷰·은선·치수를
  고르는 다이얼로그에서 현재 문서를 도면으로 저장 (`CommandExportDrawing`).
- **제약**: HLR은 정확 B-Rep 기반이라 STEP/IGES/BREP에서 동작(메시-only STL 제외).
  상세 피처 치수·단면 뷰·GD&T·BOM은 아직 ⏳.

---

## 4. 헤드리스 CLI ✅

```
argos-cli measure <file> [--vertex N | --edge N | --face N]... [--point-to-point] [--circle-mode center|min|max] [--pretty]
argos-cli section <file> [--plane xy|yz|zx] [--offset N] [--flip]
argos-cli drawing <file> -o <out.svg|out.dxf> [--projection first|third] [--views front,top,right,iso] [--format svg|dxf|both]
argos-cli info    <file>
```
- 결과를 **JSON으로 stdout** 출력 → 스크립트/AI가 그대로 소비.
- 한글 등 비ASCII 경로 지원(`wmain` + UTF-8).

---

## 5. UI / 한국어화

- ✅ ViewCube 한글 면 라벨(윗면/정면/우측/아랫면/후면/좌측, Malgun Gothic).
- ✅ 원점 좌표축 X/Y/Z 라벨(빨강/초록/파랑).
- ✅ 뷰 버튼 **한글 툴팁**(마우스오버 기능 설명) + **M** 단축키.
- ✅ 측정 패널 전체 한국어.
- ⏳ 앱 명칭 **Mayo → Argos** 리브랜딩 및 **눈 모양 아이콘**(작업 중).
- ⏳ 메뉴/대화상자 전체 한국어 번역(.qm) 적용.

---

## 6. 빌드 / 패키징

- ✅ `vcpkg.json` 으로 **OpenCASCADE 7.9.0 고정**(Mayo HEAD가 OCCT 8.x 미지원).
- ✅ 헬퍼 스크립트: `scripts/argos-build.ps1`, `argos-package.ps1`, `argos-fetch-font.ps1`.
- ✅ 패키징 시 `mayo.exe` → **`Argos.exe`** 로 배포, `argos-cli.exe` 동봉.
- ✅ GUI-free 단위테스트(`tests/argos/`, 47개 체크).

---

## 7. 라이선스

- Mayo의 **BSD-2-Clause** 그대로 유지(`LICENSE.txt`). 포크 출처 명시.

---

## 8. 코드에서 확인할 위치 (요약)

| 변경 | 파일 |
|---|---|
| Qt-free 측정/단면/IO 엔진 | `src/argos_core/*` |
| 측정 패널(자동판별·카드·칩·복사·이력) | `src/app/widget_measure.{h,cpp}` |
| 좌표축·ViewCube 한글 | `src/gui/gui_document.cpp` |
| 한글 툴팁·M 단축키 | `src/app/widget_gui_document.cpp` |
| CLI | `src/argos_cli/main.cpp` |
| OCCT 버전 고정 | `vcpkg.json` |
| 목표 UI 디자인 | `docs/ui-design/` |
| 제품 스펙 | `docs/argos-spec.md` |
