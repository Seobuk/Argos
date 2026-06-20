# 오픈소스 고지 / Third-Party Notices

Argos는 다음 오픈소스 소프트웨어와 자산을 사용합니다. 각 구성요소는 해당 라이선스를 따릅니다.
Argos uses the open-source software and assets below; each is used under its own license.

Argos 자체는 **BSD-2-Clause** 라이선스입니다([LICENSE.txt](../LICENSE.txt)). Argos는 [fougue/mayo](https://github.com/fougue/mayo)의 포크입니다.
Argos itself is licensed under **BSD-2-Clause** ([LICENSE.txt](../LICENSE.txt)) and is a fork of [fougue/mayo](https://github.com/fougue/mayo).

---

## 기반 / Foundation

| 구성요소 / Component | 버전 / Version | 라이선스 / License | 저작권 / Copyright | 링크 / Link |
|---|---|---|---|---|
| **Mayo** (포크 원본 / fork base) | — | BSD-2-Clause | © 2016 Fougue SAS | https://github.com/fougue/mayo |
| **Qt** (GUI 프레임워크) | 6.8.3 | LGPL-3.0 | © The Qt Company | https://www.qt.io |
| **Open CASCADE Technology** (지오메트리 커널) | 7.9.0 | LGPL-2.1 + OCCT exception | © OPEN CASCADE SAS | https://dev.opencascade.org |

> Qt와 OpenCASCADE는 동적 링크(DLL)로 사용되며, 배포 zip에 해당 런타임 DLL이 포함됩니다.
> Qt and OpenCASCADE are used via dynamic linking (DLLs); the distribution zip bundles their runtime DLLs.

## 번들 라이브러리 / Vendored libraries (`src/3rdparty/`)

| 구성요소 / Component | 버전 / Version | 라이선스 / License | 저작권 / Copyright | 링크 / Link |
|---|---|---|---|---|
| **nlohmann/json** | 3.11.3 | MIT | © 2013–2023 Niels Lohmann | https://github.com/nlohmann/json |
| **fmt** | — | MIT | © Victor Zverovich & contributors | https://github.com/fmtlib/fmt |
| **Microsoft GSL** | — | MIT | © Microsoft Corporation | https://github.com/microsoft/GSL |
| **magic_enum** | — | MIT | © Daniil Goncharov | https://github.com/Neargye/magic_enum |
| **KDBindings** | — | MIT | © Klarälvdalens Datakonsult AB (KDAB) | https://github.com/KDAB/KDBindings |
| **miniply** | — | MIT | © Vilya Harvey | https://github.com/vilya/miniply |
| **fast_float** | — | Apache-2.0 / MIT / BSL-1.0 | © Daniel Lemire & contributors | https://github.com/fastfloat/fast_float |

> `nlohmann/json`은 Argos가 추가한 헤더 전용 라이브러리이며, 나머지는 Mayo가 함께 제공합니다.
> `nlohmann/json` was added by Argos (header-only); the rest are vendored by Mayo upstream.

## 번들 폰트 / Bundled fonts

| 구성요소 / Component | 라이선스 / License | 저작권 / Copyright | 링크 / Link |
|---|---|---|---|
| **Noto Sans KR** (배포 zip에 포함 / shipped in the zip) | SIL Open Font License 1.1 | © Google LLC | https://fonts.google.com/noto/specimen/Noto+Sans+KR |
| **Pretendard** (UI 디자인 목업 전용 / UI mockup only, `docs/ui-design/`) | SIL Open Font License 1.1 | © Kil Hyung-jin | https://github.com/orioncactus/pretendard |

> 한글 ViewCube 라벨 등은 Windows 기본 글꼴 **Malgun Gothic**(Microsoft, 시스템 폰트)을 사용하며 재배포하지 않습니다.
> Korean ViewCube labels use Windows' built-in **Malgun Gothic** (Microsoft, system font); it is not redistributed.

---

라이선스 전문은 각 프로젝트 저장소에서 확인할 수 있습니다. 오류나 누락이 있으면 알려주세요.
Full license texts are available in each project's repository. Please report any errors or omissions.
