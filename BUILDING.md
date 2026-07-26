# Building eMule on Windows

This branch is based on `irwir/eMule` branch `v0.72a` (community release
`eMule_v0.72a-community`). The upstream source tree references third-party
projects which are not included in that repository.

## Tested toolchains

- Visual Studio Build Tools 2026 with the v145 C++ toolset, MFC/ATL and the
  Windows 10.0.26100 SDK.
- Visual Studio 2019 v142 C++ toolset for Crypto++ and id3lib. Those older
  dependency projects do not build unmodified with v145.
- CMake supplied by Visual Studio 2026.
- Release x64 builds use static MFC and the static MSVC runtime (`/MT`).

## Dependency revisions

Place the dependency source directories next to `srchybrid`:

| Directory | Repository/release | Revision |
| --- | --- | --- |
| `cryptopp` | `https://github.com/irwir/cryptopp.git` | `60f81a77e0c9a0e7ffc1ca1bc438ddfa2e43b78e` |
| `id3lib` | `https://github.com/irwir/id3lib.git` | `48c49ba1c211e8428759d1e7939b495859a74eea` |
| `ResizableLib` | `https://github.com/irwir/resizablelib.git` | `d55400579639d9a0172f238b40a8a1ca95b99e1c` |
| `miniupnpc` | `https://github.com/irwir/miniupnp.git` | `b719e529a2a1d1c39aa78375e8ba2e92afe1c8b5` |
| `zlib` | `https://github.com/irwir/zlib.git` | `51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf` |
| `mbedtls` | Mbed TLS 4.0.0 release archive | SHA-256 `2f3a47f7b3a541ddef450e4867eeecb7ce2ef7776093f3a11d6d43ead6bf2827` |

The source at the v0.72a commit was migrated to the Mbed TLS 4.0 API, despite
the GitHub release text currently mentioning 4.2. Use 4.0.0 for this revision.

## Required compatibility details

- Build Crypto++ x64 Release with v142. The expected output is
  `cryptopp/x64/Release/cryptlib.lib`.
- Copy `id3lib/config.h.win32` to `id3lib/config.h`, change the id3lib project
  include path from `../zlib` to `../zlib/include`, and build x64 Release with
  v142. The expected output is `id3lib/libprj/x64/Release/id3lib.lib`.
- Build ResizableLib configuration `Release Static Unicode|x64` with v145,
  directing its output to `ResizableLib/x64/Release/ResizableLib.lib`.
- Build zlib and miniupnpc as Release x64 static libraries with `/MT`. Copy
  the results to the legacy paths referenced by `srchybrid/emule.vcxproj`:
  `zlib/contrib/vstudio/vc/x64/Release/zlib.lib` and
  `miniupnpc/msvc/x64/Release/miniupnpc.lib`.
- Build Mbed TLS 4.0.0 with programs/tests disabled, `/MT`,
  `MBEDTLS_THREADING_C` and `MBEDTLS_THREADING_ALT`. Add
  `mbedtls/tf-psa-crypto/include/mbedtls` to the C include path so the supplied
  `threading_alt.h` is used. Combine `mbedtls.lib`, `mbedx509.lib` and
  `tfpsacrypto.lib` into
  `mbedtls/visualc/vs2017/x64/Release/mbedtls.lib`.

All dependency libraries must use the same static runtime as eMule. Mixing
`/MD` libraries with eMule's `/MT` build produces unresolved CRT symbols at
link time.

## Build eMule

From a Visual Studio developer shell:

```powershell
MSBuild.exe srchybrid\emule.vcxproj /t:Build `
  /p:Configuration=Release `
  /p:Platform=x64 `
  /p:PlatformToolset=v145 `
  /p:WindowsTargetPlatformVersion=10.0.26100.0 `
  /p:BuildProjectReferences=false `
  /m:1 /nodeReuse:false
```

The resulting executable is `srchybrid/x64/Release/emule.exe`.
