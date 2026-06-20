# Claude Code 빌드 프롬프트 — **Argos** STEP Viewer (Mayo fork) + SolidWorks-style Measure & Section

> 이 문서를 **클로드 코드 첫 메시지로 그대로 붙여넣으세요.**

---

## ⚠️ v2 변경점 (이전 안 대비)

이 빌드의 최종 목표는 **사람이 쓰는 데스크톱 앱**이자 **나중에 AI(LLM)가 직접 호출하는 측정 엔진**이다.
따라서 측정·section·파싱 로직을 **GUI에서 분리**한다. 핵심 변경은 다음과 같다:

- **3-layer 강제 분리**: `argos_core` (Qt 비의존 / OCCT 전용) ← Qt GUI ← (나중에) CLI·MCP 래퍼.
  파싱·dispatch·측정 연산은 **전부 `argos_core`** 에 둔다. GUI는 클릭·표시만 한다.
- **측정 결과는 JSON 직렬화 가능한 순수 구조체**로 반환 (GUI·CLI·MCP가 동일 결과를 공유).
- **CLI 래퍼**(`argos-cli`)를 산출물에 추가 → headless 측정이 가능해야 AI가 쓸 수 있다.
- **URDF / MJCF / 휴머노이드 로봇 포맷은 out of scope** (별도 웹 트랙에서 처리). 이 앱은 STEP/IGES/STL CAD 뷰어에 집중.

---

## 0. 목표 (한 줄)

오픈소스 CAD 뷰어 **Mayo (`fougue/mayo`, Qt + OpenCASCADE, C++)** 를 fork 하여,
**Windows 네이티브 데스크톱 앱 `Argos`** 로 빌드한다. 차별점은 ① **SolidWorks Measure 도구의 UX를 그대로 재현한 3D 치수 측정**, ② **SolidWorks 스타일 단면(Section View)**, 그리고 ③ **측정·파싱 엔진을 Qt에서 분리한 `argos_core` 라이브러리**(나중에 CLI·MCP로 AI에 노출)다.

---

## 1. 전제 / 제약

- **Base**: `https://github.com/fougue/mayo` fork (라이선스 permissive — 유지할 것)
- **Stack**: C++17, Qt 6, OpenCASCADE 7.x, CMake
- **빌드 의존성(Windows)**: **vcpkg** 사용 권장 (`vcpkg install opencascade qtbase`). conan 도 가능. 미리 빌드된 OCCT/Qt6 바이너리 경로를 CMake toolchain으로 잡을 것. *OCCT+Qt6 Windows 빌드가 첫 관문이므로 작업 0에서 반드시 먼저 통과시킬 것.*
- **Target**: Windows 네이티브 `.exe` (web/Electron 아님)
- **아키텍처 원칙 (최우선)**:
  - **`argos_core`** = Qt **비의존**, **OCCT만** 링크하는 static/shared lib.
    - 포함: STEP/IGES/STL 파싱 핸들, sub-shape 측정 dispatch, 측정 계산(거리/각도/길이/면적/반경 등), section 상태 계산.
    - 반환 타입: **JSON 직렬화 가능한 POD/struct** (`nlohmann/json` 등). Qt 타입(`QString`, `QVector`…) 절대 사용 금지.
  - **Qt GUI** = `argos_core` 를 링크. 클릭·선택·렌더·패널 표시만. 측정 "계산"을 GUI 코드에 두지 말 것.
  - **`argos-cli`** (M5) = `argos_core` 를 링크한 콘솔 실행 파일. Qt 비의존.
- **UI 언어**: 한국어 라벨. Qt UI에 한글 폰트(**NotoSansKR**) 임베드/지정하여 렌더링 깨지지 않게 할 것.
- **이미 Mayo에 있는 것 → 절대 새로 만들지 말 것 (재사용·확장만)**:
  - STEP / IGES / **STL** / mesh 로딩
  - 3D viewer, assembly tree, 카메라/뷰큐브
  - **clip plane + capping** (단면의 핵심 엔진이 이미 있음)
