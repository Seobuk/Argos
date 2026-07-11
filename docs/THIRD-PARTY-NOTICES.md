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
| **Assimp** (메시 가져오기/내보내기 플러그인) | 6.0.5 | BSD-3-Clause | © assimp team | https://github.com/assimp/assimp |
| **FreeType** (OCCT 텍스트 렌더링 / text rendering, DLL 동봉) | — | FreeType License (FTL) | © The FreeType Project (David Turner, Robert Wilhelm, Werner Lemberg) | https://freetype.org |

> Qt · OpenCASCADE · Assimp는 동적 링크(DLL)로 사용되며, 배포 zip에 해당 런타임 DLL이 포함됩니다.
> OCCT 프리빌트 번들이 요구하는 그 밖의 런타임 DLL(FreeType 등)도 zip에 함께 들어가며, 각자의 라이선스를 따릅니다.
> Qt, OpenCASCADE and Assimp are used via dynamic linking (DLLs); the distribution zip bundles their runtime DLLs,
> plus the additional runtime DLLs required by the prebuilt OCCT bundle (e.g. FreeType), each under its own license.

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
| **Noto Sans KR** (배포 zip에 포함 / shipped in the zip) | SIL Open Font License 1.1 | © 2014-2021 Adobe (Source Han Sans 기반 / based on), Google | https://fonts.google.com/noto/specimen/Noto+Sans+KR |
| **Pretendard** (UI 디자인 목업 전용 / UI mockup only, `docs/ui-design/`) | SIL Open Font License 1.1 | © Kil Hyung-jin | https://github.com/orioncactus/pretendard |

> Noto Sans KR은 빌드 시 내려받아 zip의 `fonts/` 폴더에 담기며, OFL 전문은 `licenses/OFL-NotoSansKR.txt`
> ([doc/licenses/OFL-NotoSansKR.txt](../doc/licenses/OFL-NotoSansKR.txt))로 함께 배포됩니다. 폰트 다운로드에 실패한 빌드에는 폰트가 없을 수 있습니다.
> 한글 ViewCube 라벨 등은 Windows 기본 글꼴 **Malgun Gothic**(Microsoft, 시스템 폰트)을 사용하며 재배포하지 않습니다.
> Noto Sans KR is fetched at build time into the zip's `fonts/` folder together with the OFL text at `licenses/OFL-NotoSansKR.txt`;
> a build whose font download failed may ship without it. Korean ViewCube labels use Windows' built-in **Malgun Gothic** (system font); it is not redistributed.

## 아이콘 / Icons

| 구성요소 / Component | 라이선스 / License | 출처 / Source |
|---|---|---|
| 테마 아이콘 (Mayo 원본 / from Mayo upstream, `images/themes/`) | CC BY 3.0 | [Flaticon](https://www.flaticon.com/) — 저작자별 상세는 [images/credits.txt](../images/credits.txt) 참고 / see per-author credits |
| `palette.svg` (구성요소 개별 색상 버튼) | BSD-2-Clause (Argos 자체 제작 / original Argos artwork) | — |

---

배포 zip에는 `LICENSE.txt`, 이 고지 문서(`THIRD-PARTY-NOTICES.md`), `licenses/` 폴더(라이선스 전문 모음: `3rdparty_licenses.md`,
아이콘 크레딧 `icon-credits.txt`, `OFL-NotoSansKR.txt`)가 포함됩니다. 그 밖의 라이선스 전문은 각 프로젝트 저장소에서 확인할 수 있습니다.
오류나 누락이 있으면 알려주세요.
The distribution zip includes `LICENSE.txt`, this notice file, and a `licenses/` folder (`3rdparty_licenses.md` license texts,
`icon-credits.txt`, `OFL-NotoSansKR.txt`). Other full license texts are available in each project's repository.
Please report any errors or omissions.
