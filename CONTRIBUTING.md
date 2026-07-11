# 기여 안내 / Contributing to Argos

Argos에 관심 가져주셔서 감사합니다! 버그 리포트, 기능 제안, 코드 기여 모두 환영합니다.
Thanks for your interest in Argos! Bug reports, feature requests and code contributions are all welcome.

## 버그 리포트 / 기능 제안

- [이슈](https://github.com/Seobuk/Argos/issues)로 등록해 주세요. 템플릿이 자동으로 열립니다.
- 버그는 **재현 절차**, **사용한 파일 형식(STEP/IGES/STL)**, **Argos 버전**(정보 대화상자 참고)을 함께 적어주시면 빨리 해결됩니다.
- 모델 파일을 첨부하기 어려우면 비슷하게 재현되는 공개 샘플이나 스크린샷도 좋습니다.

## 빌드 방법 / Building

Windows(MSVC) 기준:

1. **Qt 6.8.x**, **OpenCASCADE 7.9.0**, (선택) **Assimp**를 준비합니다.
2. CMake 구성 예시는 [`.github/workflows/release_windows.yml`](.github/workflows/release_windows.yml)의
   `Build (Release)` 단계가 가장 정확한 레퍼런스입니다.
3. 로컬 빌드/패키징 스크립트: [`scripts/argos-build.ps1`](scripts/argos-build.ps1) → [`scripts/argos-package.ps1`](scripts/argos-package.ps1)
4. 의존성을 vcpkg로 받으려면 저장소 루트의 [`vcpkg.json`](vcpkg.json)을 사용하세요.

## 코드 기여 / Pull Requests

- `main`에서 브랜치를 따서 작업한 뒤 PR을 올려주세요.
- **코딩 스타일은 주변 코드(Mayo 스타일)를 따릅니다** — 4칸 들여쓰기, 멤버 변수 `m_` 접두사,
  OCCT 핸들은 `OccHandle<T>`, Qt 시그널 연결은 람다 + 명시적 수신자.
- 새 소스 파일에는 기존 파일과 같은 SPDX 라이선스 헤더(`SPDX-License-Identifier: BSD-2-Clause`)를 넣어주세요.
- 측정/단면 엔진 로직은 가능하면 Qt 비의존 계층(`src/argos_core/`)에 두고, GUI는 그 위에 얹어주세요.
- UI 문자열은 한국어를 기본으로 하되 도구 설명(tooltip)에 영어 병기를 권장합니다.

## 라이선스 동의 / License of contributions

Argos는 [BSD-2-Clause](LICENSE.txt)로 배포됩니다. PR을 제출하시면 **기여 코드가 동일한
BSD-2-Clause 라이선스로 배포되는 것에 동의**하는 것으로 간주합니다 (inbound = outbound).
By submitting a pull request you agree that your contribution is licensed under the
project's BSD-2-Clause license (inbound = outbound).