- **우리가 실제로 추가하는 것**: ① `argos_core` 분리, ② SolidWorks 스타일 Measure 패널, ③ SolidWorks 스타일 Section 컨트롤, ④ `argos-cli`.

---

## 2. 작업 0 — 코드베이스 파악 + 빌드 (코드 수정 전 먼저)

아래를 **먼저 수행하고 결과를 보고**한 뒤 다음 단계로 갈 것:

1. Mayo를 clone 하고 **vcpkg(또는 conan)로 OCCT+Qt6 의존성을 잡아 빌드가 성공하는지** 확인. (이 단계가 막히면 여기서 멈추고 선택지를 보고할 것 — 절대 추측으로 대량 수정하지 말 것.)
2. 다음의 **기존 구현 위치(파일·클래스)** 를 찾아 요약:
   - (a) 현재 **measure 도구** 구현 (UI 패널 + 측정 계산부) — 계산부가 어디까지 UI와 분리돼 있는지 특히 확인.
   - (b) **clip plane / capping** 구현·노출부.
   - (c) **STEP/IGES/STL 로딩**이 어떤 클래스를 통하는지 (→ `argos_core`가 재사용할 진입점).
3. AIS selection(sub-shape 선택) 처리부(`AIS_InteractiveContext`) 파악.
4. **`argos_core` 추출 계획 제시**: Mayo의 어떤 기존 코드가 Qt-free로 떼어질 수 있고, 무엇을 새 lib로 감쌀지 경계선을 그어 보고할 것.

→ 이걸 토대로 "새로 짤 것 vs 기존 것 확장할 것 vs 코어로 분리할 것"을 구분해 계획을 제시할 것.

---

## 3. 작업 1 — SolidWorks-style Measure 도구 (핵심 가치)

> **계산은 전부 `argos_core`, 표시는 GUI.** 아래 dispatch는 `argos_core`의 공개 API다.

### 3-1. UX 요구사항 (SolidWorks Measure 재현)

1. **Modeless dock 패널 + pin** — 선택하는 동안 계속 떠 있고 실시간 갱신. Esc로 닫기, pin으로 고정.
2. **vertex · edge · face 동시 선택 (Ctrl 불필요)** — 도구 모드 전환 없이 클릭만으로 측정. *이게 SolidWorks 느낌의 본질이므로 최우선.*
3. **selection-set 자동 추론 (dispatch)** — 선택된 sub-shape 조합을 보고 측정 종류를 자동 결정 (아래 매트릭스).
4. **Point-to-Point 토글** — 같은 두 면을 "면-면 거리" ↔ "점-점 거리"로 전환.
5. **상단 옵션 바**: Units/Precision(단위·소수자리·dual unit), Show XYZ(dX/dY/dZ 분해), Projected On(투영 면적/거리).
6. **Measurement History 패널** — 현재 세션의 측정 이력 나열, 클릭 시 재선택/하이라이트.
7. **on-screen callout** — 그래픽 영역에 화살표·보조선·치수문자 주석 표시.

### 3-2. Selection Dispatch 매트릭스 (= 측정 엔진의 심장, `argos_core`에 위치)

| 선택 | 결과 | 비고 |
|---|---|---|
| vertex ×1 | X, Y, Z 좌표 | |
| vertex ×2 | 거리 (+ dX/dY/dZ) | |
| edge ×1 (line) | 길이 | |
| edge ×1 (arc/circle) | 반경·지름·중심좌표 | |
| face ×1 (plane) | 면적·둘레 | |
| face ×1 (cylinder) | 지름·면적 | |
| face ×2 (parallel) | 거리 | Point-to-Point OFF일 때 |
| face/edge ×2 | 각도 | collinear/parallel 제외 |
| face/edge 다중 | 합산 길이 / 총 면적 | Ctrl 없이 연속 선택 |
| circle ×2 | center-to-center / min / max | dropdown으로 선택 |

### 3-3. OCCT API 매핑 (`argos_core` 내부)

