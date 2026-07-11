# 보안 정책 / Security Policy

## 지원 버전 / Supported versions

최신 릴리스([Releases](https://github.com/Seobuk/Argos/releases) 최신 버전)만 보안 수정 대상입니다.
Only the latest release receives security fixes.

## 취약점 신고 / Reporting a vulnerability

보안 취약점(예: 조작된 STEP/IGES/STL/DXF 파일 파싱으로 인한 메모리 손상 등)은
**공개 이슈로 올리지 마시고** GitHub의 비공개 취약점 신고 기능을 사용해 주세요:

**https://github.com/Seobuk/Argos/security/advisories/new**

Please do NOT open a public issue for security vulnerabilities (e.g. memory corruption
while parsing crafted CAD files). Use GitHub's private vulnerability reporting at the
link above.

신고 시 다음 정보를 포함해 주세요 / Please include:

- 재현 절차와(가능하면) 재현용 파일 / reproduction steps and, if possible, a sample file
- 영향(크래시, 메모리 손상, 정보 노출 등) / impact (crash, memory corruption, info leak, ...)
- 사용한 Argos 버전과 OS / Argos version and OS

신고는 확인되는 대로 회신하며, 수정은 다음 릴리스에 포함됩니다.
Reports are acknowledged as soon as reviewed; fixes ship with the next release.