| 요소 | OCCT |
|---|---|
| sub-shape 선택 | `AIS_InteractiveContext` + `Activate(aisShape, TopAbs_VERTEX/EDGE/FACE)` 동시 활성 *(선택은 GUI, 선택된 `TopoDS_Shape`를 코어에 넘김)* |
| 점 좌표 | `BRep_Tool::Pnt(vertex)` → `gp_Pnt` |
| 거리(최소) | `BRepExtrema_DistShapeShape` |
| 길이 | `GCPnts_AbscissaPoint::Length(BRepAdaptor_Curve)` |
| 곡선 종류·원 정보 | `BRepAdaptor_Curve` → `GeomAbs_Line`/`GeomAbs_Circle` → `gp_Circ`(Radius, Location) |
| 면 종류·원통 정보 | `BRepAdaptor_Surface` → `GeomAbs_Plane`/`GeomAbs_Cylinder` → `gp_Cylinder`(Radius) |
| 면적·둘레·속성 | `BRepGProp` / `GProp_GProps` |
| 각도 | 두 `gp_Dir`(normal/curve direction) → `gp_Dir::Angle()` |
| 치수 주석(표시) | `PrsDim_LengthDimension` / `PrsDim_DiameterDimension` / `PrsDim_AngleDimension` / `PrsDim_RadiusDimension` *(주석 렌더는 GUI, 수치는 코어 결과에서)* |

> **dispatch는 UI에서 분리된, 단위테스트 가능한 순수 함수**로 `argos_core`에 작성할 것:
> ```cpp
> // argos_core/measure.h
> MeasureResult dispatch(const std::vector<TopoDS_Shape>& selected,
>                        const MeasureOptions& opt);
> std::string to_json(const MeasureResult&);   // CLI·MCP 공유용
> ```
> `MeasureResult`는 측정 종류(enum) + 값들 + (옵션) dX/dY/dZ + 좌표를 담은 POD. Qt 타입 금지.

### 3-4. 우선순위

- **P0**: 동시 선택 + dispatch, 거리(점-점/면-면)·각도·길이·면적·반경/지름·좌표, on-screen callout
- **P1**: dX/dY/dZ, Point-to-Point 토글, Measurement History, Units/Precision
- **P2**: Projected On, custom units / dual unit, history 항목 재선택

---

## 4. 작업 2 — Section View (SolidWorks 스타일)

Mayo의 **기존 clip plane + capping을 재사용**하고, 그 위에 SolidWorks 스타일 컨트롤만 얹는다.
**절단면 상태(평면 계수·offset·flip)는 `argos_core`의 `SectionState`로 보관**, GUI는 그것을 `Graphic3d_ClipPlane`에 바인딩만.

### 4-1. UX 요구사항

1. **절단면 선택**: 표준 평면(XY / YZ / ZX) 또는 선택한 planar face 기준
2. **offset 슬라이더 + 수치 입력 + flip(방향 반전)** — 평면 실시간 이동
3. **capping on/off + cap 색상** — 잘린 단면을 채워서(solid) 표시 (SolidWorks 비주얼의 핵심)
4. **(옵션) 다중 절단면** — 최대 2~3개 평면 동시, 교집합 절단
5. **(옵션) 드래그 매니퓰레이터** — 평면을 핸들로 직접 조작

### 4-2. OCCT API 매핑

| 요소 | OCCT |
|---|---|
| 절단면 | `Graphic3d_ClipPlane` (`gp_Pln` → 평면 계수) |
| 적용 범위 | `V3d_View` 전체 또는 특정 `AIS_Shape` |
| capping | `Graphic3d_ClipPlane::SetCapping(true)` + `SetCappingColor()` |
| 다중/교집합 | `Graphic3d_SequenceOfHClipPlane` + `SetChainNextPlane` |
| 드래그 | `AIS_Manipulator` 또는 커스텀 핸들 |

### 4-3. 우선순위

- **P0**: 표준평면 절단 + offset 슬라이더 + flip + capping(채움)
- **P1**: 선택 face 기준 절단, cap 색상
- **P2**: 다중 평면, 드래그 매니퓰레이터

---

## 5. 수용 기준 (Acceptance Criteria)

- [ ] Windows에서 `.exe`로 빌드·실행되고, STEP/IGES/STL을 모두 연다
- [ ] **`argos_core`가 Qt 없이 단독으로 빌드된다** (Qt 헤더/링크 의존이 0)
- [ ] **`argos-cli`로 headless 측정이 된다**: 예) `argos-cli measure part.step --vertex 12 --vertex 47` → 거리·dX/dY/dZ를 **JSON으로 stdout 출력**. (이게 AI 호출 경로의 증거)
- [ ] Measure: 도구 켠 상태에서 모드 전환 없이 vertex/edge/face를 섞어 클릭하면 매트릭스대로 올바른 값이 나온다
- [ ] Measure: 두 평행면 → 면-면 거리, Point-to-Point 켜면 점-점 거리로 바뀐다
- [ ] Measure: 측정 결과가 그래픽에 callout으로, 패널에 수치로 동시 표시된다
- [ ] Measure: Show XYZ 켜면 dX/dY/dZ가 나오고, History에 이력이 쌓인다
- [ ] Section: 표준평면으로 자르면 단면이 **capping으로 채워져** 보이고, 슬라이더로 실시간 이동·flip 된다
- [ ] 한글 UI 라벨이 깨지지 않는다 (NotoSansKR)
- [ ] `dispatch()` 측정 로직에 대한 단위테스트가 있다 (GUI 없이 `argos_core`만으로 실행)

---

## 6. 진행 순서 (Milestones)

1. **M0**: Mayo fork·vcpkg 빌드 성공 + 작업 0 파악 보고 + `argos_core` 추출 경계 제시
2. **M1**: `argos_core` 스캐폴딩(파싱 진입점 + `dispatch()` + JSON 직렬화) + 단위테스트. SolidWorks 스타일 Measure 패널 골격 + 동시 선택 연결 (P0)
3. **M2**: callout 주석 + Show XYZ + Point-to-Point + History (P1)
4. **M3**: Section View SolidWorks 컨트롤(P0~P1), `SectionState`를 코어에
5. **M4**: Projected On / 다중 평면 / 마감 (P2) + Windows 패키징
6. **M5**: **`argos-cli` 래퍼** — `argos_core` 위에 콘솔 측정 명령. (MCP 노출은 이 CLI를 감싸면 되므로 후속 작업으로 분리)

---

## 7. 산출물

- Windows 네이티브 `Argos.exe` (빌드 스크립트 포함)
- **`argos_core` 라이브러리** (Qt 비의존, 단위테스트 포함)
- **`argos-cli.exe`** (headless 측정 → JSON)
- 기능별 간단한 사용 메모 (README 갱신)
- Mayo permissive 라이선스·저작권 고지 유지

---

## 8. Out of Scope (이 앱에서 다루지 않음)

- **URDF / MJCF / SDF / glTF 등 로봇·시뮬 포맷** — 별도 웹 트랙(three.js + urdf-loader)에서 처리. Mayo에 욱여넣지 말 것.
- **MCP 서버 자체** — `argos-cli`(M5)가 완성되면 그것을 감싸는 별도 후속 작업. 단, **지금 코어를 JSON 직렬화 가능하게 만들어 두는 것**이 그 전제다.

---

### 작업 방식 지시

- 각 Milestone 끝에 **무엇을 바꿨는지 diff 요약**과 **빌드/실행 확인**을 보고할 것.
- **`argos_core`에 Qt 의존이 새어 들어가지 않게** 매 단계 확인할 것 (이 분리가 깨지면 AI 경로가 막힌다).
- 새 외부 의존성은 최소화. 가능하면 OCCT/Qt 내장 기능으로 해결. (JSON은 `nlohmann/json` 헤더온리 허용)
- 기존 Mayo 기능을 깨뜨리지 말 것 (회귀 주의).
- 불확실하면 추측해서 대량 수정하지 말고 **선택지를 제시**할 것.
